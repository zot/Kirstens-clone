/*
 * plexusinit.cpp
 *
 *  Created on: Feb 26, 2011
 *      Author: bill
 */

#include "llerror.h"
#include "plexusinit.h"

static void bp() {
  LL_INFOS("InitInfo")  << "BEEP\n" << LL_ENDL;
}

void plexusbeep() {
  bp();
}

void plexus::init() {
  LL_INFOS("InitInfo") << "INTERNAL INIT PLEXUS" << LL_ENDL;
  LL_INFOS("InitInfo") << "INTERNAL INIT PLEXUS" << LL_ENDL;
  LL_INFOS("InitInfo") << "INTERNAL INIT PLEXUS" << LL_ENDL;
}
