/*
 * thread_pool.h
 *
 *  Created on: Sep 9, 2017
 *      Author: mayfa
 *
 * Statically allocated thread pool. First init_pool must be called to initialize
 * the thread pool, then new_thread function can be called to insert new thread.
 * The pool is lineary indexed (with thread_idx) and every time new thread is
 * created, the previous thread on the same index (if any) is joined.
 */

#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

/* Number of concurrently running threads. */
#define THREAD_NUM		10

#include <err.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>

void remove_thread(void *arg);
void new_thread(void * (* fnc) (void *), void *arg, size_t arg_len);
void init_pool();

#endif /* THREAD_POOL_H_ */
