#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <string>

#include "arthcoder.h"
#include "debug.h"

template <typename bufferT, typename charT>
std::map<charT, symbol_range<bufferT>> build_table(
    std::basic_istream<charT> &is) {
  std::map<charT, symbol_range<bufferT>> symbols;

  charT current;
  while (is.get(current)) ++symbols[current].occurrences;

  bufferT cumulative_lower_bound = 0;
  for (auto &x : symbols) {
    x.second.lower = cumulative_lower_bound;
    x.second.upper = cumulative_lower_bound =
        x.second.lower + x.second.occurrences;
  }

  return symbols;
}

template <typename bufferT, typename charT>
void write_table(std::ostream &os,
                 const std::map<charT, symbol_range<bufferT>> &symbols) {
  DEBUGF('t', "WRITING TABLE");
  std::string magic{0x1B, 't', 'a', 'b',
                    static_cast<char>(std::numeric_limits<bufferT>::digits)};
  os.write(magic.data(), 5);

  STUB(std::cerr << std::showbase << std::hex << std::internal
                 << std::setfill('0'));
  for (const auto &x : symbols) {
    os.write(reinterpret_cast<const char *>(&x.first), sizeof(x.first));
    os.write(reinterpret_cast<const char *>(&x.second.occurrences),
             sizeof(x.second.occurrences));
    DEBUGF('t', "    SYMBOL: " << std::setw(4) << (x.first & 0xFFU)
                               << "; UNADJUSTED BOUNDS: [" << std::setw(10)
                               << x.second.lower << ", " << std::setw(10)
                               << x.second.upper << ")");
  }

  // writes a NUL character entry with 0 occurrences to indicate that
  // the table has ended
  const charT ZERO_CHAR_T = 0;
  const bufferT ZERO_BUFFER_T = 0;
  os.write(reinterpret_cast<const char *>(&ZERO_CHAR_T), sizeof(charT));
  os.write(reinterpret_cast<const char *>(&ZERO_BUFFER_T), sizeof(bufferT));
  DEBUGF('t',
         "PLAINTEXT LENGTH: " << std::dec << symbols.rbegin()->second.upper);
}

template <typename bufferT, typename charT>
void compress_stream(std::basic_istream<charT> &is, std::ostream &os,
                     const std::map<charT, symbol_range<bufferT>> &symbols) {
  assert(symbols.begin()->second.lower == 0);

  constexpr int buffer_bits = std::numeric_limits<bufferT>::digits;
  std::string magic{0x1B, 'd', 'a', 't', static_cast<char>(buffer_bits)};
  os.write(magic.data(), 5);

  STUB(int max_bit_count = 0);
  STUB(bufferT characters_written = 0);
  STUB(std::cerr << std::showbase << std::hex << std::internal
                 << std::setfill('0'));

  constexpr bufferT first_bit = static_cast<bufferT>(0x1U) << (buffer_bits - 1);
  constexpr bufferT second_bit = first_bit >> 1;
  int buffer_count = 0, pending_count = 0;
  bufferT upper_bound = std::numeric_limits<bufferT>::max();
  bufferT lower_bound = 0, buffer = 0, in_size = symbols.rbegin()->second.upper;
  charT current, lowest_char = symbols.begin()->first;
  while (is.get(current) ||
         (current = lowest_char, buffer_count > 0 || pending_count > 0)) {
    // generate the range and bounds for selected symbol at this depth in the
    // file
    bufferT range = upper_bound - lower_bound;
    upper_bound = lower_bound + (range / in_size * symbols.at(current).upper);
    lower_bound += range / in_size * symbols.at(current).lower;
    STUB(
        if (is.eof() && buffer_count > 0) {
          DEBUGF('z', "LEFTOVER BUFFER; PADDING WITH SYMBOL: "
                          << std::setw(4) << (current & 0xFFU) << "; RANGE: ["
                          << std::setw(10) << lower_bound << ", "
                          << std::setw(10) << upper_bound
                          << "); BITS UNTIL FULL: " << std::noshowbase
                          << std::dec << buffer_bits - buffer_count
                          << std::showbase << std::hex);
        } else if (is.eof() && pending_count > 0) {
          DEBUGF('z', "LEFTOVER PENDING; PADDING WITH SYMBOL: "
                          << std::setw(4) << (current & 0xFFU) << "; RANGE: ["
                          << std::setw(10) << lower_bound << ", "
                          << std::setw(10) << upper_bound << ")");
        } else if (is.eof()) { abort(); } else {
          DEBUGF('z', "SYMBOL " << std::dec << characters_written << ": "
                                << std::hex << std::setw(4) << (current & 0xFFU)
                                << "; RANGE: [" << std::setw(10) << lower_bound
                                << ", " << std::setw(10) << upper_bound << ")");
        });

    // debugging variables
    STUB(bufferT debug_buffer = 0);
    STUB(int bit_count = 0);

    for (;;) {
      if ((upper_bound ^ lower_bound) < first_bit) {
        // first bit matches
        if (buffer_count >= buffer_bits) {
          DEBUGF('b',
                 "        BUFFER FULL; FLUSHING: " << std::setw(10) << buffer);
          os.write(reinterpret_cast<const char *>(&buffer), sizeof(buffer));
          buffer = buffer_count = 0;
          if (is.eof() && pending_count == 0) break;
        }

        // write msb to the end of the buffer
        bufferT msb = lower_bound >> (buffer_bits - 1);
        buffer <<= 1;
        buffer |= msb;
        STUB(debug_buffer <<= 1);
        STUB(debug_buffer |= msb);
        ++buffer_count;

        DEBUGF('x', "    MOST SIGNIFICANT BIT: " << std::noshowbase << msb
                                                 << std::showbase);

        // renormalize the ranges
        upper_bound = upper_bound << 1 | static_cast<bufferT>(0x1U);
        lower_bound <<= 1;

        // add pending bits to the buffer, writing to the output stream whenever
        // if gets full
        bufferT pending_bit = msb ^ static_cast<bufferT>(0x1U);
        STUB(++bit_count);
        while (pending_count > 0) {
          if (buffer_count >= buffer_bits) {
            DEBUGF('b', "        BUFFER FULL; FLUSHING: " << std::setw(10)
                                                          << buffer);
            os.write(reinterpret_cast<const char *>(&buffer), sizeof(buffer));
            buffer = buffer_count = 0;
          }
          DEBUGF('p', "    PENDING BIT: " << std::noshowbase << pending_bit
                                          << std::showbase);
          buffer = (buffer << 1) | pending_bit;
          ++buffer_count;
          --pending_count;
          STUB((++bit_count, debug_buffer = (debug_buffer << 1) | pending_bit));
        }

        if (is.eof() && buffer_count == 0) break;
      } else if (lower_bound >= second_bit &&
                 upper_bound < (first_bit | second_bit)) {
        // pending bits, indicated by the first bits of the upper and lower
        // bounds not matching and the second bits being the inverse of the
        // first bit preserve first bit since it is undecided and discard second
        // one
        DEBUGF('p', "    BIT PENDING");
        lower_bound = lower_bound << 1 & ~first_bit;
        upper_bound = upper_bound << 1 | first_bit | static_cast<bufferT>(0x1U);
        ++pending_count;
      } else {
        DEBUGF('z', "BITS DISCARDED: " << std::dec << std::setw(4) << bit_count
                                       << std::hex << "; VALUE: "
                                       << std::setw(10) << debug_buffer);
        DEBUGF('e',
               "ENCODING COMPLETED:"
                   << std::endl
                   << "    CHARACTER: " << std::setw(4) << (current & 0xFFU)
                   << std::endl
                   << "    ENCODING: " << std::setw(10)
                   << (bit_count > 0 ? debug_buffer << (buffer_bits - bit_count)
                                     : debug_buffer)
                   << std::endl
                   << "    LENGTH: " << std::dec << bit_count << std::endl
                   << "    PENDING: " << pending_count << std::hex);
        STUB(max_bit_count = std::max(max_bit_count, bit_count));
        STUB(++characters_written);
        break;
      }
    }
  }

  // write the lower bound; it may be needed to decode the last character
  DEBUGF('b',
         "DATA END; WRITING LOWER BOUND: " << std::setw(10) << lower_bound);
  DEBUGF('e', "ENCODING COMPLETE");
  os.write(reinterpret_cast<const char *>(&lower_bound), sizeof(lower_bound));
}

template <typename bufferT, typename charT>
std::map<charT, symbol_range<bufferT>> read_table(std::istream &is) {
  DEBUGF('t', "READING TABLE");
  char magic[6];
  magic[5] = 0;
  is.read(magic, 5);
  std::string check{0x1B, 't', 'a', 'b',
                    static_cast<char>(std::numeric_limits<bufferT>::digits)};
  if (check.compare(magic) != 0) {
    std::cerr << "Error: stream contained invalid table; header does not match "
              << "expected value." << std::endl;
    std::cerr << "Expected value: " << check << " Actual value: " << magic
              << std::endl;
    return {};
  }

  STUB(std::cerr << std::hex << std::showbase << std::internal
                 << std::setfill('0'));
  std::map<charT, symbol_range<bufferT>> symbols;
  charT symbol;
  bufferT cumulative_lower_bound = 0;
  while (is.read(reinterpret_cast<char *>(&symbol), sizeof(symbol))) {
    bufferT occurrences;
    is.read(reinterpret_cast<char *>(&occurrences), sizeof(occurrences));
    if (occurrences == 0) break;
    DEBUGF('t', "    SYMBOL: " << std::setw(4) << (symbol & 0xFFU)
                               << "; UNADJUSTED BOUNDS: [" << std::setw(10)
                               << cumulative_lower_bound << ", "
                               << std::setw(10)
                               << cumulative_lower_bound + occurrences << ")");

    symbols[symbol] = {occurrences, cumulative_lower_bound + occurrences,
                       cumulative_lower_bound};
    cumulative_lower_bound += occurrences;
  }

  DEBUGF('t',
         "PLAINTEXT LENGTH: " << std::dec << symbols.rbegin()->second.upper);
  return symbols;
}

// note: unlike `compress_stream`, the `buffer` variable does not represent the
// current set of encoding bits being processed; rather, it is a secondary
// buffer that holds additional bits that are shifted in the primary buffer, the
// `encoding` variable, so that whenever the table is scanned, `encoding` is
// full, which is needed to get accurate comparisons.
template <typename bufferT, typename charT>
void decompress_stream(std::istream &is, std::basic_ostream<charT> &os,
                       const std::map<charT, symbol_range<bufferT>> &symbols) {
  constexpr int buffer_bits = std::numeric_limits<bufferT>::digits;
  char magic[6];
  magic[5] = 0;
  is.read(magic, 5);
  std::string check{0x1B, 'd', 'a', 't', static_cast<char>(buffer_bits)};
  if (check.compare(magic) != 0) {
    std::cerr << "Error: stream contained invalid table; header does not match "
              << "expected value." << std::endl;
    std::cerr << "Expected value: " << check << " Actual value: " << magic
              << std::endl;
    return;
  }

  STUB(std::cerr << std::hex << std::showbase << std::internal
                 << std::setfill('0'));
  STUB(int pending_count = 0);
  bufferT upper_bound = std::numeric_limits<bufferT>::max();
  bufferT lower_bound = 0, characters_written = 0, buffer, aux_buffer;
  bufferT out_size = symbols.rbegin()->second.upper;
  int aux_buffer_count = 0;
  constexpr bufferT first_bit = static_cast<bufferT>(0x1U) << (buffer_bits - 1);
  constexpr bufferT second_bit = first_bit >> 1;
  is.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
  for (;;) {
    bufferT range = upper_bound - lower_bound;
    charT current;

    for (auto &x : symbols) {
      // computes what the bounds would be given that x was encoded
      // and sees if the actual encoding falls within them
      bufferT upper_given_x = lower_bound + (range / out_size * x.second.upper);
      bufferT lower_given_x = lower_bound + (range / out_size * x.second.lower);

      if (buffer < upper_given_x && buffer >= lower_given_x) {
        current = x.first;
        DEBUGF('z', "SYMBOL " << std::dec << characters_written << std::hex
                              << ": " << std::setw(4) << (current & 0xFFU)
                              << "; RANGE: [" << std::setw(10) << lower_given_x
                              << ", " << std::setw(10) << upper_given_x
                              << "); ENCODING: " << std::setw(10) << buffer);
        break;
      }
    }

    os.write(&current, 1);
    ++characters_written;
    if (characters_written >= out_size) {
      DEBUGF('e', "DECODING COMPLETE");
      return;
    }

    upper_bound = lower_bound + (range / out_size * symbols.at(current).upper);
    lower_bound += range / out_size * symbols.at(current).lower;

    // debug variables
    STUB(bufferT debug_buffer = 0);
    STUB(int bit_count = 0);
    // remove matching or pending bits
    for (;;) {
      // refreshes buffer if all bits have been read
      if (aux_buffer_count == 0) {
        is.read(reinterpret_cast<char *>(&aux_buffer), sizeof(aux_buffer));
        aux_buffer_count = buffer_bits;
        if (is.eof()) {
          std::cerr << "Data ended prematurely; unable to decode.\n";
          return;
        }
        DEBUGF('b',
               "AUX BUFFER EMPTY; READING: " << std::setw(10) << aux_buffer);
      }

      if ((upper_bound ^ lower_bound) < first_bit) {
        // first bit matches
        STUB(bufferT msb = lower_bound >> (buffer_bits - 1));
        DEBUGF('x', "    MOST SIGNIFICANT BIT: " << std::noshowbase << msb
                                                 << std::showbase);
        lower_bound <<= 1;
        upper_bound = upper_bound << 1 | static_cast<bufferT>(0x1U);
        bufferT next_bit = aux_buffer >> (buffer_bits - 1);
        aux_buffer <<= 1;
        --aux_buffer_count;
        buffer = buffer << 1 | next_bit;
        STUB((++bit_count, debug_buffer = debug_buffer << 1 | msb));
        STUB(bufferT pending_bit = msb ^ static_cast<bufferT>(0x1U));
        STUB(while (pending_count > 0) {
          DEBUGF('p', "    PENDING BIT: " << std::noshowbase << pending_bit
                                          << std::showbase);
          debug_buffer = debug_buffer << 1 | pending_bit;
          --pending_count;
          ++bit_count;
        });
      } else if (lower_bound >= second_bit &&
                 upper_bound < (first_bit | second_bit)) {
        // pending bits
        DEBUGF('p', "    BIT PENDING");
        lower_bound = lower_bound << 1 & ~first_bit;
        upper_bound = upper_bound << 1 | first_bit | static_cast<bufferT>(0x1U);
        bufferT next_bit = aux_buffer >> (buffer_bits - 1);
        aux_buffer <<= 1;
        --aux_buffer_count;
        buffer = (buffer << 1 & ~first_bit) | (buffer & first_bit) | next_bit;
        STUB(++pending_count);
      } else {
        DEBUGF('z', "BITS DISCARDED: " << std::dec << std::setw(4) << bit_count
                                       << std::hex << "; VALUE: "
                                       << std::setw(10) << debug_buffer);
        DEBUGF('e',
               "DECODING COMPLETED:"
                   << std::endl
                   << "    CHARACTER: " << std::setw(4) << (current & 0xFFU)
                   << std::endl
                   << "    ENCODING: " << std::setw(10)
                   << (bit_count > 0 ? debug_buffer << (buffer_bits - bit_count)
                                     : debug_buffer)
                   << std::endl
                   << "    LENGTH: " << std::dec << bit_count << std::endl
                   << "    PENDING: " << pending_count << std::hex);
        break;
      }
    }
  }
}
