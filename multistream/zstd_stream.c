
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

#include "util.h"

//#define DEBUGME

/**
 * zstd default, for benchmarking
 */

#define MODE_COMPRESS    1
#define MODE_DECOMPRESS  2

static void perror_exit(const char *msg)
{
	perror(msg);

	exit(1);
}

static void version(void)
{
	printf("zstd-stream version 0.1\n");

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

static void headline(int mode)
{
	if (mode == MODE_COMPRESS)
		printf
		    ("Type;Level;Threads;InSize;OutSize;Blocks;Real;User;Sys;MaxMem\n");
	else
		printf
		    ("Type;Threads;InSize;OutSize;Blocks;Real;User;Sys;MaxMem\n");
	exit(0);
}

/* for the -i option */
#define MAX_ITERATIONS   1000

static void do_compress(int level, int blocksize, int fdin, int fdout)
{
	void *src, *dst;
	size_t inlen = 0, outlen = 0, blocks = 0;
	static int first = 1;

	src = malloc(blocksize);
	dst = malloc(ZSTD_compressBound(blocksize));
	if (!src)
		perror_exit("Space for source buffer");
	if (!src)
		perror_exit("Space for dest buffer");

/*
    ZSTD_parameters params = ZSTD_getParams(compressionLevel, 0, 0);
    if (maxWindowLog != 0 && params.cParams.windowLog > maxWindowLog) {
      params.cParams.windowLog = maxWindowLog;
      params.cParams = ZSTD_adjustCParams(params.cParams, 0, 0);
    }


*/

	for (;;) {
		size_t ret, srclen;

		/* read input */
		srclen = read_loop(fdin, src, blocksize);
		inlen += srclen;

		/* eof reached */
		if (srclen == 0) {
			break;
		}

		ret = ZSTD_compress(dst, blocksize, src, ZSTD_compressBound(srclen), level);
		if (ZSTD_isError(ret))
			perror_exit(ZSTD_getErrorName(ret));

		if (ret > 0) {
			write_loop(fdout, dst, ret);
			if (first)
				outlen += ret;
		}
#ifdef DEBUGME
		printf
		    ("ZSTD_compress() ret=%zu srclen=%zu\n",
		     ret, srclen);
#endif

		blocks++;
	}

	if (first) {
		printf("%d;%d;%zu;%zu;%zu", level, 1, inlen, outlen, blocks);
		first = 0;
	}

	free(dst);
	free(src);
}

static void do_decompress(int fdin, int fdout)
{
	ZBUFF_DCtx *ctx;
	void *src, *dst;
	size_t srclen, dstlen, ret, cursize, pos;

	srclen = ZBUFF_recommendedDInSize();
	dstlen = ZBUFF_recommendedDOutSize();

	ctx = ZBUFF_createDCtx();
	src = malloc(srclen);
	dst = malloc(dstlen);
	if (!ctx)
		perror_exit("ZBUFF_createDCtx() failed");
	if (!src)
		perror_exit("Space for source buffer");
	if (!dst)
		perror_exit("Space for dest buffer");

	ZBUFF_decompressInit(ctx);
	pos = 0;
	cursize = 0;
	for (;;) {
		/* read input */
		if (cursize == 0 || pos == ZBUFF_recommendedDInSize()) {
			ret = read_loop(fdin, src, ZBUFF_recommendedDInSize());

			/* eof reached */
			if (ret == 0) {
				printf("done");
				break;
			}

			srclen = ret;
			pos = 0;
		}

		cursize = srclen - pos;
		// printf ("ZBUFF1: srclen=%zu dstlen=%zu cursize=%zu pos=%zu\n", srclen, dstlen, cursize, pos);
		ret =
		    ZBUFF_decompressContinue(ctx, dst, &dstlen, src + pos,
					     &cursize);
		if (ZSTD_isError(ret))
			perror_exit(ZSTD_getErrorName(ret));
		// printf ("ZBUFF2: srclen=%zu dstlen=%zu cursize=%zu pos=%zu\n\n", srclen, dstlen, cursize, pos);

		if (dstlen) {
			write_loop(fdout, dst, dstlen);
		}

		if (cursize) {
			pos += cursize;
		}

		/* end of frame */
		if (ret == 0)
			break;
	}

	free(dst);
	free(src);
	free(ctx);
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
	int opt_blocksize = 128;
	struct rusage ru;
	struct timeval tms, tme, tm;

	while ((opt = getopt(argc, argv, "vhHb:l:i:dc")) != -1) {
		switch (opt) {
		case 'v':	/* version */
			version();
		case 'h':	/* help */
			usage();
		case 'H':	/* headline */
			headline(opt_mode);
		case 'b':	/* blocksize */
			opt_blocksize = atoi(optarg);
			break;
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
	
	/* 128 -> 128K */
	opt_blocksize *= 1024;

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
			do_compress(opt_level, opt_blocksize, fdin, fdout);
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
