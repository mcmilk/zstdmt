
/**
 * Copyright (c) 2016 - 2017 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 *
 * You can contact the author at:
 * - zstdmt source repository: https://github.com/mcmilk/zstdmt
 */

#include <stdio.h>

#include "platform.h"
#include "zstdmt.h"

/**
 * program for testing threaded stuff on zstd
 */

static void perror_exit(const char *msg)
{
	printf("%s\n", msg);
	fflush(stdout);
	exit(1);
}

static void version(void)
{
	printf("zstdmt version " VERSION "\n");
	exit(0);
}

static void usage(void)
{
	printf("Usage: zstdmt [options] INPUT > FILE\n");
	printf("or     zstdmt [options] -o FILE INPUT\n");
	printf("or     cat INPUT | zstdmt [options] -o FILE\n");
	printf("or     cat INPUT | zstdmt [options] > FILE\n\n");

	printf("Options:\n");
	printf(" -o FILE write result to a file named `FILE`\n");
	printf(" -#      set compression level to # (1-22, default:3)\n");
	printf
	    (" -T N    set number of (de)compression threads (def: #cores)\n");
	printf(" -i N    set number of iterations for testing (default: 1)\n");
	printf(" -b N    set input chunksize to N KiB (default: auto)\n");
	printf(" -c      compress (default mode)\n");
	printf(" -d      use decompress mode\n");
	printf(" -t      print timings and memory usage to stderr\n");
	printf(" -H      print headline for the timing values\n");
	printf(" -h      show usage\n");
	printf(" -v      show version\n\n");

	exit(0);
}

static void headline(void)
{
	fprintf(stderr,
		"Level;Threads;InSize;OutSize;Frames;Real;User;Sys;MaxMem\n");
	exit(0);
}

#define MODE_COMPRESS    1
#define MODE_DECOMPRESS  2

/* for the -i option */
#define MAX_ITERATIONS   1000

int my_read_loop(void *arg, ZSTDMT_Buffer * in)
{
	FILE *fd = (FILE *) arg;
	ssize_t done = fread(in->buf, 1, in->size, fd);

#if 0
	fprintf(stderr, "fread(), todo=%u done=%u\n", (unsigned)in->size,
		(unsigned)done);
	fflush(stderr);
#endif

	in->size = done;
	return 0;
}

int my_write_loop(void *arg, ZSTDMT_Buffer * out)
{
	FILE *fd = (FILE *) arg;
	ssize_t done = fwrite(out->buf, 1, out->size, fd);

#if 0
	fprintf(stderr, "fwrite(), todo=%u done=%u\n", (unsigned)out->size,
		(unsigned)done);
	fflush(stderr);
#endif

	out->size = done;
	return 0;
}

static void
do_compress(int threads, int level, int bufsize, FILE * fin, FILE * fout,
	    int opt_timings)
{
	static int first = 1;
	ZSTDMT_RdWr_t rdwr;
	size_t ret;

	/* 1) setup read/write functions */
	rdwr.fn_read = my_read_loop;
	rdwr.fn_write = my_write_loop;
	rdwr.arg_read = (void *)fin;
	rdwr.arg_write = (void *)fout;

	/* 2) create compression context */
	ZSTDMT_CCtx *ctx = ZSTDMT_createCCtx(threads, level, bufsize);
	if (!ctx)
		perror_exit("Allocating ctx failed!");

	/* 3) compress */
	ret = ZSTDMT_compressCCtx(ctx, &rdwr);
	if (ZSTDMT_isError(ret))
		perror_exit(ZSTDMT_getErrorString(ret));

	/* 4) get statistic */
	if (first && opt_timings) {
		fprintf(stderr, "%d;%d;%lu;%lu;%lu",
			level, threads,
			(unsigned long)ZSTDMT_GetInsizeCCtx(ctx),
			(unsigned long)ZSTDMT_GetOutsizeCCtx(ctx),
			(unsigned long)ZSTDMT_GetFramesCCtx(ctx));
		first = 0;
	}

	/* 5) free resources */
	ZSTDMT_freeCCtx(ctx);
}

static void do_decompress(int threads, int bufsize, FILE * fin, FILE *
			  fout, int opt_timings)
{
	static int first = 1;
	ZSTDMT_RdWr_t rdwr;
	size_t ret;

	/* 1) setup read/write functions */
	rdwr.fn_read = my_read_loop;
	rdwr.fn_write = my_write_loop;
	rdwr.arg_read = (void *)fin;
	rdwr.arg_write = (void *)fout;

	/* 2) create compression context */
	ZSTDMT_DCtx *ctx = ZSTDMT_createDCtx(threads, bufsize);
	if (!ctx)
		perror_exit("Allocating ctx failed!");

	/* 3) compress */
	ret = ZSTDMT_decompressDCtx(ctx, &rdwr);
	if (ZSTDMT_isError(ret))
		perror_exit(ZSTDMT_getErrorString(ret));

	/* 4) get statistic */
	if (first && opt_timings) {
		fprintf(stderr, "%d;%d;%lu;%lu;%lu",
			0, threads,
			(unsigned long)ZSTDMT_GetInsizeDCtx(ctx),
			(unsigned long)ZSTDMT_GetOutsizeDCtx(ctx),
			(unsigned long)ZSTDMT_GetFramesDCtx(ctx));
		first = 0;
	}

	/* 5) free resources */
	ZSTDMT_freeDCtx(ctx);
}

int main(int argc, char **argv)
{
	/* default options: */
	int opt, opt_threads = getcpucount(), opt_level = 3;
	int opt_mode = MODE_COMPRESS;
	int opt_iterations = 1, opt_bufsize = 0, opt_timings = 0;
	int opt_numbers = 0;
	char *ofilename = NULL;
	struct rusage ru;
	struct timeval tms, tme, tm;
	FILE *fin, *fout;

	while ((opt = getopt(argc, argv, "vhHT:i:dcb:o:t0123456789")) != -1) {
		switch (opt) {
		case 'v':	/* version */
			version();
		case 'h':	/* help */
			usage();
		case 'H':	/* headline */
			headline();
		case 'T':	/* threads */
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
		case 'o':	/* output filename */
			ofilename = optarg;
			break;
		case 't':	/* print timings */
			opt_timings = 1;
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if (opt_numbers == 0)
				opt_level = 0;
			else
				opt_level *= 10;
			opt_level += ((int)opt - 48);
			opt_numbers++;
			break;
		default:
			usage();
		}
	}

	/**
	 * check parameters
	 */

	/* opt_level = 1..22 */
	if (opt_level < 1)
		opt_level = 1;
	else if (opt_level > ZSTDMT_LEVEL_MAX)
		opt_level = ZSTDMT_LEVEL_MAX;

	/* opt_threads = 1..ZSTDMT_THREAD_MAX */
	if (opt_threads < 1)
		opt_threads = 1;
	else if (opt_threads > ZSTDMT_THREAD_MAX)
		opt_threads = ZSTDMT_THREAD_MAX;

	/* opt_iterations = 1..MAX_ITERATIONS */
	if (opt_iterations < 1)
		opt_iterations = 1;
	else if (opt_iterations > MAX_ITERATIONS)
		opt_iterations = MAX_ITERATIONS;

	/* opt_bufsize is in KiB */
	if (opt_bufsize > 0)
		opt_bufsize *= 1024;

	/* File IO */
	if (argc < optind + 1) {
		if (IS_CONSOLE(stdin)) {
			usage();
		} else {
			fin = stdin;
		}
	} else
		fin = fopen(argv[optind], "rb");

	if (fin == NULL)
		perror_exit("Opening infile failed");

	if (ofilename == NULL) {
		if (IS_CONSOLE(stdout)) {
			usage();
		} else {
			fout = stdout;
		}
	} else
		fout = fopen(ofilename, "wb");

	if (fout == NULL)
		perror_exit("Opening outfile failed");

	/* begin timing */
	gettimeofday(&tms, NULL);

	for (;;) {
		if (opt_mode == MODE_COMPRESS) {
			do_compress(opt_threads, opt_level, opt_bufsize, fin,
				    fout, opt_timings);
		} else {
			do_decompress(opt_threads, opt_bufsize, fin, fout,
				      opt_timings);
		}

		opt_iterations--;
		if (opt_iterations == 0)
			break;

		fseek(fin, 0, SEEK_SET);
		fseek(fout, 0, SEEK_SET);
	}

	/* show timings */
	if (opt_timings) {
		gettimeofday(&tme, NULL);
		timersub(&tme, &tms, &tm);
		getrusage(RUSAGE_SELF, &ru);
		fprintf(stderr, ";%ld.%ld;%ld.%ld;%ld.%ld;%ld\n",
			tm.tv_sec, tm.tv_usec / 1000,
			ru.ru_utime.tv_sec, ru.ru_utime.tv_usec / 1000,
			ru.ru_stime.tv_sec, ru.ru_stime.tv_usec / 1000,
			(long unsigned)ru.ru_maxrss);
	}

	/* exit should flush stdout */
	exit(0);
}
