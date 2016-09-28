
## Usage of the Testutils @ Windows

### [LZ4]
```
Usage: lz4mt [options] infile outfile

Otions:
 -l N    set level (1..16) of compression (default: 1)
 -t N    set number of (de)compression threads (default: 2)
 -i N    set number of iterations for testing (default: 1)
 -b N    set input chunksize to N MiB (default: auto)
 -c      compress (default mode)
 -d      use decompress mode
 -h      show usage
 -v      show version
```

### [LZ5]
```
Usage: lz5mt [options] infile outfile

Otions:
 -l N    set level (1..16) of compression (default: 1)
 -t N    set number of (de)compression threads (default: 2)
 -i N    set number of iterations for testing (default: 1)
 -b N    set input chunksize to N MiB (default: auto)
 -c      compress (default mode)
 -d      use decompress mode
 -h      show usage
 -v      show version
```

### [ZStandard]
```
Usage: zstdmt [options] infile outfile

Otions:
 -l N    set level of compression (default: 3)
 -t N    set number of (de)compression threads (default: 2)
 -i N    set number of iterations for testing (default: 1)
 -b N    set input chunksize to N MiB (default: auto)
 -c      compress (default mode)
 -d      use decompress mode
 -h      show usage
 -v      show version
```
