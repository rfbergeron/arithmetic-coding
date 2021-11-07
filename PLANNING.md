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

# Better function signatures
I'm going to attempt to ape the standard library and the way it declares its
functions.

The compress function will take its input from a `std::istream`. Its output will
be stored using an iterator. The underlying type of the iterator will be
extracted from it and the buffer will be converted appropriately.

The decompress function will take its input from an iterator. Two arguments will
be provided, corresponding to the first and last element of the data structure
that the data will be taken from. The underlying type of the iterator will be
extracted from it and converted to the type of the buffer appropriately. It will
write its output to a `std::ostream`.

```
template <class OutputIterator,
          class U = typename std::iterator_traits<OutputIterator>::value_type>
void compress_stream (std::istream &is, OutputIterator out);
template <class InputIterator,
          class U = typename std::iterator_traits<InputIterator>::value_type>
void decompress_data (InputIterator first, InputIterator last, std::ostream &os);
```

Alternatively, iterators could be used in place of streams. Streams could still
be supplied to the function through stream iterators. However, I've read on
stackoverflow that this presents its own host of issues.

# A very generic approach
A single interface could be used for both adaptive and non-adaptive models.
This would be accomplished by allowing the user to provide functions for
determining the ranges of character occurence and to update the frequency table.
