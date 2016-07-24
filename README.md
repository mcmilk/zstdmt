
# zstdmt

zstdmt depends currently on posix threads for multi-threading

## Overview

 - three tools:
   - `./zstd-st` - single threaded multi stram mode
   - `./zstd-mt` - multi threaded via phread_create / pthread_join
   - `./zstd-tp` - multi threaded via thread pool

## Building for Linux with gcc

 - Run `make`.
 - `./zstd-st`, `./zstd-mt` and `./zstd-tp` will be created.

## Building for Linux with Clang

 - Run `make CC=clang`
 - `./zstd-st`, `./zstd-mt` and `./zstd-tp` will be created.

## See also

 - [zstd Extremely Fast Compression algorithm](https://github.com/Cyan4973/zstd)
 - [7-Zip with zstd support](https://github.com/mcmilk/7-Zip-Zstd
