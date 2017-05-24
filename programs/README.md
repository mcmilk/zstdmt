
## Usage of the threaded compression utilities

- all utilities can be used like gzip or bzip2
- you can do also some benchmarking with the different methods
  - ```-T``` can be used to define some thread count (max is 128)
  - ```-B``` will show you the timings and RAM usage
- a just finished the testing tools, so be kindly to me, when you find errors
- do not use them for production systems yet!


### Information given by the help option

- this is a sample output of the usage of brotli-mt
- the other ones are just the same, with some differences with in the levels

```
 Usage: brotli-mt [OPTION]... [FILE]...
 Compress or uncompress FILEs (by default, compress FILES in-place).

 Standard Options:
  -#    Set compression level to # (0-11, default:3).
  -c    Force write to standard output.
  -d    Use decompress mode.
  -z    Use compress mode.
  -f    Force overwriting files and/or compression.
  -o F  Write output to file `F`, stdout is used for `-`.
  -h    Display a help screen and quit.
  -k    Keep input files after compression or decompression.
  -l    List information for the specified compressed files.
  -L    Display License and quit.
  -q    Be quiet: suppress all messages.
  -S X  Use suffix 'X' for compressed files. Default: ".brot"
  -t    Test the integrity of each file leaving any files intact.
  -v    Be more verbose.
  -V    Show version information and quit.

 Additional Options:
  -T N  Set number of (de)compression threads (def: #cores).
  -b N  Set input chunksize to N MiB (default: auto).
  -i N  Set number of iterations for testing (default: 1).
  -B    Print timings and memory usage to stderr.
  -C    Disable crc32 calculation in verbose listing mode.

 If invoked as 'brotli-mt', default action is to compress.
             as 'unbrotli-mt',  default action is to decompress.
             as 'brotlicat-mt', then: force decompress to stdout.

 With no FILE, or when FILE is -, read standard input.

 Report bugs to: https://github.com/mcmilk/zstdmt/issues
```
