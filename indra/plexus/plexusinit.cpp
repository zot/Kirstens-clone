/*
 * plexusinit.cpp
 *
 *  Created on: Feb 26, 2011
 *      Author: bill
 */

#include "llerror.h"
#include "plexusinit.h"
#include "lua.hpp"
//#include "llviewerprecompiledheaders.h"
#include "llviewermenu.h"

#undef luaL_dostring
#define luaL_dostring(L,s)	\
	(luaL_loadstring(L, s) || lua_pcall(L, 0, LUA_MULTRET, 0))

static void bp() {
	  LL_INFOS("InitInfo")  << "BEEP\n" << LL_ENDL;
}

void plexusbeep() {
  bp();
}

class PlexusTestLua : public view_listener_t {
	bool handleEvent(const LLSD& userdata) {
		LL_INFOS("InitInfo")  << "Test" << LL_ENDL;
		return true;
	}
};

static void testLua() {
	LL_INFOS("InitInfo")  << "Test" << LL_ENDL;
}


void plexus::init() {
  LL_INFOS("InitInfo") << "INTERNAL INIT PLEXUS" << LL_ENDL;
  LL_INFOS("InitInfo") << "INTERNAL INIT PLEXUS" << LL_ENDL;
  LL_INFOS("InitInfo") << "INTERNAL INIT PLEXUS" << LL_ENDL;

  lua_State *L = lua_open();
  // load the libs
  //luaL_openlibs(L);
  luaL_dostring(L, "return 3 + 4");
  LL_INFOS("InitInfo") << "LUA RESULT: " << lua_tointeger(L, -1) << LL_ENDL;
  lua_close(L);
  LL_INFOS("InitInfo") << "DONE INIT PLEXUS" << LL_ENDL;
  LL_INFOS("InitInfo") << "DONE INIT PLEXUS" << LL_ENDL;
  LL_INFOS("InitInfo") << "DONE INIT PLEXUS" << LL_ENDL;
}

void plexus::init_menus() {
  //view_listener_t::addMenu(new PlexusTestLua(), "TestLua");
  LLUICtrl::CommitCallbackRegistry::Registrar& commit = LLUICtrl::CommitCallbackRegistry::currentRegistrar();
  commit.add("TestLua", boost::bind(&testLua));
}
