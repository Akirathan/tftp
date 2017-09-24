/*
 * thread_pool.c
 *
 *  Created on: Sep 9, 2017
 *      Author: mayfa
 *
 */

#include "thread_pool.h"


static void * thread_routine(void *arg);

thread_t threads[THREAD_NUM];
static pthread_barrier_t barrier;

void
pool_destroy()
{
    for (int i = 0; i < THREAD_NUM; ++i) {
        pthread_cancel(threads[i].pthread);
        pthread_join(threads[i].pthread, NULL);
    }
}

void
pool_init()
{
    pthread_barrier_init(&barrier, NULL, THREAD_NUM + 1);

    for (size_t i = 0; i < THREAD_NUM; ++i) {
        pthread_mutex_init(&threads[i].mtx, NULL);
        threads[i].work_ready = false;
        pthread_cond_init(&threads[i].work_signal, NULL);
        pthread_create(&threads[i].pthread, NULL, thread_routine, threads + i);
    }

    /* Wait until every thread is initialized. */
    pthread_barrier_wait(&barrier);
}


static void *
thread_routine(void *arg) {
    thread_t *this = (thread_t *) arg;

    pthread_mutex_lock(&this->mtx);

    /* Wait until every thread is initialized. */
    pthread_barrier_wait(&barrier);

    for (;;) {
        /* Wait for work_signal. */
        while (!this->work_ready) {
            pthread_cond_wait(&this->work_signal, &this->mtx);
        }
        this->fnc(this->params);
        /* Cleanup. */
        free(this->params);
        this->work_ready = false;
    }
    pthread_mutex_unlock(&this->mtx);
    return NULL;
}

int
pool_insert(void * (* fnc) (void *), void *arg, size_t arg_len)
{
    void *p;

    // Allocate memory for arg.
    p = malloc(arg_len);
    memcpy(p, arg, arg_len);

    for (int i = 0; i < THREAD_NUM; ++i) {
        /* Lock can be acquired just in the moment when thread is waiting for signal. */
        if (!threads[i].work_ready && pthread_mutex_trylock(&threads[i].mtx) == 0) {
            threads[i].fnc = fnc;
            threads[i].params = p;
            threads[i].work_ready = true;
            //if (debug)
            //    printf("Thread %lu signaled to work.\n", threads[i].pthread);
            pthread_cond_signal(&threads[i].work_signal);
            pthread_mutex_unlock(&threads[i].mtx);
            return 0;
        }
    }

    return EPOOL_FULL;
}

