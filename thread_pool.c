/*
 * thread_pool.c
 *
 *  Created on: Sep 9, 2017
 *      Author: mayfa
 *
 */

#include "thread_pool.h"

//#define TP_TEST

static void * thread_routine(void *arg);
static void thread_cleanup(void *arg);

static void
thread_cleanup(void *arg) {
    thread_t *thread = (thread_t *) arg;

    /* Mutex can be unlocked if thread was cancelled during waiting for condition. */
    pthread_mutex_trylock(&thread->mtx);

    pthread_mutex_unlock(&thread->mtx);
    pthread_mutex_destroy(&thread->mtx);
    pthread_cond_destroy(&thread->work_signal);
    if (thread->params != NULL) {
        free(thread->params);
    }
}

static void *
thread_routine(void *arg) {
    pool_t *pool = (pool_t *) arg;
    thread_t *this = NULL;

    /* Find "this" thread. */
    for (int i = 0; i < THREAD_NUM; ++i) {
        if (pthread_equal(pool->threads[i].pthread, pthread_self()) != 0) {
            this = &pool->threads[i];
        }
    }

    pthread_cleanup_push(thread_cleanup, this);

    pthread_mutex_lock(&this->mtx);
    /* Wait until every thread in pool is initialized. */
    pthread_barrier_wait(&pool->barrier);
    for (;;) {
        /* Wait for work_signal. */
        while (!this->work_ready) {
            pthread_cond_wait(&this->work_signal, &this->mtx);
        }
        this->fnc(this->params);
        /* Cleanup. */
        free(this->params);
        this->params = NULL;
        this->work_ready = false;
    }

    pthread_cleanup_pop(1);
    return NULL;
}


/**
 * Cancels and joins every thread inside the pool.
 */
void
pool_destroy(pool_t *pool)
{
    pthread_barrier_destroy(&pool->barrier);

    for (int i = 0; i < THREAD_NUM; ++i) {
        thread_t *curr_thread = &pool->threads[i];

        pthread_cancel(curr_thread->pthread);
        pthread_join(curr_thread->pthread, NULL);
    }
}

/**
 * Initialize the thread pool. This function does not return until every thread
 * is initialized.
 */
void
pool_init(pool_t *pool)
{
    pthread_barrier_init(&pool->barrier, NULL, THREAD_NUM + 1);

    for (size_t i = 0; i < THREAD_NUM; ++i) {
        thread_t *curr_thread = &pool->threads[i];

        pthread_mutex_init(&curr_thread->mtx, NULL);
        curr_thread->work_ready = false;
        curr_thread->params = NULL;
        pthread_cond_init(&curr_thread->work_signal, NULL);
        pthread_create(&curr_thread->pthread, NULL, thread_routine, pool);
    }

    /* Wait until every thread is initialized. */
    pthread_barrier_wait(&pool->barrier);
}

/**
 * Assigns function fnc to any non-working thread.
 * @return 0 when fnc was successfully assigned to some thread, EPOOL_FULL when
 * every thread is busy.
 */
int
pool_insert(pool_t *pool, void *(*fnc)(void *), void *arg, size_t arg_len)
{
    void *p;

    // Allocate memory for arg.
    p = malloc(arg_len);
    memcpy(p, arg, arg_len);

    for (int i = 0; i < THREAD_NUM; ++i) {
        thread_t *curr_thread = &pool->threads[i];

        /* Lock can be acquired just in the moment when thread is waiting for signal. */
        if (!curr_thread->work_ready && pthread_mutex_trylock(&curr_thread->mtx) == 0) {
            curr_thread->fnc = fnc;
            curr_thread->params = p;
            curr_thread->work_ready = true;
            pthread_cond_signal(&curr_thread->work_signal);
            pthread_mutex_unlock(&curr_thread->mtx);
            return 0;
        }
    }

    return EPOOL_FULL;
}

#ifdef TP_TEST

#define CYCLE_CNT   3
static int th_num = 0;
static pthread_mutex_t th_num_mtx = PTHREAD_MUTEX_INITIALIZER;
static int thread_complete[CYCLE_CNT];

void *
thread_func(void *arg)
{
    int thrd_num = *((int *) arg);

    /* Simulate work. */
    usleep(4);

    pthread_mutex_lock(&th_num_mtx);
    thread_complete[thrd_num] = 1;
    th_num++;
    pthread_mutex_unlock(&th_num_mtx);

    return NULL;
}

void
test()
{
    int num = 0;
    int missed = 0;
    pool_t pool;

    pool_init(&pool);
    sleep(1); /* simulate some work. */
    for (int i = 0; i < CYCLE_CNT; ++i) {
        if (pool_insert(&pool, thread_func, &num, sizeof(num)) == EPOOL_FULL) {
            missed++;
        }
        num++;
    }
    sleep(1);
    pool_destroy(&pool);
    printf("th_num = %d\n", th_num);
    printf("missed = %d\n", missed);
    /*printf("Uncompleted threads: ");
    for (int j = 0; j < CYCLE_CNT; ++j) {
        if (!thread_complete[j]) {
            printf("[%d], ", j);
        }
    }
    printf("\n");*/
}

int main()
{
    test();
}
#endif
