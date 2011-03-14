/*
 * plexusinit.cpp
 *
 *  Created on: Feb 26, 2011
 *      Author: bill
 */

#include "llerror.h"
#include "plexusinit.h"
#include "lua.hpp"
#include "llviewermenu.h"
#include "lldir.h"
#include <boost/asio/buffer.hpp>
#include <vector>
extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
}

#undef luaL_dostring
#define luaL_dostring(L,s)	\
	(luaL_loadstring(L, s) || lua_pcall(L, 0, LUA_MULTRET, 0))

#define UNSET 0
#define SET 1
#define NOT_ALLOWED 2

typedef struct {
	int len;
	int capacity;
	char *data;
} buf;

static buf input = {0, 0, 0};
static buf output = {0, 0, 0};
static int mysocket = -1;
static long totalBytesRead = 0L, totalBytesWritten = 0L;
static int initialized = UNSET;
static int remotePort = -1;
static char *remoteHost = 0;
static std::vector<void (*)()> tickhooks;
static lua_State *L;
static char err[1024];

#define BASE_ERR(args...) sprintf(err, args); LL_INFOS("Plexus") << err << LL_ENDL;

#define ERR(args...) BASE_ERR(args); return false;

static void testLua() {
	LL_INFOS("InitInfo")  << "Test" << LL_ENDL;
}

static void bufInit(buf &b) {
	b.len = 0;
	b.capacity = 1024;
	b.data = (char *)malloc(1024);
}

static void bufReserve(buf &b, int extra) {
	int cap = b.capacity;

	for (; b.capacity < b.len + extra; cap <<= 1);
	if (cap != b.capacity) {
		char *newData = (char *)malloc(cap);

		memcpy(newData, b.data, b.len);
		b.capacity = cap;
		free(b.data);
		b.data = newData;
	}
}

static void bufDelete(buf &b, int amount) {
	memmove(b.data, b.data + amount, b.len - amount);
}

static void bufClear(buf &b) {
	b.len = 0;
}

void addTickHook(void (*hook)()) {
	tickhooks.push_back(hook);
}

static void remotedisconnect(const char *msg) {
	if (mysocket != -1) {
		close(mysocket);
		mysocket = -1;
		LL_INFOS("Plexus") << "Disconnected from remote host: " << (msg ? msg : "") << LL_ENDL;
	}
	bufClear(output);
	bufClear(input);
	perror(msg ? msg : "");
	totalBytesRead = totalBytesWritten = 0L;
}

static void readChunk() {
	bufReserve(input, 1001);
	char *ptr = input.data + input.len;
	int bytesRead = recv(mysocket, ptr, 1000, MSG_DONTWAIT);
	if (bytesRead < 0 && errno != EAGAIN && bytesRead != EAGAIN) {
		remotedisconnect("connection closed while reading");
	} else if (bytesRead > 0) {
		totalBytesRead += bytesRead;
		int lastNl = -1;
		int oldLen = input.len;

		LL_INFOS("Plexus") << "Recieved " << bytesRead << " bytes" << LL_ENDL;
		input.data[oldLen + bytesRead] = 0;
		input.len += bytesRead;
		LL_INFOS("Plexus") << "Recieved msg: " << input.data + oldLen << " bytes" << LL_ENDL;
		for (int i = 0; i < bytesRead; i++) {
			if (ptr[i] == '\n') {
				ptr[i] = ';';
				lastNl = i;
			}
		}
		if (lastNl > -1) {
			lastNl += oldLen;
			input.data[lastNl] = 0;
			if (input.len > 1) {
				char *command = input.data;
				LL_INFOS("Plexus") << "Recieved command: " << command << LL_ENDL;
				luaL_dostring(L, command);
			}
			if (lastNl + 1 < input.len) {
				bufDelete(input, lastNl + 1);
			} else {
				bufClear(input);
			}
		}
	}
}

static void writeChunk() {
	if (output.len) {
		int written = send(mysocket, output.data, output.len, MSG_DONTWAIT);

		if (written == EOF) {
			remotedisconnect("connection closed while writing");
		} else if (written > 0) {
			totalBytesWritten += written;
			if (written < output.len) {
				bufDelete(output, written);
			} else {
				output.len = 0;
			}
		}
	}
}

static void remoteallow(const char *host, int port) {
	if (host == 0) {
		LL_INFOS("Plexus") << "REMOTE ERROR: host is NULL for remote connect" << LL_ENDL;
	} else if (strlen(host) == 0) {
		LL_INFOS("Plexus") << "REMOTE ERROR: host is empty for remote connect" << LL_ENDL;
	} else if (port == 0) {
		LL_INFOS("Plexus") << "REMOTE ERROR: No port given for remote connect" << LL_ENDL;
	} else if (port < 0) {
		LL_INFOS("Plexus") << "REMOTE ERROR: Invalid port for remote connect: " << port << LL_ENDL;
	} else {
		switch (initialized) {
		case SET:
			if (strcmp(host, remoteHost)) {
				LL_INFOS("Plexus") << "REMOTE SECURITY VIOLATION: attempt to change allowed remote host from: " << remoteHost << " to " << host << "." << LL_ENDL;
				break;
			} else if (port != remotePort) {
				LL_INFOS("Plexus") << "REMOTE SECURITY VIOLATION: attempt to change allowed remote port from: " << remotePort << " to " << port << "." << LL_ENDL;
				break;
			}
			break;
		case NOT_ALLOWED:
			LL_INFOS("Plexus") << "REMOTE SECURITY VIOLATION: attempt to allow remote connections when they have been disabled." << LL_ENDL;
			break;
		case UNSET:
			initialized = SET;
			if (remoteHost) {
				free(remoteHost);
			}
			remoteHost = strdup(host);
			remotePort = port;
			LL_INFOS("Plexus") << "Allowing remote connections to " << remoteHost << ":" << remotePort << LL_ENDL;
			break;
		}
	}
}

static bool remoteconnect() {
	if (mysocket != -1) {
		ERR("REMOTE ERROR: attempt to connect when already connected.");
	}
	switch (initialized) {
	case UNSET:
		ERR("REMOTE ERROR: attempt to connect when host and port are not set");
	case NOT_ALLOWED:
		ERR("REMOTE SECURITY VIOLATION: attempt to connect when remote connections have been disabled");
	case SET:
		bool isIPAddr = true;
		struct sockaddr_in addr;

		if (!(mysocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP))) {
			ERR("Couldn't create socket");
		}
	    memset(&addr, 0, sizeof(addr));     /* Zero out structure */
	    addr.sin_family      = AF_INET;             /* Internet address family */
	    addr.sin_port        = htons(remotePort); /* Server port */
        for (char *p = remoteHost; *p; p++) {
        	if ((*p < '0' || '9' < *p) && *p != '.') {
        		isIPAddr = false;
        		break;
        	}
        }
        if (isIPAddr) {
        	LL_INFOS("Plexus") << "USING IP ADDR: " << remoteHost << LL_ENDL;
        	addr.sin_addr.s_addr = inet_addr(remoteHost);   /* Server IP address */
        } else {
        	LL_INFOS("Plexus") << "USING HOST NAME: " << remoteHost << LL_ENDL;
        	hostent *ent = gethostbyname(remoteHost);
        	if (!ent) {
        		ERR("ERROR RESOLVING SERVER %s: %s", remoteHost, hstrerror(h_errno));
        	} else {
        		addr.sin_addr.s_addr = *(in_addr_t *)ent->h_addr_list[0];
        		LL_INFOS("Plexus") << "Using ip address for host name " << remoteHost << ": " << ent->h_addr_list[0][0] << "." << ent->h_addr_list[0][1] << "." << ent->h_addr_list[0][2] << "." << ent->h_addr_list[0][3] << LL_ENDL;
        	}
        }
		LL_INFOS("Plexus") << "attempting to connect to " << remoteHost << LL_ENDL;
		if (connect(mysocket, (const struct sockaddr *)&addr, sizeof addr)) {
			mysocket = -1;
			ERR("REMOTE ERROR: Could not connect to remote host %s:%d", remoteHost, remotePort);
		}
		fcntl(mysocket, O_NONBLOCK, 1);
		LL_INFOS("Plexus") << "Connected to " << remoteHost << ":" << remotePort << LL_ENDL;
		break;
	}
	return true;
}

bool plexus::remotesend(const char *line) {
	if (mysocket != -1) {
		bufReserve(output, strlen(line));
		for (char *p = (char *)line; *p; p++) {
			output.data[output.len++] = *p;
		}
		output.data[output.len++] = '\n';
	} else {
		ERR("Ignoring remote send, because no connection: %s", line);
	}
	return true;
}

void plexus::idle() {
	static long idleCount = 0;

	if (++idleCount % 100 == 0) {
		LL_INFOS("Plexus") << "Idle: " << idleCount << LL_ENDL;
	}
	if (-1 != mysocket) {
		writeChunk();
		readChunk();
	}
	if (tickhooks.size() > 0) {
		for (int i = 0; i < tickhooks.size(); i++) {
			tickhooks[i]();
		}
	}
}

#define LERR(args...) {BASE_ERR(args);\
	luaL_error(L, err);}

struct func {
	const char *name;
	lua_CFunction func;
};

#define INFO(name) __LUA__ ## name
#define FUNC(name) l_ ## name
#define LBEGIN(name, nargs) static int FUNC(name)(lua_State *L); \
static func INFO(name) = {#name, FUNC(name)};\
static int FUNC(name)(lua_State *L) {\
	checkStack(L, nargs, #name);
#define LEND }
#define LFUNC0(name, nargs, def) LBEGIN(name, nargs) def ; return 0; LEND

static void checkStack(lua_State *L, int size, const char *funcName) {
	if (lua_gettop(L) != size) {
		LERR("Wrong number of arguments to %s", funcName);
	}
}

static void logMessage(const char *tag, const char *msg) {
	LL_INFOS(tag) << msg << LL_ENDL;
}

LFUNC0(remoteConnect, 0, remoteconnect())
LFUNC0(remoteAllow, 2, remoteallow(luaL_checkstring(L, 1), luaL_checkinteger(L, 2)))
LFUNC0(remoteSend, 1, plexus::remotesend(luaL_checkstring(L, 1)))
LFUNC0(remoteDisconnect, 1, remotedisconnect(luaL_checkstring(L, 1)))
LFUNC0(log, 2, logMessage(luaL_checkstring(L, 1), luaL_checkstring(L, 2)))

static func luaFuncs[] = {
	INFO(remoteConnect),
	INFO(remoteAllow),
	INFO(remoteSend),
	INFO(remoteDisconnect),
	INFO(log),
	{0, 0}
};

void plexus::init() {
  bufInit(input);
  bufInit(output);
  LL_INFOS("InitInfo") << "INTERNAL INIT PLEXUS" << LL_ENDL;
  LL_INFOS("InitInfo") << "INTERNAL INIT PLEXUS" << LL_ENDL;
  LL_INFOS("InitInfo") << "INTERNAL INIT PLEXUS" << LL_ENDL;

  L = lua_open();
  for (func *fp = luaFuncs; fp->name; fp++) {
	  lua_pushcfunction(L, fp->func);
	  lua_setglobal(L, fp->name);
  }
  // load the libs
  //luaL_openlibs(L);
  luaL_dostring(L, "return 3 + 4");
  luaL_dostring(L, "log('PLEXUS', 'INIT')");
  luaL_dofile(L, gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "init.lua").c_str());
  LL_INFOS("InitInfo") << gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "init.lua") << LL_ENDL;
  LL_INFOS("InitInfo") << "LUA RESULT: " << lua_tointeger(L, -1) << LL_ENDL;
//  lua_close(L);
  LL_INFOS("InitInfo") << "DONE INIT PLEXUS" << LL_ENDL;
  LL_INFOS("InitInfo") << "DONE INIT PLEXUS" << LL_ENDL;
  LL_INFOS("InitInfo") << "DONE INIT PLEXUS" << LL_ENDL;
}

void plexus::init_menus() {
  //view_listener_t::addMenu(new PlexusTestLua(), "TestLua");
  LLUICtrl::CommitCallbackRegistry::Registrar& commit = LLUICtrl::CommitCallbackRegistry::currentRegistrar();
  commit.add("TestLua", boost::bind(&testLua));
}

/*
ICOMMAND(remotestatus, "", (), {
	char buf[512];
	snprintf(buf, sizeof(buf), "hostname %s port %d socket %d read %ld written %ld", remoteHost, remotePort, mysocket, totalBytesRead, totalBytesWritten);
	result(buf);
});

ICOMMAND(remotedisconnect, "s", (char *msg), remotedisconnect(msg));

ICOMMAND(remotedisable, "", (),
	if (initialized == SET) {
		remotedisconnect("remote connections disabled");
		delete[] remoteHost;
		enet_deinitialize();
	}
	initialized = NOT_ALLOWED;
);

ICOMMAND(remoteallow, "si", (char *host, int *port), remoteallow(host, port ? *port : 0););

ICOMMAND(remoteconnect, "", (), remoteconnect(););

ICOMMAND(remotesend, "C", (char *line),
		 tc_remotesend(line);
);

*/
