// John Gnanasekaran (jgnanase) and Robert Bergeron (rbergero)
// $Id: debug.cpp,v 1.2 2018-01-25 14:12:59-08 - - $

#include <climits>
#include <iostream>
#include <vector>

using namespace std;

#include "debug.h"

debugflags::flagset debugflags::flags {};

void debugflags::setflags (const string& initflags) {
   cerr << "setting flag "  << initflags << endl;
   for (const unsigned char flag: initflags) {
      if (flag == '@') flags.set();
                  else flags.set (flag, true);
   }
}

// getflag -
//    Check to see if a certain flag is on.

bool debugflags::getflag (char flag) {
   // WARNING: Don't TRACE this function or the stack will blow up.
   return flags.test (static_cast<unsigned char> (flag));
}
