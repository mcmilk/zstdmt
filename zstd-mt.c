
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <zbuff.h>

#include "zstdmt.h"

/* exit with message and some return code */
void die_msg(int exitcode, const char *msg)
{
	printf("die_msg: %s\n", (char *)msg);

	exit(exitcode);
}

void version(void)
{
	printf("zstd-mt version 0.1\n");

	exit(0);
}

void usage(void)
{
	printf( "Usage: zstd-mt [options] infile outfile\n\n");
	printf( "Otions:\n");
	printf( " -l N    set level of compression (default: 3)\n");
	printf( " -t N    set number of threads (default: 2)\n");
	printf( " -c      compress (default mode)\n");
	printf( " -d      use decompress mode\n");
	printf( " -h      show usage\n");
	printf( " -v      show version\n");

	exit(0);
}

#define MODE_COMPRESS    1
#define MODE_DECOMPRESS  2

void do_compress(int threads, int level, char *infile, char *outfile)
{
	printf("do_compress(%d, %d, %s, %s)\n", threads, level, infile, outfile);
}

void do_decompress(int threads, char *infile, char *outfile)
{
	printf("do_decompress(%d, %s, %s)\n", threads, infile, outfile);
}

int main(int argc, char **argv)
{
	/* default options: */
	int opt, opt_threads = 2, opt_level = 3, opt_mode = MODE_COMPRESS;

	while ((opt = getopt(argc, argv, "vl:t:dc")) != -1) {
		switch (opt) {
		case 'v':	/* version */
			version();
			/* will exit then */
		case 'l':	/* level */
			opt_threads = atoi(optarg);
			break;
		case 't':	/* threads */
			opt_threads = atoi(optarg);
			break;
		case 'd':	/* mode = decompress */
			opt_mode = MODE_DECOMPRESS;
			break;
		case 'c':	/* mode = compress */
			opt_mode = MODE_COMPRESS;
			break;
		default:
			usage();
			/* will exit then */
		}
	}

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

	if (opt_mode == MODE_COMPRESS) {
		do_compress(opt_threads, opt_level, argv[optind+1], argv[optind+2]);
	} else {
		do_decompress(opt_threads, argv[optind+1], argv[optind+2]);
	}

	/* exit should flush stdout */
	exit(0);
}
