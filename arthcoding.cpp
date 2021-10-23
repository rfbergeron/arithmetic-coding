#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <memory>
#include <map>
#include <limits>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "debug.h"

using namespace std;

struct symbol_range {
   unsigned int occurences;
   unsigned int upper;
   unsigned int lower;
};

void scan_options (int argc, char** argv) {
   opterr = 0;
   for (;;) {
      int option = getopt (argc, argv, "@:");
      if (option == EOF) break;
      switch (option) {
         case '@':
            debugflags::setflags (optarg);
            break;
         case '?':

         default:
            cerr << "-" << char (optopt) << ": invalid option";
            break;
      }
   }
   //debugflags::setflags ("y");
}

// TODO: 
// eliminate unnecessary intermediate variables where necessary
// read file contents in chunks

/*void write_pending(unsigned char * buffer, unsigned char& buffer_counter, const unsigned int value, unsigned int& pending, ofstream& outstream) {
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

void compress_file(char * in_filename, char * out_filename) {
   ifstream instream({in_filename}, ifstream::binary);
   ofstream outstream({out_filename}, ofstream::binary);
   struct stat buf;
   const unsigned int ZERO = 0;

   if ((instream.rdstate() & ifstream::failbit) != 0) {
      cerr << "Failed to open file " << in_filename << endl;
      return;
   }

   if ((outstream.rdstate() & ofstream::failbit) != 0) {
      cerr << "Failed to open file " << out_filename << endl;
      return;
   }

   stat(in_filename, &buf);
   char * file_contents = new char[buf.st_size];
   instream.read(file_contents, buf.st_size);

   DEBUGF('y', "file is " << buf.st_size << " bytes long");

   map<char, symbol_range> symbols;

   for(int i = 0 ; i < buf.st_size ; ++i) {
      char current = file_contents[i];

      if(symbols.find(current) != symbols.end())
         ++symbols[current].occurences;
      else
         symbols[current] = {1, 0, 0};
   }

   DEBUGF('y', "parsed all symbols");

   // builds symbol table and writes it to outfile as it
   // constructs it
   unsigned int cumulative_lower_bound = 0;

   for(auto& x : symbols) {
      x.second.lower = cumulative_lower_bound;
      x.second.upper = cumulative_lower_bound =
         x.second.lower + x.second.occurences;

      outstream.write(&(x.first), 1);
      outstream.write(reinterpret_cast<char const *>(&x.second.occurences), sizeof(x.second.occurences));

      DEBUGF('b', "symbol: " << static_cast<unsigned int>(x.first)
         << " lower bound: " << x.second.lower <<
         " upper bound: " << x.second.upper);
   }

   //writes a NUL character entry with 0 occurences to denote that
   //the table has ended
   outstream.write(reinterpret_cast<char const *>(&ZERO), 1);
   outstream.write(reinterpret_cast<char const *>(&ZERO), sizeof(ZERO));

   for(auto& x : symbols)
      DEBUGF('y', x.first << " : " << static_cast<unsigned int>(x.first)
         << " : " << x.second.occurences);

   //should be 0xFFFFFFFF
   unsigned int upper_bound = numeric_limits<unsigned int>::max();
   unsigned int lower_bound = 0;
   unsigned int range;
   unsigned int pending = 0;
   unsigned char buffer = 0;
   unsigned char buffer_counter = 0;

   DEBUGF('a', hex << showbase);
   DEBUGF('y', hex << showbase);
   DEBUGF('x', hex << showbase);
   DEBUGF('b', hex << showbase);

   DEBUGF('y', "total chars in hex: " << buf.st_size);

   for(int j = 0 ; j < buf.st_size ; ++j) {
      char current = file_contents[j];

      //generate the range and bounds for selected symbol at this depth in the file
      range = upper_bound - lower_bound;
      DEBUGF('z', "available range: " << range);
      upper_bound = lower_bound + (range / buf.st_size * symbols[current].upper);
      lower_bound = lower_bound + (range / buf.st_size * symbols[current].lower);

      DEBUGF('z', "  range for symbol " << static_cast<unsigned int>(current) << ": " << hex <<
         showbase << lower_bound << " to " << hex << showbase << upper_bound);

      // runs while the first bit of bounds matches or pending bits are detected
      for(;;) {
         // first bit one
         if(upper_bound >= 0x80000000U && lower_bound >= 0x80000000U) {
            //DEBUGF('v', "     " << (upper_bound ^ lower_bound));
            if(buffer_counter >= 8) {
               DEBUGF('b', "writing " << buffer
                  << " to file");
               outstream.write(reinterpret_cast<char *>(&buffer), 1);
               buffer = buffer_counter = 0;
            }
            //writes one to the end of the buffer
            buffer <<= 1;
            buffer |= 1;
            ++buffer_counter;

            DEBUGF('x', "appending " << (upper_bound >> 31) <<
               "; buffer is now " << buffer);

            //renormalizes the ranges
            upper_bound <<= 1;
            upper_bound |= 1;
            lower_bound <<= 1;

            //adds pending bits to the buffer until its full,
            //then writes the buffer to the file and continues
            //write_pending(&buffer, buffer_counter, 1, pending, outstream);

               while(pending > 0) {
                  if(buffer_counter >= 8) {
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
         else if(upper_bound < 0x80000000U && lower_bound < 0x80000000U) {
            if(buffer_counter >= 8) {
               DEBUGF('b', "writing " << buffer
                  << " to file");
               outstream.write(reinterpret_cast<char *>(&buffer), 1);
               buffer = buffer_counter = 0;
            }
            //writes a zero to the end of the buffer
            buffer <<= 1;
            ++buffer_counter;

            DEBUGF('x', "appending " << (upper_bound >> 31) <<
               "; buffer is now " << buffer);

            //renormalizes the ranges
            upper_bound <<= 1;
            upper_bound |= 1;
            lower_bound <<= 1;

            //adds pending bits to the buffer until its full,
            //then writes the buffer to the file and continues
            //write_pending(&buffer, buffer_counter, 0, pending, outstream);

            while(pending > 0) {
                  if(buffer_counter >= 8) {
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
         // pending bits
         else if(lower_bound >= 0x40000000 && upper_bound < 0xC0000000U) {
            DEBUGF('x', "chosen symbol: " << static_cast<unsigned char>(current)
               << " adding pending bit");
            DEBUGF('v', "     " << lower_bound << " " << upper_bound);
            // preserve first bit since it is undecided and discard second one
            lower_bound = lower_bound << 1;
            lower_bound = lower_bound & 0x7FFFFFFE;
            upper_bound = upper_bound << 1;
            upper_bound = upper_bound | 0x80000001U;
            ++pending;
         }
         else {
            DEBUGF('z', "  adjusted range   " << static_cast<unsigned int>(current) << ": " << hex << showbase << lower_bound << " to " << hex << showbase <<upper_bound);
            break;
         }
      }
   }

   // fill out the buffer with the bits of the lower bound,
   // then add 1
   if(buffer_counter > 0) {
      DEBUGF('z', "buffer is only " << static_cast<unsigned int>(buffer) << ", need " << (8 - buffer_counter) << " more bytes");
      unsigned int midpoint = lower_bound + (range / 2);
      while (buffer_counter < 8) {
         buffer = buffer << 1;
         buffer |= ((midpoint >> 31) & 0x00000001);
         midpoint <<= 1;
         ++buffer_counter;
      }
      outstream.write(reinterpret_cast<char *>(&buffer), 1);
   }

   outstream.close();
}

void decompress_file(char * in_filename, char * out_filename) {
   ifstream instream({in_filename}, ifstream::binary);
   ofstream outstream({out_filename}, ofstream::binary);
   DEBUGF('z', "decompressing file");

   if ((instream.rdstate() & ifstream::failbit) != 0) {
      cerr << "Failed to open file " << in_filename << endl;
      return;
   }

   if ((outstream.rdstate() & ofstream::failbit) != 0) {
      cerr << "Failed to open file " << out_filename << endl;
      return;
   }

   DEBUGF('z', "reading table");

   map<char, symbol_range> symbols;
   unsigned int occurences;
   char symbol;

   //also serves as the total number of characters in the original file
   unsigned int cumulative_lower_bound = 0;

   for(;;) {
      // reads symbol
      instream.read(&symbol, 1);
      // reads # of times symbol occurred. since we wrote it directly
      // from an integer, endianness should be the same and we
      // dont have to worry about it
      instream.read(reinterpret_cast<char *>(&occurences), 4);

      // table has ended
      if(occurences == 0) break;

      DEBUGF('y', "character " << static_cast<unsigned int>(symbol) << "occured " << occurences
         << " times");

      symbols[symbol] = { occurences, cumulative_lower_bound + occurences,
         cumulative_lower_bound };

      cumulative_lower_bound += occurences;
   }

   DEBUGF('y', hex << showbase);

   for(auto& x : symbols) {
      DEBUGF('y', x.first << " : " << static_cast<unsigned int>(x.first)
         << " : " << x.second.lower << " to " << x.second.upper);
   }
   DEBUGF('y', "total characters: " << cumulative_lower_bound);

   //should be 0xFFFFFFFF
   unsigned int upper_bound = numeric_limits<unsigned int>::max();
   unsigned int lower_bound = 0;
   unsigned int range;
   unsigned int encoding = 0;
   unsigned char buffer;
   unsigned char buffer_counter = 8;
   unsigned int characters_written = 0;

   // reads the first 4 bytes of the encoded file
   // to fill the encoding variable for comparison
   // cannot read directly to int because it was
   // encoded byte by byte
   for(int i = 0 ; i < 4 ; ++i) {
      instream.read((char *)(&buffer), 1);
      encoding <<= 8;
      encoding |= buffer;
   }

   for(;;) {
      range = upper_bound - lower_bound;
      DEBUGF('z', "available range: " << range);
      DEBUGF('z', "encoding: " << encoding)
      char c;
      unsigned int encoding_length = 0;

      for(auto& x : symbols) {
         // computes what the bounds would be given that x was encoded
         // and sees if the actual encoding falls within them
         unsigned int upper_given_x = lower_bound + (range / cumulative_lower_bound * x.second.upper);
         unsigned int lower_given_x = lower_bound + (range / cumulative_lower_bound * x.second.lower);

         DEBUGF('z', "     range given that " << static_cast<unsigned int>(x.first) << " occured: "
            << hex << showbase << lower_given_x << " to " << hex << showbase << upper_given_x);

         if(encoding < upper_given_x && encoding >= lower_given_x){
            c = x.first;
            --x.second.occurences;
            DEBUGF('x', "range " << upper_given_x << " to " << lower_given_x <<
                  " matches encoding " << encoding << "for char " << c);
            break;
         }
      }

      outstream.write(&c, 1);
      ++characters_written;
      if(characters_written >= cumulative_lower_bound) return;
      DEBUGF('z', characters_written << " out of " <<
         cumulative_lower_bound << " characters written");

      upper_bound = lower_bound + (range / cumulative_lower_bound * symbols[c].upper);
      lower_bound = lower_bound + (range / cumulative_lower_bound * symbols[c].lower);

      // loop should remove bits from the range that match or were pending bits,
      // since they dont tell us anything we dont already know
      for(;;) {
         DEBUGF('z', "     upper is now " << upper_bound);
         DEBUGF('z', "     encoding is now " << encoding);
         DEBUGF('z', "     lower is now " << lower_bound);
         if(lower_bound >= 0x80000000U || upper_bound < 0x80000000U) {
            // dont need to do anything besides shift out high order bits
         }
         else if(lower_bound >= 0x40000000 && upper_bound < 0xC0000000U) {
            // preserve highest order bits and discard 2nd highest order bits
            // we also need to do this for the encoding itself
            // (remove pending bits from encoding)
            lower_bound &= 0xBFFFFFFFU;
            upper_bound |= 0x40000000;
            encoding &= 0xBFFFFFFFU;
            encoding |= ((encoding >> 1) & 0x40000000);
         }
         else {
            DEBUGF('z', "     char took " << encoding_length << " bits to encode");
            break;
         }
         ++encoding_length;

         // refreshes buffer if all bits have been read
         if(buffer_counter >= 8) {
            if(instream.rdstate() & ifstream::eofbit != 0) {
               buffer = 0;
            }
            instream.read((char *)(&buffer), 1);
            buffer_counter = 0;
         }

         unsigned char current_bit = (buffer >> (7 - buffer_counter)) & 0x1;
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

int main (int argc, char** argv) {
   //maybe change to ios_base::binary to allow
   //binary files to be compressed
   scan_options(argc, argv);

   if(strcmp(argv[optind],"decode") == 0) {
      decompress_file(argv[optind + 1], argv[optind + 2]);
   }
   else if(strcmp(argv[optind],"encode") == 0) {
      compress_file(argv[optind + 1], argv[optind + 2]);
   }
   else {
      cerr << "Usage: ./arcode [- @ flag]... encode|decode \'infile\' \'outfile\'" << endl;
      return 1;
   }
}
