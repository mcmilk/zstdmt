
# Multithreading Library for [LZ4], [LZ5] and [ZStandard]

## description
- does the same as the pzstd from the contrib directory of ZStandard
- works with skippables frame id 0x184D2A50 (12 bytes per compressed frame)
- three small test utils, which only work @ linux
- the library itself works on windows already (multithreaded 7-Zip ZS is currently in testing)

## zstdmt, usage
```
Usage: zstdmt [options] infile outfile

Options:
 -l N    set level of compression (default: 3)
 -t N    set number of (de)compression threads (default: 2)
 -i N    set number of iterations for testing (default: 1)
 -b N    set input chunksize to N MiB (default: auto)
 -c      compress (default mode)
 -d      use decompress mode (XXX, not done)
 -H      print headline for the testing values
 -h      show usage
 -v      show version
```

## lz4mt, usage
```
Usage: lz4mt [options] infile outfile

Otions:
 -l N    set level of compression (default: 1)
 -t N    set number of (de)compression threads (default: 2)
 -i N    set number of iterations for testing (default: 1)
 -b N    set input chunksize to N MiB (default: auto)
 -c      compress (default mode)
 -d      use decompress mode
 -H      print headline for the testing values
 -h      show usage
 -v      show version
```

## lz5mt, usage
```
Usage: lz5mt [options] infile outfile

Otions:
 -l N    set level of compression (default: 1)
 -t N    set number of (de)compression threads (default: 2)
 -i N    set number of iterations for testing (default: 1)
 -b N    set input chunksize to N MiB (default: auto)
 -c      compress (default mode)
 -d      use decompress mode
 -H      print headline for the testing values
 -h      show usage
 -v      show version
```

[LZ4]:https://cyan4973.github.io/lz4/
[LZ5]:https://github.com/inikep/lz5
[ZStandard]:http://facebook.github.io/zstd/
