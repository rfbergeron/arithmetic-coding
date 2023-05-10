#include "debug.h"

#include <iostream>
#include <vector>

debugflags::flagset debugflags::flags{};

void debugflags::setflags(const std::string& initflags) {
  for (const unsigned char flag : initflags) {
    if (flag == '@')
      flags.set();
    else
      flags.set(flag, true);
  }
}

bool debugflags::getflag(char flag) {
  // WARNING: Don't TRACE this function or the stack will blow up.
  return flags.test(static_cast<unsigned char>(flag));
}
