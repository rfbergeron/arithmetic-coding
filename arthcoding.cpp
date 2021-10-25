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
  // debugflags::setflags ("y");
}

// TODO:
// eliminate unnecessary intermediate variables where necessary
// read file contents in chunks

/*void write_pending(unsigned char * buffer, unsigned char& buffer_counter,
const unsigned int value, unsigned int& pending, ofstream& outstream) {
   while(pending > 0) {
      if(buffer_counter >= 8) {
         DEBUGF('b', "writing " << buffer << "pending bits to file");
            outstream.write(reinterpret_cast<char *>(buffer), 1);
            *buffer = buffer_counter = 0;
      }
      *buffer <<= 1;
      *buffer |= value;
      ++buffer_counter;
      --pending;
   }
}*/

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

    DEBUGF('b', "symbol: " << static_cast<uint32_t>(x.first)
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
  uint32_t range = upper_bound - lower_bound;
  uint32_t pending = 0;
  uint8_t buffer = 0;
  uint8_t buffer_counter = 0;

  uint32_t in_size = std::filesystem::file_size(in_path);
  DEBUGF('y', "STL says the file is " << in_size << " bytes long");
  instream.clear();
  instream.seekg(0, instream.beg);
  while (instream.get(current)) {
    // generate the range and bounds for selected symbol at this depth in the
    // file
    range = upper_bound - lower_bound;
    DEBUGF('z', "available range: " << range);
    upper_bound = lower_bound + (range / in_size * symbols[current].upper);
    lower_bound = lower_bound + (range / in_size * symbols[current].lower);

    DEBUGF('z', "  range for symbol " << static_cast<uint32_t>(current) << ": "
                                      << std::hex << std::showbase
                                      << lower_bound << " to " << std::hex
                                      << std::showbase << upper_bound);

    // runs while the first bit of bounds matches or pending bits are detected
    for (;;) {
      // first bit one
      if (upper_bound >= 0x80000000U && lower_bound >= 0x80000000U) {
        if (buffer_counter >= 8) {
          DEBUGF('b', "writing " << buffer << " to file");
          outstream.write(reinterpret_cast<char *>(&buffer), 1);
          buffer = buffer_counter = 0;
        }

        // writes one to the end of the buffer
        buffer <<= 1;
        buffer |= 1;
        ++buffer_counter;

        DEBUGF('x', "appending " << (upper_bound >> 31) << "; buffer is now "
                                 << buffer);

        // renormalizes the ranges
        upper_bound <<= 1;
        upper_bound |= 1;
        lower_bound <<= 1;

        // adds pending bits to the buffer until its full,
        // then writes the buffer to the file and continues
        // write_pending(&buffer, buffer_counter, 1, pending, outstream);

        while (pending > 0) {
          if (buffer_counter >= 8) {
            DEBUGF('b', "writing " << buffer << "pending bits to file");
            outstream.write(reinterpret_cast<char *>(&buffer), 1);
            buffer = buffer_counter = 0;
          }
          buffer <<= 1;
          ++buffer_counter;
          --pending;
        }
      }
      // first bit zero
      else if (upper_bound < 0x80000000U && lower_bound < 0x80000000U) {
        if (buffer_counter >= 8) {
          DEBUGF('b', "writing " << buffer << " to file");
          outstream.write(reinterpret_cast<char *>(&buffer), 1);
          buffer = buffer_counter = 0;
        }
        // writes a zero to the end of the buffer
        buffer <<= 1;
        ++buffer_counter;

        DEBUGF('x', "appending " << (upper_bound >> 31) << "; buffer is now "
                                 << buffer);

        // renormalizes the ranges
        upper_bound <<= 1;
        upper_bound |= 1;
        lower_bound <<= 1;

        // adds pending bits to the buffer until its full,
        // then writes the buffer to the file and continues
        // write_pending(&buffer, buffer_counter, 0, pending, outstream);

        while (pending > 0) {
          if (buffer_counter >= 8) {
            DEBUGF('b', "writing " << buffer << "pending bits to file");
            outstream.write(reinterpret_cast<char *>(&buffer), 1);
            buffer = buffer_counter = 0;
          }
          buffer <<= 1;
          buffer |= 1;
          ++buffer_counter;
          --pending;
        }
      }
      // pending bits, indicated by the first bits of the upper and lower bounds
      // not matching and the second bits being the inverse of the first bit
      else if (lower_bound >= 0x40000000 && upper_bound < 0xC0000000U) {
        DEBUGF('x', "chosen symbol: " << static_cast<uint8_t>(current)
                                      << " adding pending bit");
        DEBUGF('v', "     " << lower_bound << " " << upper_bound);
        // preserve first bit since it is undecided and discard second one
        lower_bound <<= 1;
        lower_bound &= 0x7FFFFFFE;
        upper_bound <<= 1;
        upper_bound |= 0x80000001U;
        ++pending;
      } else {
        DEBUGF('z', "  adjusted range   " << static_cast<uint32_t>(current)
                                          << ": " << std::hex << std::showbase
                                          << lower_bound << " to " << std::hex
                                          << std::showbase << upper_bound);
        break;
      }
    }
  }

  // fill out the buffer with the bits of the lower bound,
  // then add 1
  if (buffer_counter > 0) {
    DEBUGF('z', "buffer is only " << static_cast<uint32_t>(buffer) << ", need "
                                  << (8 - buffer_counter) << " more bytes");
    uint32_t midpoint = lower_bound + (range / 2);
    while (buffer_counter < 8) {
      buffer = buffer << 1;
      buffer |= ((midpoint >> 31) & 0x00000001);
      midpoint <<= 1;
      ++buffer_counter;
    }
    outstream.write(reinterpret_cast<char *>(&buffer), 1);
  }

  DEBUGF('z', "finished encoding");
  outstream.close();
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

  while (instream.get(symbol)) {
    instream.read(reinterpret_cast<char *>(&occurences), 4);

    // table has ended
    if (occurences == 0) break;

    DEBUGF('y', "character " << static_cast<uint32_t>(symbol) << "occured "
                             << occurences << " times");

    symbols[symbol] = {occurences, cumulative_lower_bound + occurences,
                       cumulative_lower_bound};

    cumulative_lower_bound += occurences;
  }

  DEBUGF('y', std::hex << std::showbase);

  for (auto &x : symbols) {
    DEBUGF('y', x.first << " : " << static_cast<uint32_t>(x.first) << " : "
                        << x.second.lower << " to " << x.second.upper);
  }
  DEBUGF('y', "total characters: " << cumulative_lower_bound);

  // should be 0xFFFFFFFF
  uint32_t upper_bound = std::numeric_limits<uint32_t>::max();
  uint32_t lower_bound = 0;
  uint32_t range = upper_bound - lower_bound;
  uint32_t encoding = 0;
  uint8_t buffer;
  uint8_t buffer_counter = 8;
  uint32_t characters_written = 0;

  // reads the first 4 bytes of the encoded file
  // to fill the encoding variable for comparison
  // cannot read directly to int because it was
  // encoded byte by byte
  for (int i = 0; i < 4; ++i) {
    instream.read(reinterpret_cast<char *>(&buffer), 1);
    encoding <<= 8;
    encoding |= buffer;
  }

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
                      << static_cast<uint32_t>(x.first) << " occured: "
                      << std::hex << std::showbase << lower_given_x << " to "
                      << std::hex << std::showbase << upper_given_x);

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
      if (lower_bound >= 0x80000000U || upper_bound < 0x80000000U) {
        // dont need to do anything besides shift out high order bits
      } else if (lower_bound >= 0x40000000 && upper_bound < 0xC0000000U) {
        // preserve highest order bits and discard 2nd highest order bits
        // we also need to do this for the encoding itself
        // (remove pending bits from encoding)
        lower_bound &= 0xBFFFFFFFU;
        upper_bound |= 0x40000000;
        encoding &= 0xBFFFFFFFU;
        encoding |= ((encoding >> 1) & 0x40000000);
      } else {
        DEBUGF('z', "     char took " << encoding_length << " bits to encode");
        break;
      }
      ++encoding_length;

      // refreshes buffer if all bits have been read
      if (buffer_counter >= 8) {
        if ((instream.rdstate() & std::ifstream::eofbit) != 0) {
          buffer = 0;
        }
        instream.read(reinterpret_cast<char *>(&buffer), 1);
        buffer_counter = 0;
      }

      uint32_t current_bit = (buffer >> (7 - buffer_counter)) & 0x1;
      encoding <<= 1;
      encoding += current_bit;
      ++buffer_counter;
      lower_bound <<= 1;
      upper_bound <<= 1;
      ++upper_bound;
    }
  }
  outstream.close();
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
