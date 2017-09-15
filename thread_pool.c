/*
 * thread_pool.c
 *
 *  Created on: Sep 9, 2017
 *      Author: mayfa
 *
 */

#include "thread_pool.h"

static struct {
	pthread_mutex_t mutex;
	pthread_t thread;
	int active;
	/*
	 * Dynamicaly allocated buffer for parameters passed to the thread.
	 * Note that it is important to copy parameters into this buffer,
	 * otherwise pointer to "volatile" value would be passed to the
	 * thread routine (value would be modified in generic_server function
	 * in server.c).
	 */
	void *par_buf;
	size_t par_len;
} threads[THREAD_NUM];

/**
 * This function must be pushed as cleanup handler to every thread that uses
 * this thread_pool.
 */
void
remove_thread(void *arg)
{
	for (size_t i = 0; i < THREAD_NUM; ++i) {
		if (pthread_equal(pthread_self(), threads[i].thread)) {
			pthread_mutex_lock(&threads[i].mutex);
			/* pthread_t are reused because threads are detached - null it. */
			memset(&threads[i].thread, 0, sizeof(threads[i].thread));
			free(threads[i].par_buf);
			threads[i].par_len = 0;
			threads[i].active = 0;
		}
		pthread_mutex_unlock(&threads[i].mutex);
	}
}

/**
 * Inserts new detached thread into pool.
 * init_pool must be called before this function.
 * If there is no room for new thread, waits for first one.
 */
void
new_thread(void * (* fnc) (void *), void *arg, size_t arg_len)
{
	void *p;
	pthread_attr_t attr;

	/* Init detached attribute. */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	/* Allocate memory for arguments. */
	p = malloc(arg_len);
	memcpy(p, arg, arg_len);

	for (size_t i = 0; i < THREAD_NUM; ++i) {
		if (threads[i].active)
			continue;

		pthread_mutex_lock(&threads[i].mutex);
		threads[i].par_buf = p;
		threads[i].par_len = arg_len;
		/* Start detached thread. */
		threads[i].active = 1;
		pthread_mutex_unlock(&threads[i].mutex);
		pthread_create(&threads[i].thread, &attr, fnc, threads[i].par_buf);
		return;
	}

	/* All threads are active - wait for first one. */
	for (;;) {
		if (threads[0].active) {
			sched_yield();
		}
		else {
			break;
		}
	}

	pthread_mutex_lock(&threads[0].mutex);
	threads[0].par_buf = p;
	threads[0].par_len = arg_len;
	threads[0].active = 1;
	pthread_mutex_unlock(&threads[0].mutex);
	pthread_create(&threads[0].thread, NULL, fnc, threads[0].par_buf);
}

void
init_pool()
{
	for (size_t i = 0; i < THREAD_NUM; ++i) {
		if (pthread_mutex_init(&threads[i].mutex, NULL) != 0) {
			perror("pthread_mutex_init");
			exit(EXIT_FAILURE);
		}
		threads[i].active = 0;
		threads[i].par_len = 0;
	}
}

