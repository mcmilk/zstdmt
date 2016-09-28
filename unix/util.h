
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

#define _FILE_OFFSET_BITS 64

#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>

ssize_t read_loop(int fd, void *buffer, size_t count);
ssize_t write_loop(int fd, const void *buffer, size_t count);
int open_read(const char *filename);
int open_rw(const char *filename);
