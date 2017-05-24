
# Multithreading Library for [Brotli], [Lizard], [LZ4], [LZ5] and [ZStandard]

## Description
- works with skippables frame id 0x184D2A50 (12 bytes per compressed frame)
- brotli is supported the same way, it will encapsulate the real brotli stream
  within an 16 byte frame header
- the frame header for brotli is defined like this:

size    | value             | description
--------|-------------------|------------
4 bytes | 0x184D2A50U       | magic for skippable frame (like zstd)
4 bytes | 4                 | size of skippable frame (4)
4 bytes | compressed size   | size of the following frame (compressed data)
2 bytes | 0x5242U           | magic for brotli "BR"
2 bytes | uncompressed size | allocation hint for decompressor (64KB * this size here)

## Usage of the Testutils
- see programs

## Usage of the Library
- see lib

[Brotli]:https://github.com/google/brotli/
[LZ4]:https://cyan4973.github.io/lz4/
[LZ5]:https://github.com/inikep/lz5/
[ZStandard]:http://facebook.github.io/zstd/
[Lizard]:https://github.com/inikep/lizard/


/TR 2017-03-26
