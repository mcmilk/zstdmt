
/**
 * Copyright (c) 2016 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 *
 * You can contact the author at:
 * - zstdmt source repository: https://github.com/mcmilk/zstdmt
 */

/* getrusage */
#include <sys/resource.h>
#include <sys/time.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "lz5mt.h"
#include "util.h"

/**
 * program for testing threaded stuff on zstd
 */

static void perror_exit(char *msg)
{
	printf("%s\n", msg);
	fflush(stdout);
	exit(1);
}

static void version(void)
{
	printf("lz5mt version 0.1\n");

	exit(0);
}

static void usage(void)
{
	printf("Usage: lz5mt [options] infile outfile\n\n");
	printf("Otions:\n");
	printf(" -l N    set level of compression (default: 3)\n");
	printf(" -t N    set number of (de)compression threads (default: 2)\n");
	printf(" -i N    set number of iterations for testing (default: 1)\n");
	printf(" -b N    set input chunksize to N MiB (default: auto)\n");
	printf(" -c      compress (default mode)\n");
	printf(" -d      use decompress mode (XXX, not done)\n");
	printf(" -H      print headline for the testing values\n");
	printf(" -h      show usage\n");
	printf(" -v      show version\n");

	exit(0);
}

static void headline(void)
{
	printf
	    ("Type;Level;Threads;InSize;Frames;OutSize;Real;User;Sys;MaxMem\n");
	exit(0);
}

#define MODE_COMPRESS    1
#define MODE_DECOMPRESS  2

/* for the -i option */
#define MAX_ITERATIONS   1000

int my_read_loop(void *arg, LZ5MT_Buffer * in)
{
	int *fd = (int *)arg;
	int done = read_loop(*fd, in->buf, in->size);

	//printf("read_loop(fd=%d, buffer=%p,count=%d)\n", *fd, in->buf,
	//       in->size);
	//fflush(stdout);
	in->size = done;

	return done;
}

int my_write_loop(void *arg, LZ5MT_Buffer * out)
{
	int *fd = (int *)arg;
	int done = write_loop(*fd, out->buf, out->size);

	//printf("write_loop(fd=%d, buffer=%p,count=%d)\n", *fd, out->buf,
	//       out->size);
	//fflush(stdout);
	out->size = done;

	return done;
}

static void do_compress(int threads, int level, int bufsize,
			int fdin, int fdout)
{
	static int first = 1;
	LZ5MT_RdWr_t rdwr;
	int ret;

	/* 1) setup read/write functions */
	rdwr.fn_read = my_read_loop;
	rdwr.fn_write = my_write_loop;
	rdwr.arg_read = (void *)&fdin;
	rdwr.arg_write = (void *)&fdout;

	/* 2) create compression context */
	LZ5MT_CCtx *ctx = LZ5MT_createCCtx(threads, level, bufsize);
	if (!ctx)
		perror_exit("Allocating ctx failed!");

	/* 3) compress */
	ret = LZ5MT_CompressCCtx(ctx, &rdwr);
	if (ret == -1)
		perror_exit("LZ5MT_CompressCCtx() failed!");

	/* 4) get statistic */
	if (first) {
		printf("%d;%d;%zu;%zu;%zu",
		       level, threads,
		       LZ5MT_GetInsizeCCtx(ctx), LZ5MT_GetOutsizeCCtx(ctx),
		       LZ5MT_GetFramesCCtx(ctx));
		first = 0;
	}

	/* 5) free resources */
	LZ5MT_freeCCtx(ctx);
}

static void do_decompress(int threads, int bufsize, int fdin, int fdout)
{
	static int first = 1;
	LZ5MT_RdWr_t rdwr;
	int ret;

	/* 1) setup read/write functions */
	rdwr.fn_read = my_read_loop;
	rdwr.fn_write = my_write_loop;
	rdwr.arg_read = (void *)&fdin;
	rdwr.arg_write = (void *)&fdout;

	/* 2) create compression context */
	LZ5MT_DCtx *ctx = LZ5MT_createDCtx(threads, bufsize);
	if (!ctx)
		perror_exit("Allocating ctx failed!");

	/* 3) compress */
	ret = LZ5MT_DecompressDCtx(ctx, &rdwr);
	if (ret == -1)
		perror_exit("LZ4MT_CompressDCtx() failed!");

	/* 4) get statistic */
	if (first) {
		printf("%d;%d;%zu;%zu;%zu",
		       0, threads,
		       LZ5MT_GetInsizeDCtx(ctx), LZ5MT_GetOutsizeDCtx(ctx),
		       LZ5MT_GetFramesDCtx(ctx));
		first = 0;
	}

	/* 5) free resources */
	LZ5MT_freeDCtx(ctx);
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
	int opt, opt_threads = 2, opt_level = 1;
	int opt_mode = MODE_COMPRESS, fdin, fdout;
	int opt_iterations = 1, opt_bufsize = 0;
	struct rusage ru;
	struct timeval tms, tme, tm;

	while ((opt = getopt(argc, argv, "vhHl:t:i:dcb:B:")) != -1) {
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
		case 'b':	/* input buffer in MB */
			opt_bufsize = atoi(optarg);
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
	else if (opt_level > 9)
		opt_level = 9;

	/* opt_threads = 1..LZ5MT_THREADMAX */
	if (opt_threads < 1)
		opt_threads = 1;
	else if (opt_threads > LZ5MT_THREAD_MAX)
		opt_threads = LZ5MT_THREAD_MAX;

	/* opt_iterations = 1..MAX_ITERATIONS */
	if (opt_iterations < 1)
		opt_iterations = 1;
	else if (opt_iterations > MAX_ITERATIONS)
		opt_iterations = MAX_ITERATIONS;

	/* opt_bufsize is in MB */
	if (opt_bufsize > 0)
		opt_bufsize *= 1024 * 1024;

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
			do_compress(opt_threads, opt_level, opt_bufsize,
				    fdin, fdout);
		} else {
			do_decompress(opt_threads, opt_bufsize, fdin, fdout);
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
