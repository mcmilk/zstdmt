
/**
 * Copyright © 2004 Trog <trog@clamav.net>
 * Copyright © 2008 - 2012 Tino Reichardt
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

#ifndef FNG_WORKQ_H
#define FNG_WORKQ_H

#include <sys/time.h>
#include <pthread.h>

typedef struct work_item_tag {
	struct work_item_tag *next;
	void *data;
	struct timeval time_queued;
} work_item_t;

typedef struct {
	work_item_t *head;
	work_item_t *tail;
	int item_count;
} work_queue_t;

typedef enum {
	POOL_INVALID,
	POOL_VALID,
	POOL_EXIT,
} pool_state_t;

typedef struct {
	pthread_mutex_t lock;
	pthread_attr_t attr;
	pthread_cond_t work;
	pthread_cond_t done;

	pool_state_t state;

	int thr_max;
	int thr_alive;
	int thr_idle;
	int idle_timeout;
	int queue_max;
	work_queue_t *queue;
	void (*fun) (void *);
} workq_t;

/**
 * initialize a new threadpool with n workers and fun() as working function
 */
extern workq_t *workq_init(int thr_max, int queue_max, int idle_timeout,
			   void (*fun) (void *));

/**
 * add work to a threadpool (data is the arg to fun())
 */
extern void workq_add(workq_t * tp, void *user_data);

/**
 * destroy a threadpool
 */
extern void workq_deinit(workq_t * tp);

#endif
