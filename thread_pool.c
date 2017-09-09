/*
 * thread_pool.c
 *
 *  Created on: Sep 9, 2017
 *      Author: mayfa
 *
 */

#include "thread_pool.h"

/**
 * This struct is usefull when allocating memory and copying parameters for
 * thread is in place.
 */
static struct {
	pthread_t thread;
	int active;
	void *par_buf;
	size_t par_len;
} threads[THREAD_NUM];

/* Current thread index. */
static size_t curr_idx = 0;

static void remove_curr_thread();

/**
 * init_pool must be called before this function.
 */
void
new_thread(void * (* fnc) (void *), void *arg, size_t arg_len)
{
	void *p;

	/* Check whether pool is full. */
	if (curr_idx == THREAD_NUM)
		curr_idx = 0;

	/* If current thread is active, join it. */
	if (threads[curr_idx].active)
		remove_curr_thread();

	/* Allocate memory for arguments. */
	p = malloc(arg_len);
	memcpy(p, arg, arg_len);
	threads[curr_idx].par_buf = p;
	threads[curr_idx].par_len = arg_len;

	/* Start thread. */
	pthread_create(&threads[curr_idx].thread, NULL, fnc, threads[curr_idx].par_buf);
	threads[curr_idx].active = 1;

	curr_idx++;
}

void
init_pool()
{
	for (size_t i = 0; i < THREAD_NUM; ++i) {
		threads[i].active = 0;
		threads[i].par_len = 0;
	}
}

/**
 * Removes current thread and dealocates its parameters.
 */
static void
remove_curr_thread()
{
	if (!threads[curr_idx].active)
		return;

	pthread_join(threads[curr_idx].thread, NULL);
	free(threads[curr_idx].par_buf);
	threads[curr_idx].par_len = 0;
	threads[curr_idx].active = 0;
}

