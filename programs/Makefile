##############################################################################
# GNU Makefile for zstmt
# /TR 2024-01-07
##############################################################################

# current versions we're using (tagged in their main repo)
BRO_VER	= "v1.1.0"
LIZ_VER	= "v1.0"
LZ4_VER	= "v1.9.4"
LZ5_VER	= "v1.5"
ZSTD_VER= "v1.5.5"
SNAP_VER= "v0.0.0"
LZFSE_VER="lzfse-1.0"
# uncomment, for cross compiling or windows binaries
#WIN_LDFLAGS	= -lwinmm -lpsapi
#CROSS		= i686-w64-mingw32-
#EXTENSION	= -w32.exe
#CROSS		= x86_64-w64-mingw32-
#EXTENSION	= -w64.exe
#LN	= @echo

LN	= ln -sf
#CC	= $(CROSS)musl-gcc
#CC	= $(CROSS)clang
CC	= $(CROSS)gcc
RANLIB	= $(CROSS)ranlib
STRIP	= $(CROSS)strip
SFLAGS	= -R .note -R .comment
#SFLAGS	= -R .note .comment

CFLAGS	= -W -pthread -Wall -pipe
CFLAGS	+= -fomit-frame-pointer
CFLAGS	+= -Wno-implicit-fallthrough
CFLAGS	+= -O3
#CFLAGS	+= -flto
#CFLAGS += -DDEBUGME
#CFLAGS += -g
#CFLAGS += -march=native
LDFLAGS	= $(WIN_LDFLAGS) -lpthread

PRGS	= lizard-mt$(EXTENSION) \
	  lz4-mt$(EXTENSION) \
	  lz5-mt$(EXTENSION) \
	  brotli-mt$(EXTENSION) \
	  zstd-mt$(EXTENSION) \
	  snappy-mt$(EXTENSION) \
	  lzfse-mt$(EXTENSION) 

all:	loadsource $(PRGS)
again:	clean $(PRGS)

ZSTDMTDIR = ../lib
COMMON	= platform.c $(ZSTDMTDIR)/threading.c

LIBBRO	= $(COMMON) $(ZSTDMTDIR)/brotli-mt_common.c $(ZSTDMTDIR)/brotli-mt_compress.c \
	  $(ZSTDMTDIR)/brotli-mt_decompress.c brotli-mt.c
LIBLIZ	= $(COMMON) $(ZSTDMTDIR)/lizard-mt_common.c $(ZSTDMTDIR)/lizard-mt_compress.c \
	  $(ZSTDMTDIR)/lizard-mt_decompress.c lizard-mt.c
LIBLZ4	= $(COMMON) $(ZSTDMTDIR)/lz4-mt_common.c $(ZSTDMTDIR)/lz4-mt_compress.c \
	  $(ZSTDMTDIR)/lz4-mt_decompress.c lz4-mt.c
LIBLZ5	= $(COMMON) $(ZSTDMTDIR)/lz5-mt_common.c $(ZSTDMTDIR)/lz5-mt_compress.c \
	  $(ZSTDMTDIR)/lz5-mt_decompress.c lz5-mt.c
LIBZSTD	= $(COMMON) $(ZSTDMTDIR)/zstd-mt_common.c $(ZSTDMTDIR)/zstd-mt_compress.c \
	  $(ZSTDMTDIR)/zstd-mt_decompress.c zstd-mt.c
LIBSNAP	= $(COMMON) $(ZSTDMTDIR)/snappy-mt_common.c $(ZSTDMTDIR)/snappy-mt_compress.c \
	  $(ZSTDMTDIR)/snappy-mt_decompress.c snappy-mt.c
LIBLZFSE = $(COMMON) $(ZSTDMTDIR)/lzfse-mt_common.c $(ZSTDMTDIR)/lzfse-mt_compress.c \
	  $(ZSTDMTDIR)/lzfse-mt_decompress.c lzfse-mt.c

# Brotli, https://github.com/google/brotli
BRODIR	= brotli/c
LIBBRO	+= \
	  $(BRODIR)/common/constants.c \
	  $(BRODIR)/common/context.c \
	  $(BRODIR)/common/dictionary.c \
	  $(BRODIR)/common/platform.c \
	  $(BRODIR)/common/shared_dictionary.c \
	  $(BRODIR)/common/transform.c \
	  $(BRODIR)/dec/bit_reader.c \
	  $(BRODIR)/dec/decode.c \
	  $(BRODIR)/dec/huffman.c \
	  $(BRODIR)/dec/state.c \
	  $(BRODIR)/enc/backward_references.c \
	  $(BRODIR)/enc/backward_references_hq.c \
	  $(BRODIR)/enc/bit_cost.c \
	  $(BRODIR)/enc/block_splitter.c \
	  $(BRODIR)/enc/brotli_bit_stream.c \
	  $(BRODIR)/enc/cluster.c \
	  $(BRODIR)/enc/command.c \
	  $(BRODIR)/enc/compound_dictionary.c \
	  $(BRODIR)/enc/compress_fragment.c \
	  $(BRODIR)/enc/compress_fragment_two_pass.c \
	  $(BRODIR)/enc/dictionary_hash.c \
	  $(BRODIR)/enc/encode.c \
	  $(BRODIR)/enc/encoder_dict.c \
	  $(BRODIR)/enc/entropy_encode.c \
	  $(BRODIR)/enc/fast_log.c \
	  $(BRODIR)/enc/histogram.c \
	  $(BRODIR)/enc/literal_cost.c \
	  $(BRODIR)/enc/memory.c \
	  $(BRODIR)/enc/metablock.c \
	  $(BRODIR)/enc/static_dict.c \
	  $(BRODIR)/enc/utf8_util.c
CF_BRO	= $(CFLAGS) -I$(BRODIR)/include


# LZ4, https://github.com/Cyan4973/lz4
LZ4DIR	= lz4/lib
LIBLZ4	+= $(LZ4DIR)/lz4.c $(LZ4DIR)/lz4frame.c $(LZ4DIR)/lz4hc.c $(LZ4DIR)/xxhash.c
CF_LZ4	= $(CFLAGS) -Ilz4/lib

# LZ5, https://github.com/inikep/lz5
LZ5DIR	= lz5/lib
LIBLZ5	+= $(LZ5DIR)/lz5.c $(LZ5DIR)/lz5frame.c $(LZ5DIR)/lz5hc.c $(LZ5DIR)/xxhash.c
CF_LZ5	= $(CFLAGS) -Ilz5/lib

# Lizard, https://github.com/inikep/lizard
LIZDIR	= lizard/lib
LIBLIZ	+= $(LIZDIR)/lizard_compress.c \
	  $(LIZDIR)/lizard_decompress.c \
	  $(LIZDIR)/lizard_frame.c \
	  $(LIZDIR)/xxhash/xxhash.c \
	  $(LIZDIR)/entropy/entropy_common.c \
	  $(LIZDIR)/entropy/fse_compress.c \
	  $(LIZDIR)/entropy/fse_decompress.c \
	  $(LIZDIR)/entropy/huf_compress.c \
	  $(LIZDIR)/entropy/huf_decompress.c
CF_LIZ	= $(CFLAGS) -Ilizard/lib

# Zstandard, https://github.com/facebook/zstd
ZSTDDIR	= zstd/lib
LIBZSTD	+= $(ZSTDDIR)/common/debug.c \
	  $(ZSTDDIR)/common/entropy_common.c \
	  $(ZSTDDIR)/common/error_private.c \
	  $(ZSTDDIR)/common/fse_decompress.c \
	  $(ZSTDDIR)/common/pool.c \
	  $(ZSTDDIR)/common/threading.c \
	  $(ZSTDDIR)/common/xxhash.c \
	  $(ZSTDDIR)/common/zstd_common.c \
	  $(ZSTDDIR)/compress/fse_compress.c \
	  $(ZSTDDIR)/compress/hist.c \
	  $(ZSTDDIR)/compress/huf_compress.c \
	  $(ZSTDDIR)/compress/zstd_compress.c \
	  $(ZSTDDIR)/compress/zstd_double_fast.c \
	  $(ZSTDDIR)/compress/zstd_fast.c \
	  $(ZSTDDIR)/compress/zstd_lazy.c \
	  $(ZSTDDIR)/compress/zstd_ldm.c \
	  $(ZSTDDIR)/compress/zstd_opt.c \
	  $(ZSTDDIR)/compress/zstd_compress_sequences.c \
	  $(ZSTDDIR)/compress/zstd_compress_superblock.c \
	  $(ZSTDDIR)/compress/zstd_compress_literals.c \
	  $(ZSTDDIR)/decompress/huf_decompress.c \
	  $(ZSTDDIR)/decompress/zstd_ddict.c \
	  $(ZSTDDIR)/decompress/zstd_decompress.c \
	  $(ZSTDDIR)/decompress/zstd_decompress_block.c \
	  $(ZSTDDIR)/decompress/huf_decompress_amd64.S \
	  $(ZSTDDIR)/legacy/zstd_v01.c $(ZSTDDIR)/legacy/zstd_v02.c \
	  $(ZSTDDIR)/legacy/zstd_v03.c $(ZSTDDIR)/legacy/zstd_v04.c \
	  $(ZSTDDIR)/legacy/zstd_v05.c $(ZSTDDIR)/legacy/zstd_v06.c \
	  $(ZSTDDIR)/legacy/zstd_v07.c
CF_ZSTD	= $(CFLAGS) -I$(ZSTDDIR) -I$(ZSTDDIR)/common -I$(ZSTDDIR)/compress \
	  -I$(ZSTDDIR)/decompress -I$(ZSTDDIR)/legacy

# snappy-c, https://github.com/andikleen/snappy-c
SNAPDIR	= snappy
LIBSNAP	+= $(SNAPDIR)/snappy.c
CF_SNAP	= $(CFLAGS) -I$(SNAPDIR)

# lzfse, https://github.com/lzfse/lzfse
LZFSEDIR = lzfse/src
LIBLZFSE += $(LZFSEDIR)/lzfse_decode.c \
            $(LZFSEDIR)/lzfse_decode_base.c \
            $(LZFSEDIR)/lzfse_encode.c \
            $(LZFSEDIR)/lzfse_encode_base.c \
            $(LZFSEDIR)/lzfse_fse.c \
            $(LZFSEDIR)/lzvn_decode_base.c \
            $(LZFSEDIR)/lzvn_encode_base.c 
CF_LZFSE = $(CFLAGS) -I$(LZFSEDIR)

# append lib include directory
CFLAGS	+= -I. -I$(ZSTDMTDIR)

brotli-mt$(EXTENSION):
	$(CC) $(CF_BRO) -DVERSION='$(BRO_VER)' -o $@ $(LIBBRO) $(LDFLAGS) -lm
	$(STRIP) $(SFLAGS) $@
	$(LN) -s $@ un$@
	$(LN) -s $@ brotlicat-mt

lizard-mt$(EXTENSION):
	$(CC) $(CF_LIZ) -DVERSION='$(LIZ_VER)' -o $@ $(LIBLIZ) $(LDFLAGS)
	$(STRIP) $(SFLAGS) $@
	$(LN) -s $@ un$@
	$(LN) -s $@ lizardcat-mt

lz4-mt$(EXTENSION):
	$(CC) $(CF_LZ4) -DVERSION='$(LZ4_VER)' -o $@ $(LIBLZ4) $(LDFLAGS)
	$(STRIP) $(SFLAGS) $@
	$(LN) -s $@ un$@
	$(LN) -s $@ lz4cat-mt

lz5-mt$(EXTENSION):
	$(CC) $(CF_LZ5) -DVERSION='$(LZ5_VER)' -o $@ $(LIBLZ5) $(LDFLAGS)
	$(STRIP) $(SFLAGS) $@
	$(LN) -s $@ un$@
	$(LN) -s $@ lz5cat-mt

zstd-mt$(EXTENSION):
	$(CC) $(CF_ZSTD) -DVERSION='$(ZSTD_VER)' -o $@ $(LIBZSTD) $(LDFLAGS)
	$(STRIP) $(SFLAGS) $@
	$(LN) $@ un$@
	$(LN) $@ zstdcat-mt

snappy-mt$(EXTENSION):
	$(CC) $(CF_SNAP) -DVERSION='$(SNAP_VER)' -o $@ $(LIBSNAP) $(LDFLAGS)
	$(STRIP) $(SFLAGS) $@
	$(LN) $@ un$@
	$(LN) $@ snappycat-mt

lzfse-mt$(EXTENSION):
	$(CC) $(CF_LZFSE) -DVERSION='$(LZFSE_VER)' -o $@ $(LIBLZFSE) $(LDFLAGS)
	$(STRIP) $(SFLAGS) $@
	$(LN) $@ un$@
	$(LN) $@ lzfsecat-mt

loadsource:
	test -d lz4    || git clone https://github.com/Cyan4973/lz4       -b $(LZ4_VER)  --depth=1 lz4
	test -d lz5    || git clone https://github.com/inikep/lz5         -b $(LZ5_VER)  --depth=1 lz5
	test -d zstd   || git clone https://github.com/facebook/zstd      -b $(ZSTD_VER) --depth=1 zstd
	test -d lizard || git clone https://github.com/inikep/lizard      -b $(LIZ_VER)  --depth=1 lizard
	test -d brotli || git clone https://github.com/google/brotli      -b $(BRO_VER)  --depth=1 brotli
	test -d lzfse  || git clone https://github.com/lzfse/lzfse        -b $(LZFSE_VER) --depth=1 lzfse
	test -d snappy || git clone https://github.com/andikleen/snappy-c                --depth=1 snappy

# tests are unix / linux only
tests:
	@dd if=/dev/urandom of=testbytes.raw bs=1M count=10 2>/dev/null
	@for m in brotli lizard lz4 lz5 zstd snappy lzfse ; do \
	cat testbytes.raw | ./$$m-mt -z > compressed.$$m ; \
	cat compressed.$$m | ./$$m-mt -d > testbytes-$$m.raw ; \
	cmp testbytes.raw testbytes-$$m.raw && echo "SUCCESS: $$m" || echo "FAILING: $$m" ; \
	rm compressed.$$m testbytes-$$m.raw ; \
	done
	@rm testbytes.raw

install:
	echo TODO ;)

clean:
	rm -f $(PRGS)
	rm -f unbrotli-mt unlizard-mt unlz4-mt unlz5-mt unzstd-mt unsnappy-mt unlzfse-mt
	rm -f brotlicat-mt lizardcat-mt lz4cat-mt lz5cat-mt zstdcat-mt snappycat-mt lzfsecat-mt

mrproper: clean
	rm -rf brotli lizard lz4 lz5 zstd snappy lzfse
