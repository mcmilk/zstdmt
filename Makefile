
CC	= gcc

CFLAGS	= -pthread -O3 -Wall -pipe -fomit-frame-pointer
LDFLAGS	= -lzstd -lpthread

#CFLAGS += -DDEBUGME
#CFLAGS += -Wno-unused-but-set-variable
#CFLAGS += -Wno-unused-variable

PRGS	= zstd-st zstd-mt zstd-tp

all:	$(PRGS)
again:	clean $(PRGS)

# single threaded testing
zstd-st: zstdmt-st.o util.o zstd-mt.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ zstd-mt.o zstdmt-st.o util.o

# simple multi threaded testing (create threads as needed)
zstd-mt: zstdmt-mt.o util.o zstd-mt.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ zstd-mt.o zstdmt-mt.o util.o

# multithreaded via thread pool
zstd-tp: zstdmt-tp.o util.o zstd-mt.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ zstd-mt.o zstdmt-tp.o util.o

clean:
	rm -rf $(PRGS) util.o zstd-*.o zstdmt-*.o

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
