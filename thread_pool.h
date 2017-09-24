/*
 * thread_pool.h
 *
 *  Created on: Sep 9, 2017
 *      Author: mayfa
 *
 * Usage:
 * Statically allocated thread pool. First init_pool must be called to initialize
 * the thread pool, then new_thread function can be called to insert new thread.
 * Every routine passed to new_thread must push remove_thread(NULL) as a cleanup handler.
 *
 * Details:
 * When new routine is inserted, its arguments are copied into newly allocated memory.
 * Every inserted thread has an active flag associated with it. The active flag is
 * set to 1 when new thread is created and to 0 when thread finished (in remove_thread).
 * Note that every thread is created as detached.
 */

#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

/* Number of concurrently running threads. */
#define THREAD_NUM		8
#define EPOOL_FULL      1

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct _thread {
    pthread_t pthread;
    /**
     * mtx is locked while this thread is working.
     */
    pthread_mutex_t mtx;
    bool work_ready;
    pthread_cond_t work_signal;
    void * (* fnc) (void *);
    void *params;
} thread_t;

typedef struct _pool {
    thread_t threads[THREAD_NUM];
    pthread_barrier_t barrier;
} pool_t;

void pool_init(pool_t *pool);
void pool_destroy(pool_t *pool);
int pool_insert(pool_t *pool, void *(*fnc)(void *), void *arg, size_t arg_len);

#endif /* THREAD_POOL_H_ */
