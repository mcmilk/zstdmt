
CC	= gcc

CFLAGS	= -O -Wall -pipe -fomit-frame-pointer
LDFLAGS	= -lzstd

CFLAGS += -Wno-unused-but-set-variable
CFLAGS += -Wno-unused-variable

ZSTDMT	= zstdmt.o
PRGS	= zstd-mt zstd-test

all:	$(PRGS)
again:	clean $(PRGS)

OBJ_MT   = $(ZSTDMT) util.o zstd-mt.o
OBJ_TEST = $(ZSTDMT) util.o zstd-test.o
OBJS     = $(OBJ_MT) $(OBJ_TEST)

zstd-mt: $(OBJ_MT)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ_MT)

zstd-test: $(OBJ_TEST)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ_TEST)

clean:
	rm -rf $(OBJS) $(PRGS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
