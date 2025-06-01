#ifndef WORKQUEUE_H__
#define WORKQUEUE_H__

struct work_queue;

/* work_queue_init() creates a newwork queue.
 *
 * thread_name: name of thread, shows up in e.g. htop output
 * queue_depth is the number of work items the queue
 *	can hold
 * thread_count is the number of threads consuming work from the
 *	queue (which may be zero.)
 * process_work is a function to process a work item, which the
 *	threads will call.  It may be NULL if thread_count == 0)
 * The new work queue is returned.
 */
struct work_queue *work_queue_init(char *threadname, int queue_depth, int thread_count,
		void (*process_work)(void *work));

/* work_queue_enqueue adds a new work item into a work queue.
 *
 * wq is the work queue to which the work item is added.
 * work is the work item
 *
 * 0 is returned if the work item is added.
 * -1 is returned if the queue is full.
 */
int work_queue_enqueue(struct work_queue *wq, void *work);

/* work_queue_dequeue() removes a work item from the queue
 * and returns it, unless the queue is empty, in which case
 * NULL is returned.
 *
 * This function is useful if the thread_count of the work
 * queue is zero.
 */
void *work_queue_dequeue(struct work_queue *wq);

/* Empty out a work queue and stop all threads
 * work items still in the queue are discarded
 */
void work_queue_shutdown(struct work_queue *wq);

/* Block new items from being added to the queue
 * and allow the queue to emptiness.  Only valid
 * with work queues with thread_count > 0
 */
void work_queue_drain(struct work_queue *wq);

/* Frees memory allocated to the work queue.  This is the
 * inverse of work_queue_init().
 */
void work_queue_free(struct work_queue *wq);

#endif
