#include <iostream>
#ifndef __ARTHCODER_H__
#define __ARTHCODER_H__

template <class TBuffer = uintmax_t>
struct symbol_range {
  TBuffer occurrences = 0;
  TBuffer upper = 0;
  TBuffer lower = 0;
};

template <class TBuffer = uintmax_t, class charT>
void compress_stream(std::basic_istream<charT> &is, std::ostream &os);

template <class TBuffer = uintmax_t, class charT>
void decompress_stream(std::istream &is, std::basic_ostream<charT> &os);
#include "arthcoder.tcc"
#endif
