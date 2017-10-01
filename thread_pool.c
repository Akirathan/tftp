#include "thread_pool.h"

static bool debug = false;

static void queue_init(work_queue_t *queue);
static int queue_push(work_queue_t *queue, work_t *work);
static int queue_pop(work_queue_t *queue, work_t *work);

static void
queue_init(work_queue_t *queue)
{
	queue->head = 0;
	queue->tail = 0;
	queue->size = 0;
}

/**
 * @return EQUEUE_FULL when queue is full, 0 on success.
 */
static int
queue_push(work_queue_t *queue, work_t *work)
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
queue_pop(work_queue_t *queue, work_t *work)
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
 * Tries to free params.
 * @param arg work.
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
 * Note that this function is called from every thread, but just one thread
 * holds the mutex.
 * @param arg pool.
 */
static void
thread_cleanupmtx(void *arg)
{
	pool_t *pool = (pool_t *) arg;

	/* If mutex is not locked by this thread (or not locked at all) return error.
	 * Otherwise unlock the mutex. Note that mutex is of ERROR_CHECK type. */
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
				printf("Thread %lu: Waiting for cond_workrdy.\n", pthread_self());
			pthread_cond_wait(&pool->cond_workrdy, &pool->mtx);
		}
		if (debug)
			printf("Thread %lu: Pop.\n", pthread_self());
		queue_pop(&pool->queue, &work);
		if (pool->queue.size == QUEUE_LEN - 1) {
			/* Signal to main thread that the queue is not full anymore. */
			if (debug)
				printf("Thread %lu: Signal cond_insertrdy.\n", pthread_self());
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
 * Enqueues function into working queue.
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

//#define TP_TEST
#ifdef TP_TEST

#define CYCLE_CNT   40
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

	printf("Thread %lu: Work: %d\n", pthread_self(), thrd_num);

	return NULL;
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
	printf("Total work: %d\n", th_num);
}

int main()
{
	test();
}
#endif
