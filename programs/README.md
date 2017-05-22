
## Usage of the testing utilities

### [LZ4](https://github.com/lz4/lz4)
```
Usage: lz4mt [options] INPUT > FILE
or     lz4mt [options] -o FILE INPUT
or     cat INPUT | lz4mt [options] -o FILE
or     cat INPUT | lz4mt [options] > FILE

Options:
 -o FILE write result to a file named `FILE`
 -#      set compression level to # (1-12, default:1)
 -T N    set number of (de)compression threads (def: #cores)
 -i N    set number of iterations for testing (default: 1)
 -b N    set input chunksize to N KiB (default: auto)
 -c      compress (default mode)
 -d      use decompress mode
 -t      print timings and memory usage to stderr
 -H      print headline for the timing values
 -h      show usage
 -v      show version
```


### [LZ5](https://github.com/inikep/lz5)
```
Usage: lz5mt [options] INPUT > FILE
or     lz5mt [options] -o FILE INPUT
or     cat INPUT | lz5mt [options] -o FILE
or     cat INPUT | lz5mt [options] > FILE

Options:
 -o FILE write result to a file named `FILE`
 -#      set compression level to # (1-15, default:1)
 -T N    set number of (de)compression threads (def: #cores)
 -i N    set number of iterations for testing (default: 1)
 -b N    set input chunksize to N KiB (default: auto)
 -c      compress (default mode)
 -d      use decompress mode
 -t      print timings and memory usage to stderr
 -H      print headline for the timing values
 -h      show usage
 -v      show version
```

### [ZStandard](https://github.com/facebook/zstd)
```
Usage: zstdmt [options] INPUT > FILE
or     zstdmt [options] -o FILE INPUT
or     cat INPUT | zstdmt [options] -o FILE
or     cat INPUT | zstdmt [options] > FILE

Options:
 -o FILE write result to a file named `FILE`
 -#      set compression level to # (1-22, default:3)
 -T N    set number of (de)compression threads (def: #cores)
 -i N    set number of iterations for testing (default: 1)
 -b N    set input chunksize to N KiB (default: auto)
 -c      compress (default mode)
 -d      use decompress mode
 -t      print timings and memory usage to stderr
 -H      print headline for the timing values
 -h      show usage
 -v      show version
```

### [Lizard](https://github.com/inikep/lizard)
```
Usage: lizardmt [options] INPUT > FILE
or     lizardmt [options] -o FILE INPUT
or     cat INPUT | lizardmt [options] -o FILE
or     cat INPUT | lizardmt [options] > FILE

Options:
 -o FILE write result to a file named `FILE`
 -#      set compression level to # (1-10, default:1)
 -T N    set number of (de)compression threads (def: #cores)
 -i N    set number of iterations for testing (default: 1)
 -b N    set input chunksize to N KiB (default: auto)
 -c      compress (default mode)
 -d      use decompress mode
 -t      print timings and memory usage to stderr
 -H      print headline for the timing values
 -h      show usage
 -v      show version

Method options:
 -M N    use method M of lizard (default:1)
    1:   fastLZ4: give better decompression speed than LZ4
    2:   LIZv1: give better ratio than LZ4 keeping 75% decompression speed
    3:   fastLZ4 + Huffman: add Huffman coding to fastLZ4
    4:   LIZv1 + Huffman: add Huffman coding to LIZv1
```
