/*
 * plexusinit.h
 *
 *  Created on: Feb 26, 2011
 *      Author: bill
 */

#ifndef PLEXUSINIT_H_
#define PLEXUSINIT_H_

namespace plexus {
  void init();
  void init_menus();
  void idle();
  bool remotesend(const char *line);
}

#endif /* PLEXUSINIT_H_ */
