/*
 * thread_pool.c
 *
 *  Created on: Sep 9, 2017
 *      Author: mayfa
 */

#define THREAD_NUM		10

#include <pthread.h>

/*************************************/

static pthread_t threads[THREAD_NUM];
static int active_threads[THREAD_NUM];
static int initialized = 0;

static void init();

/**
 * Returns 1 if new thread cannot be allocated, returns 0 on success.
 */
int
new_thread(void * (* fnc) (void *), void *arg)
{
	/* Find first empty thread slot. */
	size_t i = 0;
	while (active_threads[i] == 1 && i < THREAD_NUM) {
		i++;
	}

	if (i == THREAD_NUM)
		return 1;

}

/**
 * Cleans the pool: removes exitted threads.
 */
void
try_join()
{
	for (size_t i = 0; i < THREAD_NUM; ++i) {

	}
}

static void
init()
{
	if (initialized)
		return;

	for (size_t i = 0; i < THREAD_NUM; ++i) {
		active_threads[i] = 0;
	}
}
