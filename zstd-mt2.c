
/**
 * Copyright © 2016 Tino Reichardt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* getrusage */
#include <sys/resource.h>
#include <sys/time.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <zbuff.h>

#include "zstdmt-mt2.h"
#include "util.h"

/**
 * program for testing threaded stuff on zstd
 */

static void perror_exit(const char *msg)
{
	perror(msg);

	exit(1);
}

static void version(void)
{
	printf("zstd-mt2 version 0.1\n");

	exit(0);
}

static void usage(void)
{
	printf("Usage: zstd-mt2 [options] infile outfile\n\n");
	printf("Otions:\n");
	printf(" -l N    set level of compression (default: 3)\n");
	printf(" -t N    set number of compression threads (default: 2)\n");
	printf(" -i N    set number of iterations for testing (default: 1)\n");
	printf(" -c      compress (default mode)\n");
	printf
	    (" -d      use decompress mode (threads are set to stream count!)\n");
	printf(" -H      print headline for the testing values\n");
	printf(" -h      show usage\n");
	printf(" -v      show version\n");

	exit(0);
}

static void headline(void)
{
	printf("Level;Threads;InSize;OutSize;Frames;Real;User;Sys;MaxMem\n");
	exit(0);
}

#define MODE_COMPRESS    1
#define MODE_DECOMPRESS  2

/* for the -i option */
#define MAX_ITERATIONS   1000

ssize_t my_read_loop(int fd, void *buffer, size_t count)
{
	//printf("read_loop(fd=%d, buffer=%p,count=%zu)\n", fd, buffer, count);
	return read_loop(fd, buffer, count);
}

ssize_t my_write_loop(int fd, const void *buffer, size_t count)
{
	//printf("write_loop(fd=%d, buffer=%p,count=%zu)\n", fd, buffer, count);
	return write_loop(fd, buffer, count);
}

static void do_compress(int threads, int level, int fdin, int fdout)
{
	static int first = 1;

	/* 1) create compression context */
	ZSTDMT_CCtx *ctx = ZSTDMT_createCCtx(threads, level);
	if (!ctx)
		perror_exit("Allocating ctx failed!");
	ZSTDMT_CompressCCtx(ctx, my_read_loop, my_write_loop, fdin, fdout);

	if (first) {
		printf("%d;%d;%zu;%zu;%zu",
		       level, threads,
		       ZSTDMT_GetCurrentInsizeCCtx(ctx),
		       ZSTDMT_GetCurrentOutsizeCCtx(ctx),
		       ZSTDMT_GetCurrentFrameCCtx(ctx));
		first = 0;
	}

	ZSTDMT_freeCCtx(ctx);
}

static void do_decompress(int fdin, int fdout)
{
	unsigned char buf[4];
	ssize_t ret;

	/* 1) read 2 Byte Format Header */
	ret = read_loop(fdin, buf, 2);
	if (ret != 2)
		perror_exit("Reading input failed!");

	/* 2) allocate ctx, give the 2 byte header for calculating the streams */
	ZSTDMT_DCtx *ctx = ZSTDMT_createDCtx(buf);
	if (!ctx)
		perror_exit("Allocating ctx failed!");

	ZSTDMT_DecompressDCtx(ctx, read_loop, write_loop, fdin, fdout);
	ZSTDMT_freeDCtx(ctx);
}

#define tsub(a, b, result) \
do { \
(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
(result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
 if ((result)->tv_nsec < 0) { \
  --(result)->tv_sec; (result)->tv_nsec += 1000000000; \
 } \
} while (0)

int main(int argc, char **argv)
{
	/* default options: */
	int opt, opt_threads = 2, opt_level = 3;
	int opt_mode = MODE_COMPRESS, fdin, fdout;
	int opt_iterations = 1;
	struct rusage ru;
	struct timeval tms, tme, tm;

	while ((opt = getopt(argc, argv, "vhHl:t:i:dc")) != -1) {
		switch (opt) {
		case 'v':	/* version */
			version();
		case 'h':	/* help */
			usage();
		case 'H':	/* headline */
			headline();
		case 'l':	/* level */
			opt_level = atoi(optarg);
			break;
		case 't':	/* threads */
			opt_threads = atoi(optarg);
			break;
		case 'i':	/* iterations */
			opt_iterations = atoi(optarg);
			break;
		case 'd':	/* mode = decompress */
			opt_mode = MODE_DECOMPRESS;
			break;
		case 'c':	/* mode = compress */
			opt_mode = MODE_COMPRESS;
			break;
		default:
			usage();
		}
	}

	/* prog [options] infile outfile */
	if (argc != optind + 2)
		usage();

	/**
	 * check parameters
	 */

	/* opt_level = 1..22 */
	if (opt_level < 1)
		opt_level = 1;
	else if (opt_level > ZSTD_maxCLevel())
		opt_level = ZSTD_maxCLevel();

	/* opt_threads = 1..ZSTDMT_THREADMAX */
	if (opt_threads < 1)
		opt_threads = 1;
	else if (opt_threads > ZSTDMT_THREADMAX)
		opt_threads = ZSTDMT_THREADMAX;

	/* opt_iterations = 1..MAX_ITERATIONS */
	if (opt_iterations < 1)
		opt_iterations = 1;
	else if (opt_iterations > MAX_ITERATIONS)
		opt_iterations = MAX_ITERATIONS;

	/* file names */
	fdin = open_read(argv[optind]);
	if (fdin == -1)
		perror_exit("Opening infile failed");

	fdout = open_rw(argv[optind + 1]);
	if (fdout == -1)
		perror_exit("Opening outfile failed");

	/* begin timing */
	gettimeofday(&tms, NULL);

	for (;;) {
		if (opt_mode == MODE_COMPRESS) {
			do_compress(opt_threads, opt_level, fdin, fdout);
		} else {
			do_decompress(fdin, fdout);
		}

		opt_iterations--;
		if (opt_iterations == 0)
			break;

		lseek(fdin, 0, SEEK_SET);
		lseek(fdout, 0, SEEK_SET);
	}

	/* end of timing */
	gettimeofday(&tme, NULL);
	timersub(&tme, &tms, &tm);
	getrusage(RUSAGE_SELF, &ru);
	printf(";%ld.%ld;%ld.%ld;%ld.%ld;%ld\n",
	       tm.tv_sec, tm.tv_usec / 1000,
	       ru.ru_utime.tv_sec, ru.ru_utime.tv_usec / 1000,
	       ru.ru_stime.tv_sec, ru.ru_stime.tv_usec / 1000, ru.ru_maxrss);

	/* exit should flush stdout */
	exit(0);
}
