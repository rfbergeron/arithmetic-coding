#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <bitset>
#include <limits>
#include <string>

// debug -
//    static class for maintaining global debug flags, each indicated
//    by a single character.
// setflags -
//    Takes a string argument, and sets a flag for each char in the
//    string.  As a special case, '@', sets all flags.
// getflag -
//    Used by the DEBUGF macro to check to see if a flag has been set.
//    Not to be called by user code.

class debugflags {
 private:
  using flagset = std::bitset<std::numeric_limits<char>::max() + 1>;
  static flagset flags;

 public:
  static void setflags(const std::string& optflags);
  static bool getflag(char flag);
};

// DEBUGF -
//    Macro which expands into debug code.  First argument is a
//    debug flag char, second argument is output code that can
//    be sandwiched between <<.  Beware of operator precedence.
//    Example:
//       DEBUGF ('u', "foo = " << foo);
//    will print two words and a newline if flag 'u' is  on.
//    Traces are preceded by filename, line number, and function.

#ifdef NDEBUG
#define DEBUGF(FLAG, CODE) ;
#define DEBUGS(FLAG, STMT) ;
#define STUB(STMT) ;
#else
#define DEBUGF(FLAG, CODE)            \
  {                                   \
    if (debugflags::getflag(FLAG)) {  \
      std::cerr << CODE << std::endl; \
    }                                 \
  }
#define DEBUGS(FLAG, STMT)           \
  {                                  \
    if (debugflags::getflag(FLAG)) { \
      STMT;                          \
    }                                \
  }
#define STUB(STMT) \
  STMT;
#endif

#endif
