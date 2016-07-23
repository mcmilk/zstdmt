
CC	= gcc

CFLAGS	= -pthread -O3 -Wall -pipe -fomit-frame-pointer
LDFLAGS	= -lzstd -lpthread

#CFLAGS += -Wno-unused-but-set-variable
#CFLAGS += -Wno-unused-variable

# lib objects
ZSTDMT	= zstdmt.o workq.o

# testing stuff
OBJ_MT   = $(ZSTDMT) util.o zstd-mt.o
OBJS     = $(ZSTDMT) $(OBJ_MT)

PRGS	= zstd-mt

all:	$(PRGS)
again:	clean $(PRGS)

zstd-mt: $(OBJ_MT)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ_MT)

clean:
	rm -rf $(OBJS) $(PRGS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
