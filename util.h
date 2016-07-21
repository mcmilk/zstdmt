
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

#define _FILE_OFFSET_BITS 64

#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>

ssize_t read_loop(int fd, void *buffer, size_t count);
ssize_t write_loop(int fd, const void *buffer, size_t count);
int open_read(const char *filename);
int open_rw(const char *filename);
