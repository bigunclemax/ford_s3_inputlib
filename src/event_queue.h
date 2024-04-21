#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

struct _node {
	void *data;
	struct _node *next;
};
typedef struct _node queue_node_t;

struct _evt_q {
	queue_node_t *first;
	queue_node_t *last;
};
typedef struct _evt_q queue_t;

queue_t *queue_factory(void);
void    queue_destroy(queue_t *que);
int     enque(queue_t *que, void *data);
void    *deque(queue_t *que);

#endif
