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
    // note: i believe all constructors from the STL set the in/out mode bits
    // appropriately for streams, so there's no need to specify them here
    std::ifstream instream(file1, std::ios_base::binary);
    if (instream.fail()) {
      std::cerr << "Failed to open file " << file1 << std::endl;
      return 1;
    }

    std::ofstream outstream(file2, std::ios_base::binary);
    if (outstream.fail()) {
      std::cerr << "Failed to open file " << file2 << std::endl;
      return 1;
    }

    auto table = read_table<uint32_t, char>(instream);
    decompress_stream(instream, outstream, table);
  } else if (command.compare("encode") == 0) {
    std::ifstream instream(file1, std::ios_base::binary);
    if (instream.fail()) {
      std::cerr << "Failed to open file " << file1 << std::endl;
      return 1;
    }

    std::ofstream outstream(file2, std::ios_base::binary);
    if (outstream.fail()) {
      std::cerr << "Failed to open file " << file2 << std::endl;
      return 1;
    }

    auto table = build_table<uint32_t>(instream);
    instream.clear();
    instream.seekg(0, std::ios_base::beg);
    write_table(outstream, table);
    compress_stream(instream, outstream, table);
  } else {
    std::cerr
        << "Usage: ./arcode [- @ flag]... encode|decode \'infile\' \'outfile\'"
        << std::endl;
    return 1;
  }
}
