
CC	= gcc

CFLAGS	= -pthread -O3 -Wall -pipe -fomit-frame-pointer
LDFLAGS	= -lzstd -lpthread

#CFLAGS += -DDEBUGME
#CFLAGS += -Wno-unused-but-set-variable
#CFLAGS += -Wno-unused-variable

PRGS	= zstd-st zstd-mt zstd-tp zstd-mt2

all:	$(PRGS)
again:	clean $(PRGS)

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
