#include <fcntl.h>
#include <unistd.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "arthcoder.h"
#include "debug.h"

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
    std::ifstream instream(file1, std::ifstream::binary | std::ifstream::in);
    if ((instream.rdstate() & std::ifstream::failbit) != 0) {
      std::cerr << "Failed to open file " << file1 << std::endl;
      return 1;
    }

    std::ofstream outstream(file2);
    if ((outstream.rdstate() & std::ofstream::failbit) != 0) {
      std::cerr << "Failed to open file " << file2 << std::endl;
      return 1;
    }

    decompress_stream<uint32_t>(instream, outstream);
  } else if (command.compare("encode") == 0) {
    std::ifstream instream(file1);
    if ((instream.rdstate() & std::ifstream::failbit) != 0) {
      std::cerr << "Failed to open file " << file1 << std::endl;
      return 1;
    }

    std::ofstream outstream(file2, std::ofstream::binary | std::ofstream::out);
    if ((outstream.rdstate() & std::ofstream::failbit) != 0) {
      std::cerr << "Failed to open file " << file2 << std::endl;
      return 1;
    }

    compress_stream<uint32_t>(instream, outstream);
  } else {
    std::cerr
        << "Usage: ./arcode [- @ flag]... encode|decode \'infile\' \'outfile\'"
        << std::endl;
    return 1;
  }
}