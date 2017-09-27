#include "thread_pool.h"

//#define TP_TEST
static bool debug = true;

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

static void queue_init(queue_t *queue);
static int queue_push(queue_t *queue, work_t *work);
static int queue_pop(queue_t *queue, work_t *work);

static void
queue_init(queue_t *queue)
{
	queue->head = 0;
	queue->tail = 0;
	queue->size = 0;
}

/**
 * @return EQUEUE_FULL when queue is full, 0 on success.
 */
static int
queue_push(queue_t *queue, work_t *work)
{
	if (queue->head == queue->tail && queue->size != 0) {
		return EQUEUE_FULL;
	}
	else {
		/* Copy work inside work_queue. */
		queue->work_queue[queue->tail] = *work;
		/* Shift tail. */
		queue->tail = (queue->tail + 1) % QUEUE_LEN;
		queue->size++;
		return 0;
	}
}

/**
 * @return EQUEUE_EMPTY when queue is empty, 0 on success.
 */
static int 
queue_pop(queue_t *queue, work_t *work)
{
	if (queue->head == queue->tail && queue->size == 0) {
		return EQUEUE_EMPTY;
	}
	else {
		/* Copy work from queue. */
		*work = queue->work_queue[queue->head];
		/* Shift head. */
		queue->head = (queue->head + 1) % QUEUE_LEN;
		queue->size--;
		return 0;
	}
}


/**
 * When this function is called, mtx may or may not be locked.
 */
static void
thread_cleanupwork(void *arg)
{
	work_t *work = (work_t *) arg;

	if (work->params != NULL) {
		free(work->params);
	}
}

/**
 * Tries to unlock mutex.
 * @param arg pool.
 */
static void
thread_cleanupmtx(void *arg)
{
	pool_t *pool = (pool_t *) arg;

	/* If mutex is not locked by this thread (or not locked at all) return error.,
	 * otherwise unlocks the mutex. Note that mutex is of ERROR_CHECK type. */
	(void) pthread_mutex_unlock(&pool->mtx);
}

static void *
thread_routine(void *arg)
{
	pool_t *pool = (pool_t *) arg;
	work_t work = {NULL, NULL};

	pthread_cleanup_push(thread_cleanupwork, &work);
	pthread_cleanup_push(thread_cleanupmtx, pool);

	for (;;) {
		pthread_mutex_lock(&pool->mtx);
		while (pool->queue.size == 0) {
			if (debug)
				printf("Non-main: Waiting for cond_workrdy.\n");
			pthread_cond_wait(&pool->cond_workrdy, &pool->mtx);
		}
		if (debug)
			printf("Non-main: Pop.\n");
		queue_pop(&pool->queue, &work);
		if (pool->queue.size == QUEUE_LEN - 1) {
			/* Signal main thread that the queue is not full anymore. */
			if (debug)
				printf("Non-main: Signal cond_insertrdy.\n");
			pthread_cond_signal(&pool->cond_insertrdy);
		}
		pthread_mutex_unlock(&pool->mtx);

		work.func(work.params);
		free(work.params);
		work.params = NULL;
	}
	pthread_cleanup_pop(1);
	pthread_cleanup_pop(1);
	return NULL;
}

/**
 * Cancels and joins every thread inside the pool.
 */
void
pool_destroy(pool_t *pool)
{
	for (int i = 0; i < THREAD_NUM; ++i) {
		pthread_cancel(pool->threads[i]);
		pthread_join(pool->threads[i], NULL);
	}
	pthread_mutex_destroy(&pool->mtx);
	pthread_cond_destroy(&pool->cond_workrdy);
	pthread_cond_destroy(&pool->cond_insertrdy);
}

/**
 * Initialize the thread pool.
 */
void
pool_init(pool_t *pool)
{
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	/* Set mutex as ERRORCHECK because every thread in cleanup tries to unlock
	 * this mutex. */
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	pthread_mutex_init(&pool->mtx, &attr);
	pthread_cond_init(&pool->cond_workrdy, NULL);
	pthread_cond_init(&pool->cond_insertrdy, NULL);
	queue_init(&pool->queue);
	for (int i = 0; i < THREAD_NUM; ++i) {
		pthread_create(&pool->threads[i], NULL, thread_routine, (void *) pool);
	}
	pthread_mutexattr_destroy(&attr);
}

/**
 * Assigns function fnc to any non-working thread.
 */
void
pool_insert(pool_t *pool, void *(*fnc)(void *), void *arg, size_t arg_len)
{
	void *p;
	work_t work;

	/* Allocate memory for arg. */
	p = malloc(arg_len);
	memcpy(p, arg, arg_len);

	/* Fill work struct. */
	work.func = fnc;
	work.params = p;

	pthread_mutex_lock(&pool->mtx);
	/* Wait until pool is ready for insert. */
	while (pool->queue.size == QUEUE_LEN) {
		if (debug)
			printf("Main: Waiting for cond_insertrdy.\n");
		pthread_cond_wait(&pool->cond_insertrdy, &pool->mtx);
	}
	if (debug)
		printf("Main: Push.\n");
	queue_push(&pool->queue, &work);
	if (pool->queue.size == 1) {
		/* Wake all waiting threads to work. */
		if (debug)
			printf("Main: Broadcast cond_workrdy.\n");
		pthread_cond_broadcast(&pool->cond_workrdy);
	}
	pthread_mutex_unlock(&pool->mtx);
}

#ifdef TP_TEST

#define CYCLE_CNT   1
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

	printf("Non-main work: %d\n", thrd_num);

	return NULL;
}

void
queue_test()
{
	queue_t queue;
	work_t work;
	int ret = 0;

	queue_init(&queue);
	for (int i = 0; i < QUEUE_LEN; ++i) {
		queue_push(&queue, &work);
	}
	ret = queue_push(&queue, &work); // EQUEUE_FULL

	for (int j = 0; j < QUEUE_LEN; ++j) {
		queue_pop(&queue, &work);
	}
	ret = queue_pop(&queue, &work); // EQUEUE_EMPTY
}

void
test()
{
	pool_t pool;
	int num = 0;

	pool_init(&pool);
	sleep(1); /* simulate some work. */
	for (int i = 0; i < CYCLE_CNT; ++i) {
		pool_insert(&pool, thread_func, &num, sizeof(num));
		num++;
	}
	sleep(1);
	pool_destroy(&pool);
}

int main()
{
	test();
}
#endif