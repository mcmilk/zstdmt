
CC	= gcc
STRIP	= echo strip
#CC	= clang

CFLAGS	= -W -pthread -Wall -pipe
CFLAGS	+= -fomit-frame-pointer
CFLAGS	+= -I ../lib
LDFLAGS	= -lpthread
# -static

#CFLAGS += -DDEBUGME
CFLAGS += -g
CFLAGS += -O
#CFLAGS += -march=native
#CFLAGS += -Wno-unused-but-set-variable
#CFLAGS += -Wno-unused-variable
#CFLAGS += -Wno-unused-function

PRGS	= zstdmt lz4mt lz5mt

COMMON	= util.c
LIBLZ4	= ../lib/threading.c ../lib/lz4mt_common.c ../lib/lz4mt_compress.c ../lib/lz4mt_decompress.c
LIBLZ5	= ../lib/threading.c ../lib/lz5mt_common.c ../lib/lz5mt_compress.c ../lib/lz5mt_decompress.c
LIBZSTD	= ../lib/threading.c ../lib/zstdmt_common.c ../lib/zstdmt_compress.c ../lib/zstdmt_decompress.c

all:	$(PRGS)
again:	clean $(PRGS)

lz4mt:
	$(CC) $(CFLAGS) $(LDFLAGS) -llz4 -o $@ lz4mt.c $(LIBLZ4) $(COMMON)
	$(STRIP) $@

lz5mt:
	$(CC) $(CFLAGS) $(LDFLAGS) -llz5 -o $@ lz5mt.c $(LIBLZ5) $(COMMON)
	$(STRIP) $@

zstdmt:
	$(CC) $(CFLAGS) $(LDFLAGS) -lzstd -o $@ zstdmt.c $(LIBZSTD) $(COMMON)
	$(STRIP) $@

clean:
	rm -rf $(PRGS)

.c.c:
	$(CC) $(CFLAGS) -c $< -o $@
