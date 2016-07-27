
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

#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include "workq.h"

#if (__GNUC__ > 2)
#define likely(x)	__builtin_expect(!!(x),1)
#define unlikely(x)	__builtin_expect(!!(x),0)
#else
#define likely(x)	x
#define unlikely(x)	x
#endif				/* __builtin_expect */

static void die_pthread()
{
	exit(111);
}

static void work_queue_push(work_queue_t * work_q, void *data);
static void *work_queue_pop(work_queue_t * work_q);
static void *work_queue_worker(void *arg);

static void work_queue_push(work_queue_t * work_q, void *data)
{
	work_item_t *work_item;

	if (unlikely(!work_q)) {
		return;
	}

	work_item = (work_item_t *) malloc(sizeof(work_item_t));
	work_item->next = NULL;
	work_item->data = data;

	if (work_q->head == NULL) {
		work_q->head = work_q->tail = work_item;
		work_q->item_count = 1;
	} else {
		work_q->tail->next = work_item;
		work_q->tail = work_item;
		work_q->item_count++;
	}

	return;
}

static void *work_queue_pop(work_queue_t * work_q)
{
	work_item_t *work_item;
	void *data;

	if (unlikely(!work_q || !work_q->head)) {
		return 0;
	}

	work_item = work_q->head;
	data = work_item->data;
	work_q->head = work_item->next;
	if (work_q->head == NULL) {
		work_q->tail = NULL;
	}
	free(work_item);
	work_q->item_count--;

	return data;
}

static void *work_queue_worker(void *arg)
{
	workq_t *tp = (workq_t *) arg;
	void *job_data;
	int r, must_exit = 0;
	struct timespec timeout;

	/* loop looking for work */
	for (;;) {
		r = pthread_mutex_lock(&tp->lock);
		if (unlikely(r != 0))
			die_pthread();
		timeout.tv_sec = time(NULL) + tp->idle_timeout;
		timeout.tv_nsec = 0;
		tp->thr_idle++;
		while (((job_data = work_queue_pop(tp->queue)) == NULL)
		       && (tp->state != POOL_EXIT)) {
			/* waiting for work until timeout */
			r = pthread_cond_timedwait(&tp->work, &tp->lock,
						   &timeout);
			if (r == ETIMEDOUT) {
				must_exit = 1;
				break;
			}
		}
		tp->thr_idle--;

		if (tp->state == POOL_EXIT) {
			must_exit = 1;
		}

		r = pthread_mutex_unlock(&tp->lock);
		if (unlikely(r != 0))
			die_pthread();

		if (job_data) {
			tp->fun(job_data);
		} else if (must_exit) {
			/* thread should exit now */
			break;
		}

		/* we are ready for new work */
		r = pthread_cond_signal(&tp->done);
		if (unlikely(r != 0))
			die_pthread();

	}

	r = pthread_mutex_lock(&tp->lock);
	if (unlikely(r != 0))
		die_pthread();

	tp->thr_alive--;

	/* signal that all threads are finished */
	if (unlikely(tp->thr_alive == 0)) {
		r = pthread_cond_broadcast(&tp->work);
		if (unlikely(r != 0))
			die_pthread();
	}

	r = pthread_mutex_unlock(&tp->lock);
	if (unlikely(r != 0))
		die_pthread();

	return NULL;
}

workq_t *workq_init(int thr_max, int queue_max, int idle_timeout,
		    void (*fun) (void *))
{
	workq_t *tp = (workq_t *) malloc(sizeof(workq_t));
	int r;

	if (unlikely(thr_max <= 0))
		die_pthread();

	tp->queue = (work_queue_t *) malloc(sizeof(work_queue_t));
	tp->queue->head = tp->queue->tail = NULL;
	tp->queue->item_count = 0;

	tp->thr_max = thr_max;
	tp->thr_alive = 0;
	tp->thr_idle = 0;
	tp->idle_timeout = idle_timeout;
	tp->queue_max = queue_max;
	tp->fun = fun;
	tp->state = POOL_VALID;

	r = pthread_mutex_init(&tp->lock, 0);
	if (unlikely(r != 0))
		die_pthread();

	r = pthread_cond_init(&tp->work, 0);
	if (unlikely(r != 0))
		die_pthread();

	r = pthread_cond_init(&tp->done, 0);
	if (unlikely(r != 0))
		die_pthread();

	r = pthread_attr_init(&tp->attr);
	if (unlikely(r != 0))
		die_pthread();

	r = pthread_attr_setdetachstate(&tp->attr, PTHREAD_CREATE_DETACHED);

	if (unlikely(r != 0))
		die_pthread();

	return tp;
}

void workq_deinit(workq_t * tp)
{
	int r;

	if (unlikely(!tp)) {
		return;
	}

	if (unlikely(tp->state != POOL_VALID)) {
		return;
	}

	r = pthread_mutex_lock(&tp->lock);
	if (unlikely(r != 0))
		die_pthread();

	tp->state = POOL_EXIT;

	/* wait for threads to exit */
	if (likely(tp->thr_alive > 0)) {
		r = pthread_cond_broadcast(&tp->work);
		if (unlikely(r != 0))
			die_pthread();
	}

	while (likely(tp->thr_alive > 0)) {
		r = pthread_cond_wait(&tp->work, &tp->lock);
		if (unlikely(r != 0))
			die_pthread();
	}

	r = pthread_mutex_unlock(&tp->lock);
	if (unlikely(r != 0))
		die_pthread();

	r = pthread_mutex_destroy(&tp->lock);
	if (unlikely(r != 0))
		die_pthread();

	r = pthread_cond_destroy(&tp->work);
	if (unlikely(r != 0))
		die_pthread();

	r = pthread_cond_destroy(&tp->done);
	if (unlikely(r != 0))
		die_pthread();

	r = pthread_attr_destroy(&tp->attr);
	if (unlikely(r != 0))
		die_pthread();

	free(tp->queue);
	free(tp);

	return;
}

void workq_add(workq_t * tp, void *user_data)
{
	pthread_t thr_id;
	int r;

	if (unlikely(!tp)) {
		return;
	}

	r = pthread_mutex_lock(&tp->lock);
	if (unlikely(r != 0))
		die_pthread();

	if (tp->state != POOL_VALID) {
		return;
	}

	if ((tp->thr_idle == 0) && (tp->thr_alive < tp->thr_max)) {

		/* Start a new thread */
		r = pthread_create(&thr_id, &tp->attr, work_queue_worker, tp);
		if (unlikely(r != 0))
			die_pthread();
		tp->thr_alive++;
	}

	while (tp->queue->item_count > tp->queue_max) {
		r = pthread_cond_wait(&tp->done, &tp->lock);
		if (unlikely(r != 0))
			die_pthread();
	}

	work_queue_push(tp->queue, user_data);
	pthread_cond_signal(&(tp->work));

	r = pthread_mutex_unlock(&tp->lock);
	if (unlikely(r != 0))
		die_pthread();

	return;
}
