#include <fcntl.h>
#include <unistd.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "debug.h"

struct symbol_range {
  uint32_t occurences = 0;
  uint32_t upper = 0;
  uint32_t lower = 0;
};

void scan_options(int argc, char **argv) {
  opterr = 0;
  for (;;) {
    int option = getopt(argc, argv, "@:");
    if (option == EOF) break;
    switch (option) {
      case '@':
        debugflags::setflags(optarg);
        break;
      case '?':

      default:
        std::cerr << "-" << char(optopt) << ": invalid option" << std::endl;
        break;
    }
  }
}

void compress_file(std::string in_filename, std::string out_filename) {
  std::ifstream instream(in_filename);
  if ((instream.rdstate() & std::ifstream::failbit) != 0) {
    std::cerr << "Failed to open file " << in_filename << std::endl;
    return;
  }

  std::ofstream outstream(out_filename,
                          std::ofstream::binary | std::ofstream::out);
  if ((outstream.rdstate() & std::ofstream::failbit) != 0) {
    std::cerr << "Failed to open file " << out_filename << std::endl;
    return;
  }

  std::map<char, symbol_range> symbols;

  char current;
  while (instream.get(current)) {
    ++symbols[current].occurences;
  }

  // builds symbol table and writes it to outfile as it constructs it
  uint32_t cumulative_lower_bound = 0;
  std::cerr << std::showbase << std::hex << std::internal << std::setfill('0');
  DEBUGF('t', "WRITING TABLE");
  for (auto &x : symbols) {
    x.second.lower = cumulative_lower_bound;
    x.second.upper = cumulative_lower_bound =
        x.second.lower + x.second.occurences;

    outstream.write(&(x.first), 1);
    outstream.write(reinterpret_cast<char const *>(&x.second.occurences),
                    sizeof(x.second.occurences));

    DEBUGF('t', "    SYMBOL: " << std::setw(4) << (x.first & 0xFFU)
                               << "; UNADJUSTED BOUNDS: [" << std::setw(10)
                               << x.second.lower << ", " << std::setw(10)
                               << x.second.upper << ")");
  }

  // writes a NUL character entry with 0 occurences to denote that
  // the table has ended
  const uint32_t ZERO = 0;
  outstream.write(reinterpret_cast<char const *>(&ZERO), 1);
  outstream.write(reinterpret_cast<char const *>(&ZERO), sizeof(ZERO));

  uint32_t upper_bound = std::numeric_limits<uint32_t>::max();
  uint32_t lower_bound = 0;
  int pending = 0, buffer_counter = 0, max_bit_count = 0;
  uint32_t buffer = 0;
  const int buffer_bits = std::numeric_limits<uint32_t>::digits;
  uint32_t first_bit = static_cast<uint32_t>(0x1U) << (buffer_bits - 1);
  uint32_t second_bit = first_bit >> 1;

  uint32_t in_size = cumulative_lower_bound;
  instream.clear();
  instream.seekg(0, instream.beg);

  while (instream.get(current)) {
    // generate the range and bounds for selected symbol at this depth in the
    // file
    uint32_t range = upper_bound - lower_bound;
    upper_bound = lower_bound + (range / in_size * symbols[current].upper);
    lower_bound += range / in_size * symbols[current].lower;

    DEBUGF('z', "NEXT SYMBOL: " << std::setw(4) << (current & 0xFFU)
                                << "; RANGE: [" << std::setw(10) << lower_bound
                                << ", " << std::setw(10) << upper_bound << ")");

    uint32_t debug_buffer = 0;
    int bit_count = 0;
    for (;;) {
      if ((upper_bound ^ lower_bound) < first_bit) {
        // first bit matches
        if (buffer_counter >= buffer_bits) {
          DEBUGF('b',
                 "        BUFFER FULL; FLUSHING: " << std::setw(10) << buffer);
          outstream.write(reinterpret_cast<char *>(&buffer), sizeof(buffer));
          buffer = buffer_counter = 0;
        }

        // write msb to the end of the buffer
        uint32_t msb = lower_bound >> (buffer_bits - 1);
        buffer <<= 1;
        buffer |= msb;
        debug_buffer <<= 1;
        debug_buffer |= msb;
        ++buffer_counter;

        DEBUGF('x', "    MOST SIGNIFICANT BIT: " << std::noshowbase << msb
                                                 << std::showbase);

        // renormalize the ranges
        upper_bound <<= 1;
        upper_bound |= 1;
        lower_bound <<= 1;

        // add pending bits to the buffer, writing to the output stream whenever
        // if gets full
        uint32_t pending_bit = msb ^ static_cast<uint32_t>(0x1U);
        if (pending > 0) {
          DEBUGF('p', std::noshowbase << std::dec << std::setfill(' ')
                                      << "    FLUSHING " << std::setw(2)
                                      << pending << " PENDING BITS WITH VALUE "
                                      << pending_bit << std::showbase
                                      << std::hex << std::setfill('0'));
        }
        ++bit_count;
        while (pending > 0) {
          if (buffer_counter >= buffer_bits) {
            DEBUGF('b', "        BUFFER FULL; FLUSHING: " << std::setw(10)
                                                          << buffer);
            outstream.write(reinterpret_cast<char *>(&buffer), sizeof(buffer));
            buffer = buffer_counter = 0;
          }
          buffer <<= 1;
          buffer |= pending_bit;
          debug_buffer <<= 1;
          debug_buffer |= pending_bit;
          ++buffer_counter;
          --pending;
          ++bit_count;
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
        upper_bound |= first_bit | static_cast<uint32_t>(0x1U);
        ++pending;
      } else {
        DEBUGF('z', "CURRENT ENCODING SEGMENT: "
                        << std::setw(10)
                        << (buffer << (buffer_bits - buffer_counter)));
        DEBUGF('e', "ENCODING COMPLETED:"
                        << std::endl
                        << "    CHARACTER: " << std::setw(4)
                        << (current & 0xFFU) << std::endl
                        << "    ENCODING: " << std::setw(10)
                        << (debug_buffer << (buffer_bits - bit_count))
                        << std::endl
                        << "    LENGTH: " << std::dec << bit_count << std::endl
                        << "    PENDING: " << pending << std::hex);
        max_bit_count = std::max(max_bit_count, bit_count);
        break;
      }
    }
  }

  // fill out the buffer with the high order bits of the lower bound
  if (buffer_counter > 0) {
    int bits_needed = buffer_bits - buffer_counter;
    DEBUGF('b', std::noshowbase << std::dec << "PADDING BUFFER WITH "
                                << bits_needed << " BITS" << std::showbase
                                << std::hex);
    buffer <<= bits_needed;
    buffer |= lower_bound >> buffer_counter;
    DEBUGF('b', "FINAL BUFFER: " << std::setw(10) << buffer);
    outstream.write(reinterpret_cast<char *>(&buffer), sizeof(buffer));
  }
}

void decompress_file(std::string in_filename, std::string out_filename) {
  std::ifstream instream(in_filename, std::ifstream::binary);
  if ((instream.rdstate() & std::ifstream::failbit) != 0) {
    std::cerr << "Failed to open file " << in_filename << std::endl;
    return;
  }

  std::ofstream outstream(out_filename, std::ofstream::binary);
  if ((outstream.rdstate() & std::ofstream::failbit) != 0) {
    std::cerr << "Failed to open file " << out_filename << std::endl;
    return;
  }

  std::map<char, symbol_range> symbols;
  uint32_t occurences;
  char symbol;

  // also serves as the total number of characters in the original file
  uint32_t cumulative_lower_bound = 0;
  std::cerr << std::hex << std::showbase << std::internal << std::setfill('0');
  DEBUGF('t', "READING TABLE");
  while (instream.get(symbol)) {
    instream.read(reinterpret_cast<char *>(&occurences), 4);

    // table has ended
    if (occurences == 0) break;

    DEBUGF('t', "    SYMBOL: " << std::setw(4) << (symbol & 0xFFU)
                               << "; UNADJUSTED BOUNDS: [" << std::setw(10)
                               << cumulative_lower_bound << ", "
                               << std::setw(10)
                               << cumulative_lower_bound + occurences << ")");

    symbols[symbol] = {occurences, cumulative_lower_bound + occurences,
                       cumulative_lower_bound};
    cumulative_lower_bound += occurences;
  }

  DEBUGF('y', "Total characters: " << std::dec << cumulative_lower_bound
                                   << std::hex);

  uint32_t upper_bound = std::numeric_limits<uint32_t>::max();
  uint32_t lower_bound = 0;
  int pending = 0, buffer_counter = 0;
  uint32_t buffer, encoding;
  int buffer_bits = std::numeric_limits<uint32_t>::digits;
  uint32_t first_bit = static_cast<uint32_t>(0x1U) << (buffer_bits - 1);
  uint32_t second_bit = first_bit >> 1;
  uint32_t characters_written = 0;
  uint32_t in_size = cumulative_lower_bound;

  // fill encoding and buffer
  instream.read(reinterpret_cast<char *>(&encoding), sizeof(encoding));
  instream.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
  for (;;) {
    uint32_t range = upper_bound - lower_bound;
    char current;
    int encoding_length = 0;

    DEBUGF('c', "SCANNING FOR SYMBOLS THAT COULD HAVE BEEN ENCODED WITH "
                    << std::setw(10) << encoding);
    for (auto &x : symbols) {
      // computes what the bounds would be given that x was encoded
      // and sees if the actual encoding falls within them
      uint32_t upper_given_x = lower_bound + (range / in_size * x.second.upper);
      uint32_t lower_given_x = lower_bound + (range / in_size * x.second.lower);

      if (encoding < upper_given_x && encoding >= lower_given_x) {
        current = x.first;
        --x.second.occurences;
        DEBUGF('c', "    CHARACTER " << std::setw(4) << (current & 0xFFU)
                                     << " BOUNDS THE ENCODING");
      }
    }

    outstream.write(&current, 1);
    ++characters_written;
    if (characters_written >= in_size) return;

    upper_bound = lower_bound + (range / in_size * symbols[current].upper);
    lower_bound += range / in_size * symbols[current].lower;

    DEBUGF('z', "NEXT SYMBOL: " << std::setw(4) << (current & 0xFFU)
                                << "; RANGE: [" << std::setw(10) << lower_bound
                                << ", " << std::setw(10) << upper_bound << ")");
    DEBUGF('z', "CURRENT ENCODING SEGMENT: " << std::setw(10) << encoding);

    // loop should remove bits from the range that match or were pending bits,
    // since they dont tell us anything we dont already know
    uint32_t debug_buffer = 0;
    int bit_count = 0;
    for (;;) {
      if ((upper_bound ^ lower_bound) < first_bit) {
        // first bit matches
        uint32_t msb = lower_bound >> (buffer_bits - 1);
        debug_buffer <<= 1;
        debug_buffer |= msb;
        lower_bound <<= 1;
        upper_bound <<= 1;
        upper_bound |= static_cast<uint32_t>(0x1U);
        encoding <<= 1;
        uint32_t current_bit = (buffer & first_bit) >> (buffer_bits - 1);
        buffer <<= 1;
        encoding |= current_bit;
        ++buffer_counter;
        ++encoding_length;
        ++bit_count;
        while (pending > 0) {
          // encoding <<= 1;
          // uint32_t current_bit = (buffer & 0x80000000U) >> 31;
          // buffer <<= 1;
          // encoding |= current_bit;
          // refreshes buffer if all bits have been read
          /*
          if (buffer_counter >= 32) {
              if ((instream.rdstate() & std::ifstream::eofbit) != 0) {
              buffer = 0;
              }
              instream.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
              buffer_counter = 0;
          }
          */
          debug_buffer <<= 1;
          debug_buffer |= ~msb;
          --pending;
          ++bit_count;
        }
      } else if (lower_bound >= second_bit &&
                 upper_bound < (first_bit | second_bit)) {
        // pending bits
        DEBUGF('p', "    BIT PENDING");
        lower_bound <<= 1;
        lower_bound &= ~first_bit;
        upper_bound <<= 1;
        upper_bound |= first_bit | static_cast<uint32_t>(0x1U);
        ++pending;
        /*
        uint32_t encoding_msb = encoding & 0x80000000U;
        encoding <<= 1;
        encoding |= encoding_msb;
        uint32_t current_bit = (buffer & 0x80000000U) >> 31;
        buffer <<= 1;
        encoding |= current_bit;
        ++buffer_counter;
        lower_bound &= 0xBFFFFFFFU;
        upper_bound |= 0x40000000U;
        encoding &= 0xBFFFFFFFU;
        encoding |= ((encoding >> 1) & 0x40000000U);
        ++buffer_counter;
        lower_bound <<= 1;
        upper_bound <<= 1;
        upper_bound |= 0x00000001U;
        uint32_t current_bit = (buffer & 0x80000000U) >> 31;
        buffer <<= 1;
        encoding |= current_bit;
        */
        ++encoding_length;
      } else {
        DEBUGF('e', "DECODING COMPLETED:"
                        << std::endl
                        << "    CHARACTER: " << std::setw(4)
                        << (current & 0xFFU) << std::endl
                        << "    ENCODING: " << std::setw(10)
                        << (debug_buffer << (buffer_bits - bit_count))
                        << std::endl
                        << "    LENGTH: " << std::dec << bit_count << std::endl
                        << "    PENDING: " << pending << std::hex);
        break;
      }

      // refreshes buffer if all bits have been read
      if (buffer_counter >= buffer_bits) {
        if ((instream.rdstate() & std::ifstream::eofbit) != 0) {
          buffer = 0;
        }
        instream.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
        buffer_counter = 0;
      }
    }
  }
}

int main(int argc, char **argv) {
  // maybe change to ios_base::binary to allow
  // binary files to be compressed
  if (argc < 3) return EXIT_FAILURE;
  scan_options(argc, argv);
  if (optind + 3 > argc) return EXIT_FAILURE;

  std::string command{argv[optind]};
  std::string file1{argv[optind + 1]};
  std::string file2{argv[optind + 2]};

  if (command.compare("decode") == 0) {
    decompress_file(file1, file2);
  } else if (command.compare("encode") == 0) {
    compress_file(file1, file2);
  } else {
    std::cerr
        << "Usage: ./arcode [- @ flag]... encode|decode \'infile\' \'outfile\'"
        << std::endl;
    return 1;
  }
}
