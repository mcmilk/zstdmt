
CC	= gcc
#CC	= clang

CFLAGS	= -W -pthread -Wall -pipe -fomit-frame-pointer
LDFLAGS	= -lpthread

#CFLAGS += -DDEBUGME
#CFLAGS += -g
CFLAGS += -O
#CFLAGS += -march=native
#CFLAGS += -Wno-unused-but-set-variable
#CFLAGS += -Wno-unused-variable
#CFLAGS += -Wno-unused-function

PRGS	= zstdmt lz4mt lz5mt

OBJ	= threading.o util.o zstdmt_common.o
OBJLZ4	= lz4mt_compress.o lz4mt.o lz4mt_decompress.o
OBJLZ5	= lz5mt_compress.o lz5mt.o lz5mt_decompress.o
OBJZSTD	= zstdmt_compress.o zstdmt.o

all:	$(PRGS)
again:	clean $(PRGS)

lz4mt: $(OBJ) $(OBJLZ4)
	$(CC) $(CFLAGS) $(LDFLAGS) -llz4 -o $@ $(OBJLZ4) $(OBJ)

lz5mt: $(OBJ) $(OBJLZ5)
	$(CC) $(CFLAGS) $(LDFLAGS) -llz5 -o $@ $(OBJLZ5) $(OBJ)

zstdmt: $(OBJ) $(OBJZSTD)
	$(CC) $(CFLAGS) $(LDFLAGS) -lzstd -o $@ $(OBJZSTD) $(OBJ)

clean:
	rm -rf $(PRGS) $(OBJLZ4) $(OBJLZ5) $(OBJZSTD) $(OBJ)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
