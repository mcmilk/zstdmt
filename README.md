
# zstdmt, lz4mt and lz5mt

## zstdmt, description

- does the same as the pzstd from the contrib directory of ZStandard
- works with skippables frame id 0x184D2A50 (12 bytes per compressed frame)

## zstdmt, usage

'''
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
'''
