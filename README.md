
# zstdmt

zstdmt depends currently on posix threads for multi-threading and is
only tested on linux

I will add support for windows and CreateThread() together with
WaitForMultipleObjects() / WaitForSingleObject() later...

## Building for Linux with gcc

 - Run `make`.

## Building for Linux with Clang

 - Run `make CC=clang`

## Overview

 - three tools:
   - `./zstd-st` - single threaded multi stram mode
   - `./zstd-mt` - multi threaded via phread_create / pthread_join
   - `./zstd-tp` - multi threaded via thread pool

## See also

 - [zstd Extremely Fast Compression algorithm](https://github.com/Cyan4973/zstd)
 - [7-Zip with zstd support](https://github.com/mcmilk/7-Zip-Zstd)
