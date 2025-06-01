
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pthread_util.h"

struct work_queue {
	pthread_mutex_t lock;
	void **queue;
	int head; /* add things to head */
	int tail; /* remove things from tail */
	pthread_t *worker;
	int nitems;
	int capacity;
	void (*process_work)(void *work);
	pthread_cond_t wakeup;
	int thread_count;
	int blocked;
};

static int queue_empty(struct work_queue *wq)
{
	return wq->head == wq->tail;
}

static int queue_full(struct work_queue *wq)
{
	return ((wq->head + 1) % wq->capacity) == wq->tail;
}

/* Wake up a thread to work on a work item. */
static void work_queue_wakeup(struct work_queue *wq)
{
	if (wq->thread_count <= 0) /* Nothing to do if no threads present */
		return;
        pthread_mutex_lock(&wq->lock);
        int rc = pthread_cond_signal(&wq->wakeup);
        if (rc)
                fprintf(stderr, "work_queue_wakeup: Huh... pthread_cond_broadcast failed.\n");
        pthread_mutex_unlock(&wq->lock);
}

static void *work_queue_wait_for_work(struct work_queue *wq)
{
	pthread_mutex_lock(&wq->lock);
	do {
		if (!queue_empty(wq)) {
			void *work = wq->queue[wq->tail];
			wq->tail = (wq->tail + 1) % wq->capacity;
			wq->nitems--;
			pthread_mutex_unlock(&wq->lock);
			return work;
		}
		pthread_cond_wait(&wq->wakeup, &wq->lock);
	} while (1);
}

static void *worker_thread(void *x)
{
	struct work_queue *wq = x;

	do {
		void *work = work_queue_wait_for_work(wq);
		if (work)
			wq->process_work(work);
	} while (1);
	return NULL;
}

struct work_queue *work_queue_init(char *threadname, int queue_depth, int thread_count,
		void (*process_work)(void *work))
{
	struct work_queue *wq = malloc(sizeof(*wq));	
	wq->queue = calloc(queue_depth, sizeof(*wq->queue));
	wq->worker = calloc(thread_count, sizeof(pthread_t));
	wq->nitems = 0;
	wq->head = 0;
	wq->tail = 0;
	wq->blocked = 0;
	wq->capacity = queue_depth;
	pthread_mutex_init(&wq->lock, NULL);
	wq->process_work = process_work;
	pthread_cond_init(&wq->wakeup, NULL);
	wq->thread_count = 0;
	char qname[16];
	
	for (int i = 0; i < thread_count; i++) {
		snprintf(qname, sizeof(qname) - 4, "%s", threadname);
		int len = strlen(qname);
		snprintf(&qname[len], 3, "%d", i <= 99 ? i : 99); 
		int rc = create_thread(&wq->worker[i], worker_thread, wq, qname, 1);
		if (rc != 0) {
			fprintf(stderr, "work_queue_init: Error creating worker thread: %s\n",
					strerror(rc));
		} else {
			wq->thread_count++;
		}
	}
	return wq;
}

int work_queue_enqueue(struct work_queue *wq, void *work)
{
	pthread_mutex_lock(&wq->lock);
	if (queue_full(wq) || wq->blocked) {
		pthread_mutex_unlock(&wq->lock);
		return -1;
	}
	wq->queue[wq->head] = work;
	wq->head = (wq->head + 1) % wq->capacity;
	wq->nitems++;
	pthread_mutex_unlock(&wq->lock);
	if (wq->thread_count > 0)
		work_queue_wakeup(wq);
	return 0;
}

void *work_queue_dequeue(struct work_queue *wq)
{
	pthread_mutex_lock(&wq->lock);
	if (queue_empty(wq)) {
		pthread_mutex_unlock(&wq->lock);
		return NULL;
	}
	void *work = wq->queue[wq->tail];
	wq->tail = (wq->tail + 1) % wq->capacity;
	wq->nitems--;
	pthread_mutex_unlock(&wq->lock);
	return work;
}

void work_queue_free(struct work_queue *wq)
{
	free(wq->queue);
	free(wq->worker);
	free(wq);
}

void work_queue_shutdown(struct work_queue *wq)
{
	pthread_mutex_lock(&wq->lock);
	wq->blocked = 1;
	wq->nitems = 0;
	for (int i = 0; i < wq->thread_count; i++)
		pthread_cancel(wq->worker[i]);
	pthread_mutex_unlock(&wq->lock);
}

void work_queue_drain(struct work_queue *wq)
{
	pthread_mutex_lock(&wq->lock);
	wq->blocked = 1;
	while (wq->nitems > 0) {
		pthread_mutex_unlock(&wq->lock);
		work_queue_wakeup(wq);
		usleep(1000); /* Hm, this is hacky */
		pthread_mutex_lock(&wq->lock);
	}
	pthread_mutex_unlock(&wq->lock);
}

