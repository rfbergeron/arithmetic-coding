#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>

#include "arthcoder.h"
#include "debug.h"

template <class TBuffer, class charT>
void compress_stream(std::basic_istream<charT> &is, std::ostream &os) {
  std::map<charT, symbol_range<TBuffer>> symbols;

  charT current;
  while (is.get(current)) {
    ++symbols[current].occurrences;
  }

  // builds symbol table and writes it to outfile as it constructs it
  TBuffer cumulative_lower_bound = 0;
  std::cerr << std::showbase << std::hex << std::internal << std::setfill('0');
  DEBUGF('t', "WRITING TABLE");
  for (auto &x : symbols) {
    x.second.lower = cumulative_lower_bound;
    x.second.upper = cumulative_lower_bound =
        x.second.lower + x.second.occurrences;

    const char *casted_charT = reinterpret_cast<const char *>(&x.first);
    os.write(casted_charT, sizeof(x.first));
    const char *casted_TBuffer =
        reinterpret_cast<const char *>(&x.second.occurrences);
    os.write(casted_TBuffer, sizeof(x.second.occurrences));
    DEBUGF('t', "    SYMBOL: " << std::setw(4) << (x.first & 0xFFU)
                               << "; UNADJUSTED BOUNDS: [" << std::setw(10)
                               << x.second.lower << ", " << std::setw(10)
                               << x.second.upper << ")");
  }

  // writes a NUL character entry with 0 occurrences to denote that
  // the table has ended
  const TBuffer ZERO = 0;
  const char *CASTED_ZERO = reinterpret_cast<const char *>(&ZERO);
  os.write(CASTED_ZERO, 1);
  os.write(CASTED_ZERO, sizeof(ZERO));

  TBuffer upper_bound = std::numeric_limits<TBuffer>::max();
  TBuffer lower_bound = 0, buffer = 0;
  int pending_count = 0, buffer_count = 0, in_size = cumulative_lower_bound;
  const int buffer_bits = std::numeric_limits<TBuffer>::digits;
  TBuffer first_bit = static_cast<TBuffer>(0x1U) << (buffer_bits - 1);
  TBuffer second_bit = first_bit >> 1;

  // debugging variables
  STUB(int max_bit_count = 0);

  is.clear();
  is.seekg(0, is.beg);

  while (is.get(current)) {
    // generate the range and bounds for selected symbol at this depth in the
    // file
    TBuffer range = upper_bound - lower_bound;
    upper_bound = lower_bound + (range / in_size * symbols[current].upper);
    lower_bound += range / in_size * symbols[current].lower;

    DEBUGF('z', "NEXT SYMBOL: " << std::setw(4) << (current & 0xFFU)
                                << "; RANGE: [" << std::setw(10) << lower_bound
                                << ", " << std::setw(10) << upper_bound << ")");

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
        STUB(if (pending_count > 0) {
          DEBUGF('p', std::noshowbase
                          << std::dec << std::setfill(' ') << "    FLUSHING "
                          << std::setw(2) << pending_count
                          << " PENDING BITS WITH VALUE " << pending_bit
                          << std::showbase << std::hex << std::setfill('0'));
        } ++bit_count;);
        while (pending_count > 0) {
          if (buffer_count >= buffer_bits) {
            DEBUGF('b', "        BUFFER FULL; FLUSHING: " << std::setw(10)
                                                          << buffer);
            const char *casted_buffer = reinterpret_cast<const char *>(&buffer);
            os.write(casted_buffer, sizeof(buffer));
            buffer = buffer_count = 0;
          }
          buffer <<= 1;
          buffer |= pending_bit;
          ++buffer_count;
          --pending_count;
          STUB(++bit_count; debug_buffer <<= 1; debug_buffer |= pending_bit;);
        }
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
        DEBUGF('z', "BITS DISCARDED: " << std::dec << std::setw(2) << bit_count
                                       << std::hex << "; VALUE: "
                                       << std::setw(10) << debug_buffer);
        DEBUGF('e', "ENCODING COMPLETED:"
                        << std::endl
                        << "    CHARACTER: " << std::setw(4)
                        << (current & 0xFFU) << std::endl
                        << "    ENCODING: " << std::setw(10)
                        << (debug_buffer << (buffer_bits - bit_count))
                        << std::endl
                        << "    LENGTH: " << std::dec << bit_count << std::endl
                        << "    PENDING: " << pending_count << std::hex);
        STUB(max_bit_count = std::max(max_bit_count, bit_count));
        break;
      }
    }
  }

  // fill out the buffer with the high order bits of the lower bound, then write
  // the rest of the bits of the lower bound shifted as far left as possible
  if (buffer_count > 0) {
    int bits_needed = buffer_bits - buffer_count;
    DEBUGF('b', std::noshowbase << std::dec << "PADDING BUFFER WITH "
                                << bits_needed << " BITS" << std::showbase
                                << std::hex);
    buffer <<= bits_needed;
    buffer |= lower_bound >> buffer_count;
    lower_bound <<= bits_needed;
    DEBUGF('b', "FINAL BUFFER: " << std::setw(10) << buffer);
    DEBUGF('b', "FINAL LOWER BOUND: " << std::setw(10) << lower_bound);
    const char *casted_buffer = reinterpret_cast<const char *>(&buffer);
    os.write(casted_buffer, sizeof(buffer));
    const char *casted_lower_bound =
        reinterpret_cast<const char *>(&lower_bound);
    os.write(casted_lower_bound, sizeof(lower_bound));
  }
}

template <class TBuffer, class charT>
void decompress_stream(std::istream &is, std::basic_ostream<charT> &os) {
  std::map<charT, symbol_range<TBuffer>> symbols;
  TBuffer occurrences;
  charT symbol;

  // also serves as the total number of characters in the original file
  TBuffer cumulative_lower_bound = 0;
  std::cerr << std::hex << std::showbase << std::internal << std::setfill('0');
  DEBUGF('t', "READING TABLE");
  while (is.read(reinterpret_cast<char *>(&symbol), sizeof(symbol))) {
    is.read(reinterpret_cast<char *>(&occurrences), sizeof(TBuffer));

    // table has ended
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

  DEBUGF('y', "Total characters: " << std::dec << cumulative_lower_bound
                                   << std::hex);

  TBuffer upper_bound = std::numeric_limits<TBuffer>::max();
  TBuffer lower_bound = 0, characters_written = 0, buffer, encoding;
  TBuffer out_size = cumulative_lower_bound;
  int pending_count = 0, buffer_count = 0;
  int buffer_bits = std::numeric_limits<TBuffer>::digits;
  TBuffer first_bit = static_cast<TBuffer>(0x1U) << (buffer_bits - 1);
  TBuffer second_bit = first_bit >> 1;

  // fill encoding and buffer
  is.read(reinterpret_cast<char *>(&encoding), sizeof(encoding));
  is.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
  for (;;) {
    TBuffer range = upper_bound - lower_bound;
    charT current;

    DEBUGF('c', "SCANNING FOR SYMBOLS THAT COULD HAVE BEEN ENCODED WITH "
                    << std::setw(10) << encoding);
    for (auto &x : symbols) {
      // computes what the bounds would be given that x was encoded
      // and sees if the actual encoding falls within them
      TBuffer upper_given_x = lower_bound + (range / out_size * x.second.upper);
      TBuffer lower_given_x = lower_bound + (range / out_size * x.second.lower);

      if (encoding < upper_given_x && encoding >= lower_given_x) {
        current = x.first;
        --x.second.occurrences;
        DEBUGF('c', "    CHARACTER " << std::setw(4) << (current & 0xFFU)
                                     << " BOUNDS THE ENCODING");
      }
    }

    os.write(&current, 1);
    ++characters_written;
    if (characters_written >= out_size) return;

    upper_bound = lower_bound + (range / out_size * symbols[current].upper);
    lower_bound += range / out_size * symbols[current].lower;

    DEBUGF('z', "NEXT SYMBOL: " << std::setw(4) << (current & 0xFFU)
                                << "; RANGE: [" << std::setw(10) << lower_bound
                                << ", " << std::setw(10) << upper_bound << ")");

    // debug variables
    STUB(TBuffer debug_buffer = 0);
    STUB(int bit_count = 0);
    // remove matching or pending bits
    for (;;) {
      if ((upper_bound ^ lower_bound) < first_bit) {
        // first bit matches
        TBuffer msb = lower_bound >> (buffer_bits - 1);
        lower_bound <<= 1;
        upper_bound <<= 1;
        upper_bound |= static_cast<TBuffer>(0x1U);
        encoding <<= 1;
        TBuffer current_bit = (buffer & first_bit) >> (buffer_bits - 1);
        buffer <<= 1;
        encoding |= current_bit;
        ++buffer_count;
        STUB(++bit_count; debug_buffer <<= 1; debug_buffer |= msb;
             while (pending_count > 0) {
               debug_buffer <<= 1;
               debug_buffer |= msb ^ static_cast<TBuffer>(0x1U);
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
        ++buffer_count;
        STUB(++pending_count);
      } else {
        DEBUGF('z', "BITS DISCARDED: " << std::dec << std::setw(2) << bit_count
                                       << std::hex << "; VALUE: "
                                       << std::setw(10) << debug_buffer);
        DEBUGF('e', "DECODING COMPLETED:"
                        << std::endl
                        << "    CHARACTER: " << std::setw(4)
                        << (current & 0xFFU) << std::endl
                        << "    ENCODING: " << std::setw(10)
                        << (debug_buffer << (buffer_bits - bit_count))
                        << std::endl
                        << "    LENGTH: " << std::dec << bit_count << std::endl
                        << "    PENDING: " << pending_count << std::hex);
        break;
      }

      // refreshes buffer if all bits have been read
      if (buffer_count >= buffer_bits) {
        if (is.rdstate() & std::ifstream::eofbit) {
          buffer = 0;
        } else {
          is.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
          buffer_count = 0;
        }
      }
    }
  }
}
