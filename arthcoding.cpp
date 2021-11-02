#include <fcntl.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
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
  std::filesystem::path in_path{in_filename};
  std::ifstream instream(in_path);
  if ((instream.rdstate() & std::ifstream::failbit) != 0) {
    std::cerr << "Failed to open file " << in_filename << std::endl;
    return;
  }

  std::filesystem::path out_path{out_filename};
  std::ofstream outstream(out_path, std::ofstream::binary | std::ofstream::out);
  if ((outstream.rdstate() & std::ofstream::failbit) != 0) {
    std::cerr << "Failed to open file " << out_filename << std::endl;
    return;
  }

  std::map<char, symbol_range> symbols;

  char current;
  while (instream.get(current)) {
    ++symbols[current].occurences;
  }

  DEBUGF('y', "parsed all symbols");

  // builds symbol table and writes it to outfile as it constructs it
  uint32_t cumulative_lower_bound = 0;
  for (auto &x : symbols) {
    x.second.lower = cumulative_lower_bound;
    x.second.upper = cumulative_lower_bound =
        x.second.lower + x.second.occurences;

    outstream.write(&(x.first), 1);
    outstream.write(reinterpret_cast<char const *>(&x.second.occurences),
                    sizeof(x.second.occurences));

    DEBUGF('b', "symbol: " << std::showbase << std::hex << x.first
                           << std::noshowbase << std::dec
                           << " lower bound: " << x.second.lower
                           << " upper bound: " << x.second.upper);
  }

  // writes a NUL character entry with 0 occurences to denote that
  // the table has ended
  const uint32_t ZERO = 0;
  outstream.write(reinterpret_cast<char const *>(&ZERO), 1);
  outstream.write(reinterpret_cast<char const *>(&ZERO), sizeof(ZERO));

  uint32_t upper_bound = std::numeric_limits<uint32_t>::max();
  uint32_t lower_bound = 0;
  uint32_t pending = 0;
  uint32_t buffer = 0;
  int buffer_counter = 0;
  const int buffer_bits = std::numeric_limits<uint32_t>::digits;
  uint32_t first_bit = 0x00000001U << (buffer_bits - 1);

  uint32_t in_size = std::filesystem::file_size(in_path);
  DEBUGF('y', "STL says the file is " << in_size << " bytes long");
  instream.clear();
  instream.seekg(0, instream.beg);

  std::cerr << std::showbase << std::hex << std::internal << std::setfill('0');
  while (instream.get(current)) {
    // generate the range and bounds for selected symbol at this depth in the
    // file
    uint32_t range = upper_bound - lower_bound;
    upper_bound = lower_bound + (range / in_size * symbols[current].upper);
    lower_bound += range / in_size * symbols[current].lower;

    DEBUGF('z', "range for symbol " << std::setw(4) << static_cast<int>(current)
                                    << ": " << lower_bound << " to "
                                    << upper_bound);

    for (;;) {
      if ((upper_bound ^ lower_bound) < first_bit) {
        // first bit matches
        if (buffer_counter >= buffer_bits) {
          DEBUGF('b', "        buffer full; writing " << std::setw(10) << buffer
                                                      << " to file");
          outstream.write(reinterpret_cast<char *>(&buffer), sizeof(buffer));
          buffer = buffer_counter = 0;
        }

        // write msb to the end of the buffer
        uint32_t msb = lower_bound >> (buffer_bits - 1);
        buffer <<= 1;
        buffer |= msb;
        ++buffer_counter;

        DEBUGF('x', "    appending " << std::noshowbase << msb << std::showbase
                                     << "; buffer is now " << std::setw(10)
                                     << buffer);

        // renormalize the ranges
        upper_bound <<= 1;
        upper_bound |= 1;
        lower_bound <<= 1;

        // add pending bits to the buffer, writing to the output stream whenever
        // if gets full
        uint32_t pending_bit = msb ^ 0x00000001U;
        if (pending > 0) {
          DEBUGF('b', std::noshowbase << std::dec << std::setfill(' ')
                                      << "    flushing " << std::setw(2)
                                      << pending << " pending bits with value "
                                      << pending_bit << std::showbase
                                      << std::hex << std::setfill('0'));
        }
        while (pending > 0) {
          if (buffer_counter >= buffer_bits) {
            DEBUGF('b', "        buffer full; writing "
                            << std::setw(10) << buffer << " to file");
            outstream.write(reinterpret_cast<char *>(&buffer), sizeof(buffer));
            buffer = buffer_counter = 0;
          }
          buffer <<= 1;
          buffer |= pending_bit;
          ++buffer_counter;
          --pending;
        }
      } else if (lower_bound >= 0x40000000U && upper_bound < 0xC0000000U) {
        // pending bits, indicated by the first bits of the upper and lower
        // bounds not matching and the second bits being the inverse of the
        // first bit preserve first bit since it is undecided and discard second
        // one
        DEBUGF('b', "    pending bit detected");
        lower_bound <<= 1;
        lower_bound &= 0x7FFFFFFFU;
        upper_bound <<= 1;
        upper_bound |= 0x80000001U;
        ++pending;
      } else {
        DEBUGF('z',
               "fully encoded " << std::setw(4) << static_cast<int>(current));
        break;
      }
    }
  }

  // fill out the buffer with the high order bits of the lower bound
  if (buffer_counter > 0) {
    int bits_needed = buffer_bits - buffer_counter;
    DEBUGF('z', std::noshowbase << std::dec << "buffer needs " << bits_needed
                                << " more bits" << std::showbase << std::hex);
    buffer <<= bits_needed;
    buffer |= lower_bound >> buffer_counter;
    DEBUGF('z', "final buffer: " << std::setw(10) << buffer);
    outstream.write(reinterpret_cast<char *>(&buffer), sizeof(buffer));
  }

  DEBUGF('z', "finished encoding");
}

void decompress_file(std::string in_filename, std::string out_filename) {
  std::filesystem::path in_path{in_filename};
  std::ifstream instream(in_path, std::ifstream::binary);
  if ((instream.rdstate() & std::ifstream::failbit) != 0) {
    std::cerr << "Failed to open file " << in_filename << std::endl;
    return;
  }

  std::filesystem::path out_path{out_filename};
  std::ofstream outstream(out_path, std::ofstream::binary);
  if ((outstream.rdstate() & std::ofstream::failbit) != 0) {
    std::cerr << "Failed to open file " << out_filename << std::endl;
    return;
  }

  DEBUGF('z', "reading table");

  std::map<char, symbol_range> symbols;
  uint32_t occurences;
  char symbol;

  // also serves as the total number of characters in the original file
  uint32_t cumulative_lower_bound = 0;

  DEBUGF('y', std::hex << std::showbase);
  while (instream.get(symbol)) {
    instream.read(reinterpret_cast<char *>(&occurences), 4);

    // table has ended
    if (occurences == 0) break;

    DEBUGF('y', "character " << symbol << "occured " << occurences << " times");

    symbols[symbol] = {occurences, cumulative_lower_bound + occurences,
                       cumulative_lower_bound};
    cumulative_lower_bound += occurences;
  }

  DEBUGF('y', std::dec << std::noshowbase);
  DEBUGF('y', "Total characters: " << cumulative_lower_bound);

  uint32_t upper_bound = std::numeric_limits<uint32_t>::max();
  uint32_t lower_bound = 0;
  uint32_t range = upper_bound - lower_bound;
  uint32_t encoding;
  uint32_t buffer;
  int buffer_counter = 0;
  int buffer_bits = std::numeric_limits<uint32_t>::digits;
  uint32_t first_bit = 0x00000001U << buffer_bits;
  uint32_t characters_written = 0;

  // fill encoding and buffer
  instream.read(reinterpret_cast<char *>(&encoding), sizeof(encoding));
  instream.read(reinterpret_cast<char *>(&buffer), sizeof(buffer));
  DEBUGF('y', std::hex << std::showbase);

  for (;;) {
    range = upper_bound - lower_bound;
    DEBUGF('z', "available range: " << range);
    DEBUGF('z', "encoding: " << encoding)
    char c;
    uint32_t encoding_length = 0;

    for (auto &x : symbols) {
      // computes what the bounds would be given that x was encoded
      // and sees if the actual encoding falls within them
      uint32_t upper_given_x =
          lower_bound + (range / cumulative_lower_bound * x.second.upper);
      uint32_t lower_given_x =
          lower_bound + (range / cumulative_lower_bound * x.second.lower);

      DEBUGF('z', "     range given that "
                      << x.first << " occured: " << lower_given_x << " to "
                      << upper_given_x);

      if (encoding < upper_given_x && encoding >= lower_given_x) {
        c = x.first;
        --x.second.occurences;
        DEBUGF('x', "range " << upper_given_x << " to " << lower_given_x
                             << " matches encoding " << encoding << "for char "
                             << c);
        break;
      }
    }

    outstream.write(&c, 1);
    ++characters_written;
    if (characters_written >= cumulative_lower_bound) return;
    DEBUGF('z', characters_written << " out of " << cumulative_lower_bound
                                   << " characters written");

    upper_bound =
        lower_bound + (range / cumulative_lower_bound * symbols[c].upper);
    lower_bound =
        lower_bound + (range / cumulative_lower_bound * symbols[c].lower);

    // loop should remove bits from the range that match or were pending bits,
    // since they dont tell us anything we dont already know
    for (;;) {
      DEBUGF('z', "     upper is now " << upper_bound);
      DEBUGF('z', "     encoding is now " << encoding);
      DEBUGF('z', "     lower is now " << lower_bound);
      if ((upper_bound ^ lower_bound) < first_bit) {
        // first bit matches
        lower_bound <<= 1;
        upper_bound <<= 1;
        upper_bound |= 0x00000001U;
        encoding <<= 1;
        uint32_t current_bit =
            (buffer >> (buffer_bits - 1 - buffer_counter)) & 0x00000001U;
        encoding |= current_bit;
        ++buffer_counter;
        ++encoding_length;
      } else if (lower_bound >= 0x40000000U && upper_bound < 0xC0000000U) {
        // pending bits
        lower_bound <<= 1;
        lower_bound &= 0x7FFFFFFFU;
        upper_bound <<= 1;
        upper_bound |= 0x80000001U;
        uint32_t encoding_msb =
            encoding &
            (0x00000001U << (std::numeric_limits<uint32_t>::digits - 1));
        encoding <<= 1;
        encoding |= encoding_msb;
        uint32_t current_bit =
            (buffer >> (buffer_bits - 1 - buffer_counter)) & 0x00000001U;
        encoding |= current_bit;
        ++buffer_counter;
        ++encoding_length;
      } else {
        DEBUGF('z', "     char took " << encoding_length << " bits to encode");
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
