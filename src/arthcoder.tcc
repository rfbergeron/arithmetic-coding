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

template <class TBuffer, class charT>
std::map<charT, symbol_range<TBuffer>> build_table(
    std::basic_istream<charT> &is) {
  std::map<charT, symbol_range<TBuffer>> symbols;

  charT current;
  while (is.get(current)) {
    ++symbols[current].occurrences;
  }

  TBuffer cumulative_lower_bound = 0;
  for (auto &x : symbols) {
    x.second.lower = cumulative_lower_bound;
    x.second.upper = cumulative_lower_bound =
        x.second.lower + x.second.occurrences;
  }

  return symbols;
}

template <class TBuffer, class charT>
void write_table(std::ostream &os,
                 const std::map<charT, symbol_range<TBuffer>> &symbols) {
  std::cerr << std::showbase << std::hex << std::internal << std::setfill('0');
  DEBUGF('t', "WRITING TABLE");
  // write magic number to stream
  std::string magic{0x1B, 't', 'a', 'b',
                    static_cast<char>(std::numeric_limits<TBuffer>::digits)};
  os.write(magic.data(), 5);

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
  const charT ZERO_CHART = 0;
  const TBuffer ZERO_TBUFFER = 0;
  os.write(reinterpret_cast<const char *>(&ZERO_CHART), sizeof(charT));
  os.write(reinterpret_cast<const char *>(&ZERO_TBUFFER), sizeof(TBuffer));
  DEBUGF('t',
         "PLAINTEXT LENGTH: " << std::dec << symbols.rbegin()->second.upper);
}

template <class TBuffer, class charT>
void compress_stream(std::basic_istream<charT> &is, std::ostream &os,
                     const std::map<charT, symbol_range<TBuffer>> &symbols) {
  TBuffer upper_bound = std::numeric_limits<TBuffer>::max();
  TBuffer lower_bound = 0, buffer = 0;
  int pending_count = 0, buffer_count = 0;
  TBuffer in_size = symbols.rbegin()->second.upper;
  charT lowest_char = symbols.begin()->first;
  assert(symbols.begin()->second.lower == 0);
  const int buffer_bits = std::numeric_limits<TBuffer>::digits;
  TBuffer first_bit = static_cast<TBuffer>(0x1U) << (buffer_bits - 1);
  TBuffer second_bit = first_bit >> 1;
  std::string magic{0x1B, 'd', 'a', 't', static_cast<char>(buffer_bits)};
  os.write(magic.data(), 5);

  // debugging variables
  STUB(int max_bit_count = 0);
  STUB(size_t characters_written = 0);
  STUB(std::cerr << std::showbase << std::hex << std::internal
                 << std::setfill('0'));

  charT current;
  while (is.get(current) ||
         (current = lowest_char, buffer_count > 0 || pending_count > 0)) {
    // generate the range and bounds for selected symbol at this depth in the
    // file
    TBuffer range = upper_bound - lower_bound;
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
    STUB(TBuffer debug_buffer = 0);
    STUB(int bit_count = 0);

    for (;;) {
      if ((upper_bound ^ lower_bound) < first_bit) {
        // first bit matches
        if (buffer_count >= buffer_bits) {
          DEBUGF('b',
                 "        BUFFER FULL; FLUSHING: " << std::setw(10) << buffer);
          const char *casted_buffer = reinterpret_cast<const char *>(&buffer);
          os.write(casted_buffer, sizeof(buffer));
          buffer = buffer_count = 0;
          if (is.eof() && pending_count == 0) break;
        }

        // write msb to the end of the buffer
        TBuffer msb = lower_bound >> (buffer_bits - 1);
        buffer <<= 1;
        buffer |= msb;
        STUB(debug_buffer <<= 1);
        STUB(debug_buffer |= msb);
        ++buffer_count;

        DEBUGF('x', "    MOST SIGNIFICANT BIT: " << std::noshowbase << msb
                                                 << std::showbase);

        // renormalize the ranges
        upper_bound <<= 1;
        upper_bound |= 1;
        lower_bound <<= 1;

        // add pending bits to the buffer, writing to the output stream whenever
        // if gets full
        TBuffer pending_bit = msb ^ static_cast<TBuffer>(0x1U);
        STUB(++bit_count);
        while (pending_count > 0) {
          if (buffer_count >= buffer_bits) {
            DEBUGF('b', "        BUFFER FULL; FLUSHING: " << std::setw(10)
                                                          << buffer);
            const char *casted_buffer = reinterpret_cast<const char *>(&buffer);
            os.write(casted_buffer, sizeof(buffer));
            buffer = buffer_count = 0;
          }
          DEBUGF('p', "    PENDING BIT: " << std::noshowbase << pending_bit
                                          << std::showbase);
          buffer <<= 1;
          buffer |= pending_bit;
          ++buffer_count;
          --pending_count;
          STUB((++bit_count, debug_buffer <<= 1, debug_buffer |= pending_bit));
        }

        if (is.eof() && buffer_count == 0) break;
      } else if (lower_bound >= second_bit &&
                 upper_bound < (first_bit | second_bit)) {
        // pending bits, indicated by the first bits of the upper and lower
        // bounds not matching and the second bits being the inverse of the
        // first bit preserve first bit since it is undecided and discard second
        // one
        DEBUGF('p', "    BIT PENDING");
        lower_bound <<= 1;
        lower_bound &= ~first_bit;
        upper_bound <<= 1;
        upper_bound |= first_bit | static_cast<TBuffer>(0x1U);
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
  os.write(reinterpret_cast<const char *>(&lower_bound), sizeof(lower_bound));
}

template <class TBuffer, class charT>
std::map<charT, symbol_range<TBuffer>> read_table(std::istream &is) {
  std::cerr << std::hex << std::showbase << std::internal << std::setfill('0');
  std::map<charT, symbol_range<TBuffer>> symbols;

  charT symbol;
  TBuffer cumulative_lower_bound = 0;
  DEBUGF('t', "READING TABLE");

  char magic[6];
  magic[5] = 0;
  is.read(magic, 5);
  std::string check{0x1B, 't', 'a', 'b',
                    static_cast<char>(std::numeric_limits<TBuffer>::digits)};
  if (check.compare(magic) != 0) {
    std::cerr << "Error: stream contained invalid table; header does not match "
              << "expected value." << std::endl;
    std::cerr << "Expected value: " << check << " Actual value: " << magic
              << std::endl;
    return symbols;
  }

  while (is.read(reinterpret_cast<char *>(&symbol), sizeof(symbol))) {
    TBuffer occurrences;
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
template <class TBuffer, class charT>
void decompress_stream(std::istream &is, std::basic_ostream<charT> &os,
                       const std::map<charT, symbol_range<TBuffer>> &symbols) {
  std::cerr << std::hex << std::showbase << std::internal << std::setfill('0');
  TBuffer upper_bound = std::numeric_limits<TBuffer>::max();
  TBuffer lower_bound = 0, buffer, encoding;
  size_t characters_written = 0;
  TBuffer out_size = symbols.rbegin()->second.upper;
  int buffer_bits = std::numeric_limits<TBuffer>::digits;
  int buffer_count = 0;
  TBuffer first_bit = static_cast<TBuffer>(0x1U) << (buffer_bits - 1);
  TBuffer second_bit = first_bit >> 1;

  char magic[6];
  magic[5] = 0;
  is.read(magic, 5);
  std::string check{0x1B, 'd', 'a', 't',
                    static_cast<char>(std::numeric_limits<TBuffer>::digits)};
  if (check.compare(magic) != 0) {
    std::cerr << "Error: stream contained invalid table; header does not match "
              << "expected value." << std::endl;
    std::cerr << "Expected value: " << check << " Actual value: " << magic
              << std::endl;
    return;
  }

  STUB(int pending_count = 0);
  // fill encoding
  is.read(reinterpret_cast<char *>(&encoding), sizeof(encoding));
  for (;;) {
    TBuffer range = upper_bound - lower_bound;
    charT current;

    for (auto &x : symbols) {
      // computes what the bounds would be given that x was encoded
      // and sees if the actual encoding falls within them
      TBuffer upper_given_x = lower_bound + (range / out_size * x.second.upper);
      TBuffer lower_given_x = lower_bound + (range / out_size * x.second.lower);

      if (encoding < upper_given_x && encoding >= lower_given_x) {
        current = x.first;
        DEBUGF('z', "SYMBOL " << std::dec << characters_written << std::hex
                              << ": " << std::setw(4) << (current & 0xFFU)
                              << "; RANGE: [" << std::setw(10) << lower_given_x
                              << ", " << std::setw(10) << upper_given_x
                              << "); ENCODING: " << std::setw(10) << encoding);
        break;
      }
    }

    os.write(&current, 1);
    ++characters_written;
    if (characters_written >= out_size) return;

    upper_bound = lower_bound + (range / out_size * symbols.at(current).upper);
    lower_bound += range / out_size * symbols.at(current).lower;

    // debug variables
    STUB(TBuffer debug_buffer = 0);
    STUB(int bit_count = 0);
    // remove matching or pending bits
    for (;;) {
      // refreshes buffer if all bits have been read
      if (buffer_count == 0) {
        is.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
        buffer_count = buffer_bits;
        if (is.eof()) buffer = second_bit;
        STUB(
            if (is.eof()) {
              DEBUGF('b', "ENCODING END; FILLING BUFFER WITH LOWER BOUND: "
                              << buffer);
            } else {
              DEBUGF('b', "BUFFER EMPTY; READING: " << std::setw(10) << buffer);
            });
      }

      if ((upper_bound ^ lower_bound) < first_bit) {
        // first bit matches
        STUB(TBuffer msb = lower_bound >> (buffer_bits - 1));
        DEBUGF('x', "    MOST SIGNIFICANT BIT: " << std::noshowbase << msb
                                                 << std::showbase);
        lower_bound <<= 1;
        upper_bound <<= 1;
        upper_bound |= static_cast<TBuffer>(0x1U);
        encoding <<= 1;
        TBuffer current_bit = (buffer & first_bit) >> (buffer_bits - 1);
        buffer <<= 1;
        encoding |= current_bit;
        --buffer_count;
        STUB((++bit_count, debug_buffer = (debug_buffer << 1) | msb));
        STUB(while (pending_count > 0) {
          DEBUGF('p', "    PENDING BIT: " << std::noshowbase
                                          << (msb ^ static_cast<TBuffer>(0x1U))
                                          << std::showbase);
          debug_buffer =
              (debug_buffer << 1) | (msb ^ static_cast<TBuffer>(0x1U));
          --pending_count;
          ++bit_count;
        });
      } else if (lower_bound >= second_bit &&
                 upper_bound < (first_bit | second_bit)) {
        // pending bits
        DEBUGF('p', "    BIT PENDING");
        lower_bound <<= 1;
        lower_bound &= ~first_bit;
        upper_bound <<= 1;
        upper_bound |= first_bit | static_cast<TBuffer>(0x1U);
        TBuffer encoding_msb = encoding & first_bit;
        encoding <<= 1;
        encoding &= ~first_bit;
        encoding |= encoding_msb;
        TBuffer current_bit = (buffer & first_bit) >> (buffer_bits - 1);
        buffer <<= 1;
        encoding |= current_bit;
        --buffer_count;
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
