#include <iostream>
#include <map>
#ifndef __ARTHCODER_H__
#define __ARTHCODER_H__

template <class TBuffer = uintmax_t>
struct symbol_range {
  TBuffer occurrences = 0;
  TBuffer upper = 0;
  TBuffer lower = 0;
};

template <class TBuffer = uintmax_t, class charT>
std::map<charT, symbol_range<TBuffer>> build_table(
    std::basic_istream<charT> &is);

template <class TBuffer, class charT>
void write_table(std::ostream &os,
                 const std::map<charT, symbol_range<TBuffer>> &symbols);

template <class TBuffer, class charT>
void compress_stream(std::basic_istream<charT> &is, std::ostream &os,
                     const std::map<charT, symbol_range<TBuffer>> &symbols);

template <class TBuffer = uintmax_t, class charT>
std::map<charT, symbol_range<TBuffer>> read_table(std::istream &is);

template <class TBuffer, class charT>
void decompress_stream(std::istream &is, std::basic_ostream<charT> &os,
                       const std::map<charT, symbol_range<TBuffer>> &symbols);
#include "arthcoder.tcc"
#endif
