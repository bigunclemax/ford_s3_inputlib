#include "event_queue.h"

#include <stdlib.h>
#include <pthread.h>

#ifdef DEBUG
#define print printf
#else
#define print(...)
#endif

// FIXME: get rid of global mutex
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

queue_t *queue_factory()
{
	queue_t *new_queue = malloc(sizeof(queue_t));
	if (new_queue == NULL) {
		print("Malloc failed creating the que\n");
		return NULL;
	}

	new_queue->first = NULL;
	new_queue->last = NULL;

	print("Generated the que @ %p\n", new_queue);

	return new_queue;
}

void queue_destroy(queue_t *que)
{
	print("Enter to queue_destroy\n");

	if (que == NULL)
		return;

	print("que is not null, que = %p\n", que);

	pthread_mutex_lock(&g_mutex);
	if (que->first == NULL) {
		print("que->first == NULL .... \n");
		free(que);
		pthread_mutex_unlock(&g_mutex);
		return;
	}

	print("que is there lets try to free it...\n");

	queue_node_t *_node = que->first;

	while (_node != NULL) {
		// freeing the data coz it's on the heap and no one to free it
		// except for this one
		print("freeing : %s\n", (char *) _node->data);
		free(_node->data);
		queue_node_t *tmp = _node->next;
		free(_node);
		_node = tmp;
	}

	free(que);

	pthread_mutex_unlock(&g_mutex);
}

int enque(queue_t *que, void *data)
{
	queue_node_t *new_node = malloc(sizeof(queue_node_t));
	if (new_node == NULL) {
		print("Malloc failed creating a node\n");
		return -1;
	}

	/* assuming data is in heap */
	new_node->data = data;
	new_node->next = NULL;

	pthread_mutex_lock(&g_mutex);
	if (que->first == NULL) {
		/* new/empty q */
		que->first = new_node;
		que->last = new_node;
	} else {
		que->last->next = new_node;
		que->last = new_node;
	}
	pthread_mutex_unlock(&g_mutex);

	return 0;
}

void *deque(queue_t *que)
{
	print("Entered to deque\n");
	if (que == NULL) {
		print("que is null exiting...\n");
		return NULL;
	}

	pthread_mutex_lock(&g_mutex);
	if (que->first == NULL) {
		pthread_mutex_unlock(&g_mutex);
		print("que->first is null exiting...\n");
		return NULL;
	}

	void *data;
	queue_node_t *_node = que->first;
	if (que->first == que->last) {
		que->first = NULL;
		que->last = NULL;
	} else {
		que->first = _node->next;
	}

	data = _node->data;

	print("Freeing _node@ %p", _node);
	free(_node);
	pthread_mutex_unlock(&g_mutex);
	print("Exiting deque\n");

	return data;
}
