
CROSS	= i686-w64-mingw32-
CC	= $(CROSS)gcc
STRIP	= $(CROSS)strip
RANLIB	= $(CROSS)ranlib
#CC	= clang

CFLAGS	= -W -pthread -Wall -pipe
CFLAGS	= -fomit-frame-pointer
LDFLAGS	= -lpthread

#CFLAGS += -DDEBUGME
#CFLAGS += -g
CFLAGS += -O3
#CFLAGS += -march=native
#CFLAGS += -Wno-unused-but-set-variable
#CFLAGS += -Wno-unused-variable
#CFLAGS += -Wno-unused-function

PRGS	= lz4mt lz5mt zstdmt

all:	loadsource $(PRGS)
again:	clean $(PRGS)

ZSTDMTDIR = ../lib
CFLAGS	+= -I. -I$(ZSTDMTDIR)

LIBLZ4	= $(ZSTDMTDIR)/threading.c $(ZSTDMTDIR)/lz4mt_common.c $(ZSTDMTDIR)/lz4mt_compress.c $(ZSTDMTDIR)/lz4mt_decompress.c
LIBLZ5	= $(ZSTDMTDIR)/threading.c $(ZSTDMTDIR)/lz5mt_common.c $(ZSTDMTDIR)/lz5mt_compress.c $(ZSTDMTDIR)/lz5mt_decompress.c
LIBZSTD	= $(ZSTDMTDIR)/threading.c $(ZSTDMTDIR)/zstdmt_common.c $(ZSTDMTDIR)/zstdmt_compress.c $(ZSTDMTDIR)/zstdmt_decompress.c

LZ4DIR	= lz4/lib
LIBLZ4	+= $(LZ4DIR)/lz4.c $(LZ4DIR)/lz4frame.c $(LZ4DIR)/lz4hc.c $(LZ4DIR)/xxhash.c
CF_LZ4	+= $(CFLAGS) -Ilz4/lib

LZ5DIR	= lz5/lib
LIBLZ5	+= $(LZ5DIR)/lz5.c $(LZ5DIR)/lz5frame.c $(LZ5DIR)/lz5hc.c $(LZ5DIR)/xxhash.c
CF_LZ5	+= $(CFLAGS) -Ilz5/lib

ZSTDDIR    = zstd/lib
ZSTDCOMMON = $(ZSTDDIR)/common
LIBZSTD   += $(ZSTDCOMMON)/entropy_common.c  $(ZSTDCOMMON)/fse_decompress.c  $(ZSTDCOMMON)/xxhash.c  $(ZSTDCOMMON)/zstd_common.c  $(ZSTDCOMMON)/error_private.c
ZSTDCOMP   = $(ZSTDDIR)/compress
LIBZSTD   += $(ZSTDCOMP)/fse_compress.c  $(ZSTDCOMP)/huf_compress.c  $(ZSTDCOMP)/zbuff_compress.c  $(ZSTDCOMP)/zstd_compress.c
ZSTDDECOMP = $(ZSTDDIR)/decompress
LIBZSTD   += $(ZSTDDECOMP)/huf_decompress.c   $(ZSTDDECOMP)/zstd_decompress.c
ZSTDLEGACY = $(ZSTDDIR)/legacy
LIBZSTD   += $(ZSTDLEGACY)/zstd_v05.c  $(ZSTDLEGACY)/zstd_v06.c  $(ZSTDLEGACY)/zstd_v07.c
CF_ZSTD	+= $(CFLAGS) -I$(ZSTDDIR) -I$(ZSTDCOMMON) -I$(ZSTDCOMP) -I$(ZSTDDECOMP) -I$(ZSTDLEGACY)

loadsource:
	test -d lz4  || git clone https://github.com/Cyan4973/lz4  -b v1.7.3 --depth=1 lz4
	test -d lz5  || git clone https://github.com/inikep/lz5    -b v1.5   --depth=1 lz5
	test -d zstd || git clone https://github.com/facebook/zstd -b v1.1.1 --depth=1 zstd

lz4mt: lz4mt.o
	$(CC) $(CF_LZ4) $(LDFLAGS) -o $@ $(LIBLZ4) lz4mt.o
	$(STRIP) $@

lz5mt: lz5mt.o
	$(CC) $(CF_LZ5) $(LDFLAGS) -o $@ $(LIBLZ5) lz5mt.o
	$(STRIP) $@

zstdmt: zstdmt.o
	$(CC) $(CF_ZSTD) $(LDFLAGS) -o $@ $(LIBZSTD) zstdmt.o
	$(STRIP) $@

clean:
	rm -f $(PRGS) *.o *.a
