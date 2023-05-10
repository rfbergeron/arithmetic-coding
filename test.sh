#!/bin/sh

# https://stackoverflow.com/questions/20796200/how-to-loop-over-files-in-directory-and-change-path-and-add-suffix-to-filename
for filename in test/*; do
    [ -e "$filename" ] || continue;
    echo "$filename" | grep '\(.*\.enc\)\|\(.*\.dec\)' >/dev/null 2>&1 && continue;
    ./build/arcode encode "$filename" "$filename".enc
    ./build/arcode decode "$filename".enc "$filename".dec
    diff "$filename" "$filename".dec
done
