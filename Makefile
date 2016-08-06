
CC	= gcc

CFLAGS	= -pthread -O3 -Wall -pipe -fomit-frame-pointer
LDFLAGS	= -lzstd -lpthread

CFLAGS	+= -I./zstd/lib/common

ZSTD_FILES := common/*.c compress/*.c decompress/*.c dictBuilder/*.c

ZCOMMON	+= \
	zstd/lib/common/entropy_common.c \
	zstd/lib/common/fse_decompress.c \
	zstd/lib/common/xxhash.c \
	zstd/lib/common/zstd_common.c \
	zstd/lib/compress/fse_compress.c \
	zstd/lib/compress/huf_compress.c \
	zstd/lib/compress/zbuff_compress.c \
	zstd/lib/compress/zstd_compress.c \
	zstd/lib/decompress/huf_decompress.c \
	zstd/lib/decompress/zbuff_decompress.c \
	zstd/lib/decompress/zstd_decompress.c

#CFLAGS += -DDEBUGME
CFLAGS += -Wno-unused-but-set-variable
CFLAGS += -Wno-unused-variable
CFLAGS += -Wno-unused-function

PRGS	= zstd-st zstd-mt zstd-mt2 zstd-zstd

all:	$(PRGS)
again:	clean $(PRGS)

#zstd-xl:
#	$(CC) -pthread $(CFLAGS) zstd-zstd.c util.c $(ZCOMMON)  -o $@ 

# zstd standard version
zstd-zstd: zstd-zstd.o util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ zstd-zstd.o util.o


# single threaded testing
zstd-st: zstdmt-st.o util.o zstd-mt.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ zstd-mt.o zstdmt-st.o util.o

# multi threaded testing (simple create/join)
zstd-mt: zstdmt-mt.o util.o zstd-mt.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ zstd-mt.o zstdmt-mt.o util.o

# multi threaded testing (each thread reads/writes itself - async)
zstd-mt2: zstdmt-mt2.o util.o zstd-mt2.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ zstd-mt2.o zstdmt-mt2.o util.o

clean:
	rm -rf $(PRGS) util.o zstd-*.o zstdmt-*.o

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
