/*
 * thread_pool.c
 *
 *  Created on: Sep 9, 2017
 *      Author: mayfa
 *
 */

#include "thread_pool.h"

static pthread_t threads[THREAD_NUM];
static int active_threads[THREAD_NUM];
/* Current thread index. */
static size_t thread_idx = 0;

static void remove_curr_thread();

/**
 * init_pool must be called before this function.
 */
void
new_thread(void * (* fnc) (void *), void *arg)
{
	/* Check whether pool is full. */
	if (thread_idx == THREAD_NUM)
		thread_idx = 0;

	if (active_threads[thread_idx])
		remove_curr_thread();

	pthread_create(&threads[thread_idx], NULL, fnc, arg);
	active_threads[thread_idx] = 1;
	thread_idx++;
}

void
init_pool()
{
	for (size_t i = 0; i < THREAD_NUM; ++i) {
		active_threads[i] = 0;
	}
}

/**
 * Removes next thread.
 */
static void
remove_curr_thread()
{
	if (!active_threads[thread_idx])
		return;

	pthread_join(threads[thread_idx], NULL);
	active_threads[thread_idx] = 0;
}

