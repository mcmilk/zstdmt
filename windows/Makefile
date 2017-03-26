
#CROSS	= i686-w64-mingw32-
CROSS	= x86_64-w64-mingw32-
CC	= $(CROSS)gcc
RANLIB	= $(CROSS)ranlib

STRIP	= $(CROSS)strip
SFLAGS	= -R .note -R .comment

LDFLAGS	= -lpthread
CFLAGS	= -W -pthread -Wall -pipe -flto
CFLAGS	+= -fomit-frame-pointer

#CFLAGS += -DDEBUGME
#CFLAGS += -g
CFLAGS += -O3
#CFLAGS += -march=native
#CFLAGS += -Wno-unused-but-set-variable
#CFLAGS += -Wno-unused-variable
#CFLAGS += -Wno-unused-function

PRGS	= lz4mt.exe lz5mt.exe zstdmt.exe lizardmt.exe

LZ4_VER	= "v1.7.5"
LZ5_VER	= "v1.5"
ZSTD_VER= "v1.1.4"
LIZ_VER	= "v2.0"

all:	loadsource $(PRGS)
again:	clean $(PRGS)

ZSTDMTDIR = ../lib

LIBLZ4	= $(ZSTDMTDIR)/threading.c $(ZSTDMTDIR)/lz4mt_common.c $(ZSTDMTDIR)/lz4mt_compress.c $(ZSTDMTDIR)/lz4mt_decompress.c lz4mt.c
LIBLZ5	= $(ZSTDMTDIR)/threading.c $(ZSTDMTDIR)/lz5mt_common.c $(ZSTDMTDIR)/lz5mt_compress.c $(ZSTDMTDIR)/lz5mt_decompress.c lz5mt.c
LIBZSTD	= $(ZSTDMTDIR)/threading.c $(ZSTDMTDIR)/zstdmt_common.c $(ZSTDMTDIR)/zstdmt_compress.c $(ZSTDMTDIR)/zstdmt_decompress.c zstdmt.c
LIBLIZ	= $(ZSTDMTDIR)/threading.c $(ZSTDMTDIR)/lizardmt_common.c $(ZSTDMTDIR)/lizardmt_compress.c $(ZSTDMTDIR)/lizardmt_decompress.c lizardmt.c

LZ4DIR	= lz4/lib
LIBLZ4	+= $(LZ4DIR)/lz4.c $(LZ4DIR)/lz4frame.c $(LZ4DIR)/lz4hc.c $(LZ4DIR)/xxhash.c
CF_LZ4	= $(CFLAGS) -Ilz4/lib

LZ5DIR	= lz5/lib
LIBLZ5	+= $(LZ5DIR)/lz5.c $(LZ5DIR)/lz5frame.c $(LZ5DIR)/lz5hc.c $(LZ5DIR)/xxhash.c
CF_LZ5	= $(CFLAGS) -Ilz5/lib

ZSTDDIR	= zstd/lib
LIBZSTD	+= $(ZSTDDIR)/common/entropy_common.c $(ZSTDDIR)/common/fse_decompress.c \
	  $(ZSTDDIR)/common/xxhash.c $(ZSTDDIR)/common/zstd_common.c \
	  $(ZSTDDIR)/common/error_private.c $(ZSTDDIR)/compress/fse_compress.c \
	  $(ZSTDDIR)/compress/huf_compress.c $(ZSTDDIR)/compress/zstd_compress.c \
	  $(ZSTDDIR)/decompress/huf_decompress.c $(ZSTDDIR)/decompress/zstd_decompress.c \
	  $(ZSTDDIR)/legacy/zstd_v01.c $(ZSTDDIR)/legacy/zstd_v02.c \
	  $(ZSTDDIR)/legacy/zstd_v03.c $(ZSTDDIR)/legacy/zstd_v04.c \
	  $(ZSTDDIR)/legacy/zstd_v05.c $(ZSTDDIR)/legacy/zstd_v06.c \
	  $(ZSTDDIR)/legacy/zstd_v07.c
CF_ZSTD	= $(CFLAGS) -I$(ZSTDDIR) -I$(ZSTDDIR)/common -I$(ZSTDDIR)/compress -I$(ZSTDDIR)/decompress -I$(ZSTDDIR)/legacy

LIZDIR	= lizard/lib
LIBLIZ	+= $(LIZDIR)/lz5_compress.c $(LIZDIR)/lz5_decompress.c $(LIZDIR)/lz5frame.c \
	  $(LIZDIR)/xxhash/xxhash.c \
	  $(LIZDIR)/entropy/entropy_common.c $(LIZDIR)/entropy/fse_compress.c \
	  $(LIZDIR)/entropy/fse_decompress.c $(LIZDIR)/entropy/huf_compress.c \
	  $(LIZDIR)/entropy/huf_decompress.c
CF_LIZ	= $(CFLAGS) -Ilizard/lib

CFLAGS	+= -I. -I$(ZSTDMTDIR)

loadsource:
	test -d lz4    || git clone https://github.com/Cyan4973/lz4  -b $(LZ4_VER)  --depth=1 lz4
	test -d lz5    || git clone https://github.com/inikep/lz5    -b $(LZ5_VER)  --depth=1 lz5
	test -d zstd   || git clone https://github.com/facebook/zstd -b $(ZSTD_VER) --depth=1 zstd
	test -d lizard || git clone https://github.com/inikep/lizard -b $(LIZ_VER)  --depth=1 lizard

lz4mt.exe:
	$(CC) $(CF_LZ4) $(CDFLAGS) $(LDFLAGS) -DVERSION='$(LZ4_VER)' -o $@ $(LIBLZ4)
	$(STRIP) $(SFLAGS) $@

lz5mt.exe:
	$(CC) $(CF_LZ5) $(CDFLAGS) $(LDFLAGS) -DVERSION='$(LZ5_VER)' -o $@ $(LIBLZ5)
	$(STRIP) $(SFLAGS) $@

zstdmt.exe:
	$(CC) $(CF_ZSTD) $(CDFLAGS) $(LDFLAGS) -DVERSION='$(ZSTD_VER)' -o $@ $(LIBZSTD)
	$(STRIP) $(SFLAGS) $@

lizardmt.exe:
	$(CC) $(CF_LIZ) $(CDFLAGS) $(LDFLAGS) -DVERSION='$(LIZ_VER)' -o $@ $(LIBLIZ)
	$(STRIP) $(SFLAGS) $@

clean:
	rm -f $(PRGS) *.o *.a

mrproper: clean
	rm -rf lz4 lz5 zstd lizard
