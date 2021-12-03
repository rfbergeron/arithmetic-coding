# Conversion to function templates
Although we are going to require that the user provide the algorithm a character
stream as input, we want the output to be flexible. With that end, the function
will be templated on an output iterator and the underlying type of that
iterator. As an interim solution, the bounds for characters and the encoding
buffer will be of type `uintmax_t`, since that is the type that the STL
filesystem library uses to store the length of files.

When writing this, I'm primarily going to be targeting 64-bit systems. Main
concern there would be little vs big-endian. 32-bit support could also be
possible, but I'm not sure it would be necessary to explicitly implement it
since `uintmax_t` on x86 should also be 64 bits. Not sure about 32-bit ARM,
which would be the other main 32-bit platform to be concerned about.

# Single pass approach
The existing implementation has the following problems:
1. Two passes are required, which means that standard input cannot be encoded
   without the input being buffered first.
2. We can separate out the reading, building, and writing of the map out into
   separate functions and make the user responsible for buffering data if
   necessary. However, this means that the user may write the map to another
   file instead of two the same file, so the functions responsible for reading
   the encoding and table need to account for this.

We can fix both of these problems by encoding the data in a single pass. This
is accomplished by having each character's probability start out equally, and
incrementing the number of occurrences each time a character is read. To ensure
that all characters start with equal probability and that the algorithm
operates cleanly, each character will default to having an occrrence of one.

The occurrence count increment occurs after the character has been encoded, so
that the operation can be mirrored on the decoder's side, since it cannot know
beforehand what was encoded.

This means that the unadjusted ranges for each character cannot be pulled from
a map each time. They must be recalculated each time a character is encoded or
decoded.

# Improved two pass approach
Since there is no guarantee that we will be able to seek a stream to read its
contents multiple times, the user should be required to explicitly build the
map before encoding or decoding the stream.

The two pass encode and decode methods will not write the table to the output;
the user must also perform another call to do that, to allow tables to be stored
separately from the data.

To make it easy to determine whether or not a file has been encoded with a table
prepended to it, we will need to do the following:
1. Have a "Beginning of encoding" marker, which will be a sequence of bytes.
2. Have an explicit "End of encoding" character, which receives its own table
   entry.
3. Have a "End of table" marker, so that the table reader does not interpret any
   of the encoded data as table entries.
4. Optionally, have a "Beginning of table" marker. This could also just be a
   magic number, and the presence of a table in the stream determined by 

# Two pass dynamic approach
This approach is the same as above, except that instead of using static values
when encoding the data, after encoding each character, the occurrenc count for
that character is decremented, so that the probability ranges are exact at each
location in the stream.

This scheme may need a different function, since ideally in the static two pass
approach, the map argument would be a constant reference to avoid copies. That
wouldn't be possible here, since we must modify the map. Alternatively, this
scheme could construct a second map that holds the number of each character that
has already been encoded, which is subtracted from the original character map.

# Understanding stream iterators
Stream iterators require a single template argument `T`, which is the type that
the contents of the stream will be converted to (for `istreams`) or from (for
`ostreams`). So, dereferencing an `istream_iterator` will perform the conversion
`charT -> T` for as many valid characters as possible, and dereferencing and
assigning to an `istream_iterator` will convert an object of type `T` to a
character sequence and write the sequence to the stream.

# Working with more than just files
TODO

# Better function signatures
Unlike STL functions, which largely use iterators for function parameters, it
would make more sense to use streams here, since we are primarily concerned with
reading and writing text and binary files. The flexibility we would be concerned
with would be working with streams that have different `charT` types. However,
since we are working with binary data, the compressed stream should only have
a `charT` of type `char`.

The compress and decompress functions would then be templated on the `charT`
type of the plaintext, and the type of unsigned integer to be used as a buffer.

If the user wishes to compress arbitary types (custom data structures), the user
would first need to convert it to a binary or text format that can be streamed.
This is surprisingly not that difficult: you can cast anything to `char*`,
construct a string using this value, and then construct a `streamstream` using
the string. This works, but causes many extraneous copies. Alternatively, some
implementations allow you to set the buffer of a `stringstream` directly, so
all you'd have to do is create a `stringstream`, cast the value to `char*`, and
set the stream's buffer to this value.

# Bit representations of different widths on systems with different endianness
## Big endian
16 bit: `0x1100`
32 bit: `0x33221100`
64 bit: `0x7766554433221100`
## Little endian
16 bit: `0x0011`
32 bit: `0x00112233`
64 bit: `0x0011223344556677`
