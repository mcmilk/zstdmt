
CC	= gcc

CFLAGS	= -O -Wall -pipe -fomit-frame-pointer
LDFLAGS	= -lzstd

CFLAGS += -Wno-unused-but-set-variable
CFLAGS += -Wno-unused-variable

# lib objects
ZSTDMT	= zstdmt.o

# testing stuff
OBJ_MT   = $(ZSTDMT) util.o zstd-mt.o
OBJS     = $(OBJ_MT) $(OBJ_TEST)

PRGS	= zstd-mt

all:	$(PRGS)
again:	clean $(PRGS)


zstd-mt: $(OBJ_MT)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ_MT)

clean:
	rm -rf $(OBJS) $(PRGS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
