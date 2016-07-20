
CC	= gcc

CFLAGS	= -O -Wall -pipe -fomit-frame-pointer
LDFLAGS	= -lzstd

CFLAGS += -Wno-unused-but-set-variable
CFLAGS += -Wno-unused-variable

SRC	= $(shell ls *.c)
OBJS	= $(SRC:.c=.o)
PRGS	= zstd-mt

all:	$(PRGS)
again:	clean $(PRGS)

zstd-mt: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -rf $(OBJS) $(PRGS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
