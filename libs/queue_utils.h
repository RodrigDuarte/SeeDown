/* FILE: queue_utils.h
 * AUTHORS: Rodrigo Duarte, with some code from the "Algorithms and Data Structures" course lectured by Prof. Carlos Barrico
 * DESCRIPTION: This file contains the implementation of queues with thread safe operations.
 */


// Queue node structure
struct queue_node {
    void *data;

    struct queue_node *next;
};

typedef struct queue_node *QUEUE_NODE;

// Queue structure
struct queue {
    int size;               // The number of elements in the queue

    QUEUE_NODE head;       // The head of the queue
    QUEUE_NODE tail;       // The tail of the queue

    pthread_mutex_t mutex;  // The mutex to protect the queue
};

typedef struct queue *QUEUE;

QUEUE create_queue();
QUEUE_NODE create_queue_node(void *data);
void *dequeue(QUEUE queue);
int enqueue(QUEUE queue, void *data);

/* Function to create a new queue
 * Returns a pointer to the new queue or NULL if there is an error
 */
QUEUE create_queue() {
    QUEUE queue = (QUEUE) malloc(sizeof(struct queue));
    if (queue == NULL)
        return NULL;

    queue->size = 0;
    queue->head = NULL;
    queue->tail = NULL;

    // Initialize the mutex
    pthread_mutex_init(&queue->mutex, NULL);

    return queue;
}

/* Function to create a new queue node
 * Returns a pointer to the new queue node or NULL if there is an error
 */
QUEUE_NODE create_queue_node(void *data) {
    QUEUE_NODE node = (QUEUE_NODE) malloc(sizeof(struct queue_node));
    if (node == NULL)
        return NULL;

    node->data = data;
    node->next = NULL;

    return node;
}

/* Funtion to dequeue a node from the queue
 * Returns the removed package of the queue or NULL if there is an error
 */
void *dequeue(QUEUE queue) {
    if (queue->head == NULL)
        return NULL;

    QUEUE_NODE tmp;
    void *data;

    pthread_mutex_lock(&queue->mutex);  // Lock the queue
    
    if (queue->head == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    if (queue->size == 1) {
        tmp = queue->head;
        queue->head = NULL;
        queue->tail = NULL;
    } else {
        tmp = queue->head;
        queue->head = tmp->next;
    }

    (queue->size)--;

    data = tmp->data;

    pthread_mutex_unlock(&queue->mutex); // Unlock the queue

    free(tmp);

    return data;
}

/* Function to enqueue a node to the queue
 * Returns the new size of the queue or -1 if there is an error
 */
int enqueue(QUEUE queue, void *data) {
    QUEUE_NODE node = create_queue_node(data);
    if (node == NULL)
        return -1;

    pthread_mutex_lock(&queue->mutex);  // Lock the queue

    if (queue->size == 0) {
        queue->head = node;
        queue->tail = node;
    } else {
        queue->tail->next = node;
        queue->tail = node;
    }

    (queue->size)++;

    pthread_mutex_unlock(&queue->mutex); // Unlock the queue

    return queue->size;
}
