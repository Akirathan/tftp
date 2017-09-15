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
#define THREAD_NUM		10

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>

void remove_thread(void *arg);
void new_thread(void * (* fnc) (void *), void *arg, size_t arg_len);
void init_pool();

#endif /* THREAD_POOL_H_ */
