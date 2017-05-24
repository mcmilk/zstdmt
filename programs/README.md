
## Usage of the threaded compression utilities

- all utilities can be used like gzip or bzip2
- you can do also some benchmarking with the different methods
  - ```-T``` can be used to define some thread count (max is 128)
  - ```-B``` will show you the timings and RAM usage
- a just finished the testing tools, so be kindly to me, when you find errors
- do not use them for production systems yet!


### Information given by the -h option

- this is a sample output of the usage of lz4-mt
- the other ones are just the same, with some differences with in the levels

```
Usage: lz4-mt [OPTION]... [FILE]...
Compress or uncompress FILEs (by default, compress FILES in-place).

Gzip/Bzip2 Like Options:
 -#    Set compression level to # (1-12, default:3).
 -c    Force write to standard output.
 -d    Use decompress mode.
 -z    Use compress mode.
 -f    Force overwriting files and/or compression.
 -h    Display a help screen and quit.
 -k    Keep input files after compression or decompression.
 -l    List information for the specified compressed files.
 -L    Display License and quit.
 -q    Be quiet: suppress all messages.
 -S X  Use suffix `X` for compressed files. Default: ".lz4"
 -t    Test the integrity of each file leaving any files intact.
 -v    Be more verbose.
 -V    Show version information and quit.

Additional Options:
 -T N  Set number of (de)compression threads (def: #cores).
 -b N  Set input chunksize to N MiB (default: auto).
 -i N  Set number of iterations for testing (default: 1).
 -H    Print headline for the timing values and quit.
 -B    Print timings and memory usage to stderr.

With no FILE, or when FILE is -, read standard input.

Report bugs to: https://github.com/mcmilk/zstdmt/issues
```
