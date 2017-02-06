
## Usage of zstdmt

- lz4mt and lz5mt have nearly the same usage (levels are different)

```
Usage: zstdmt [options] > FILE
or     cat INPUT | zstdmt [options] > FILE
or     cat INPUT | zstdmt [options] -o FILE

Options:
 -o FILE write result to a file named `FILE`
 -l N    set level of compression (default: 3)
 -t N    set number of (de)compression threads (def: #cores)
 -i N    set number of iterations for testing (default: 1)
 -b N    set input chunksize to N KiB (default: auto)
 -c      compress (default mode)
 -d      use decompress mode
 -T      print timings and memory usage to stderr
 -H      print headline for the timing values
 -h      show usage
 -v      show version
```
