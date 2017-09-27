/**
 * thread_pool.h
 *
 *  Created on: Sep 9, 2017
 *      Author: mayfa
 *
 * Usage:
 * Statically allocated thread pool. First pool_init must be called to initialize
 * the thread pool, then pool_insert function can be called to enqueue function
 * into working queue.
 *
 * Details:
 * Threads are created in pool_init and waiting for "work-ready" signal after return.
 * If the queue is empty and first work is inserted, "work-ready" signal is broadcasted.
 * If the queue is full and work is popped from the queue, "insert-ready" signal is
 * signaled to the main thread.
 *
 * Parameters for the function are copied into dynamically allocated memory.
 *
 */

#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

/* Number of concurrently running threads. */
#define THREAD_NUM		2
#define QUEUE_LEN		2

#define EQUEUE_FULL 	1
#define EQUEUE_EMPTY 	2

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct _work {
	void *(* func) (void *);
	void *params;
} work_t;

typedef struct _queue {
	work_t work_queue[QUEUE_LEN];
	size_t head;
	size_t tail;
	size_t size;
} queue_t;

typedef struct _pool {
	pthread_mutex_t mtx;
	/**
	 * Signalized when queue was empty and now contains work.
	 */
	pthread_cond_t cond_workrdy;
	/**
	 * Signalized when queue is full and one unit of work is popped.
	 */
	pthread_cond_t cond_insertrdy;
	pthread_t threads[THREAD_NUM];
	queue_t queue;
} pool_t;

void pool_init(pool_t *pool);
void pool_destroy(pool_t *pool);
void pool_insert(pool_t *pool, void *(*fnc)(void *), void *arg, size_t arg_len);

#endif /* THREAD_POOL_H_ */
