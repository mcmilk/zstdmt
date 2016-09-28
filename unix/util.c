
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

#include "util.h"

ssize_t read_loop(int fd, void *buffer, size_t count)
{
	ssize_t offset;

	offset = 0;
	while (count > 0) {
		ssize_t block = read(fd, buffer + offset, count);

		if (block < 0)
			return block;
		if (!block)
			return offset;

		offset += block;
		count -= block;
	}

	return offset;
}

ssize_t write_loop(int fd, const void *buffer, size_t count)
{
	ssize_t done = 0;

	while (count > 0) {
		ssize_t block = write(fd, buffer + done, count);

		if (block < 0)
			return block;
		if (!block)
			return done;

		done += block;
		count -= block;
	}

	return done;
}

int open_read(const char *filename)
{
	return open(filename, O_RDONLY | O_NDELAY);
}

int open_rw(const char *filename)
{
	return open(filename, O_TRUNC | O_RDWR | O_CREAT | O_NDELAY, 0644);
}
