/**
 * thread_pool.h
 *
 *  Created on: Sep 9, 2017
 *      Author: mayfa
 *
 * Usage:
 * Statically allocated thread pool. First pool_init must be called to initialize
 * the thread pool, then pool_insert function can be called to assign function to
 * some thread.
 *
 * Details:
 * Initialization of every thread in pool is synchronized with a barrier, ie.
 * pool_init function does not return until every thread waits on a barrier.
 *
 * When thread is initialized, it waits for "ready to work" condition, which is signalled
 * from the main (outer) thread. It is required that pool_insert is called from an
 * outer thread ie. thread that is not from the pool.
 *
 * pool_insert finds first non-working thread (tries to lock its mutex), assigns
 * function to this thread and signals "ready to work" condition. Parameters for
 * the function are copied into dynamically allocated memory.
 *
 */

#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

/* Number of concurrently running threads. */
#define THREAD_NUM		2
/* Error: Thread pool is busy. */
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
     * mtx is locked while thread is working.
     */
    pthread_mutex_t mtx;
    /**
     * work_ready is set by main thread and then work_signal condition is signalled.
     */
    bool work_ready;
    pthread_cond_t work_signal;
    void * (* fnc) (void *);
    /**
     * Pointer to dynamically allocated parameters.
     */
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
