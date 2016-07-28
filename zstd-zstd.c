
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

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <zbuff.h>

#include "util.h"

//#define DEBUGME
#ifdef DEBUGME
#include <stdio.h>
#endif

/**
 * zstd default, for benchmarking
 */

static void perror_exit(const char *msg)
{
	perror(msg);

	exit(1);
}

static void version(void)
{
	printf("zstd-zstd version 0.1\n");

	exit(0);
}

static void usage(void)
{
	printf("Usage: zstd-zstd [options] infile outfile\n\n");
	printf("Otions:\n");
	printf(" -l N    set level of compression (default: 3)\n");
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

static void do_compress(int level, int fdin, int fdout)
{
	ZBUFF_CCtx *ctx;
	void *src, *dst;
	size_t inlen = 0, outlen = 0, frames = 0;
	static int first = 1;

	ctx = ZBUFF_createCCtx();
	src = malloc(ZBUFF_recommendedCInSize());
	dst = malloc(ZBUFF_recommendedCOutSize());
	if (!ctx) perror_exit("ZBUFF_createCCtx() failed");
	if (!src) perror_exit("Space for source buffer");
	if (!src) perror_exit("Space for dest buffer");

	ZBUFF_compressInit(ctx, level);
	for (;;) {
		size_t ret, srclen, dstlen;

		/* read input */
		srclen = read_loop(fdin, src, ZBUFF_recommendedCInSize());
		inlen += srclen;

		/* eof reached */
		if (srclen == 0) {
			dstlen = ZBUFF_recommendedCOutSize();
			ret = ZBUFF_compressFlush(ctx, dst, &dstlen);
			if (ZSTD_isError(ret))
				perror_exit(ZSTD_getErrorName(ret));

			write_loop(fdout, dst, dstlen);
			outlen += dstlen;

			dstlen = ZBUFF_recommendedCOutSize();
			ret = ZBUFF_compressEnd(ctx, dst, &dstlen);
			if (ZSTD_isError(ret))
				perror_exit(ZSTD_getErrorName(ret));

			write_loop(fdout, dst, dstlen);
			outlen += dstlen;

			break;
		}

		dstlen = ZBUFF_recommendedCOutSize();
		ret = ZBUFF_compressContinue(ctx, dst, &dstlen, src, &srclen);
		if (ZSTD_isError(ret))
			perror_exit(ZSTD_getErrorName(ret));

		if (dstlen) {
			write_loop(fdout, dst, dstlen);
			outlen += dstlen;
		}

#ifdef DEBUGME
	printf("ZBUFF_decompressContinue ret=%zu srclen=%zu dstlen=%zu\n",
	       ret, srclen, dstlen);
#endif

		frames++;
	}

	if (first) {
		printf("%d;%d;%zu;%zu;%zu", level, 1, inlen, outlen, frames);
		first = 0;
	}

	free(dst);
	free(src);
	free(ctx);
}

static void do_decompress(int fdin, int fdout)
{
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
	int opt, opt_level = 3, opt_iterations = 1;
	int opt_mode = MODE_COMPRESS, fdin, fdout;
	struct rusage ru;
	struct timeval tms, tme, tm;

	while ((opt = getopt(argc, argv, "vhHl:i:dc")) != -1) {
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
			do_compress(opt_level, fdin, fdout);
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
