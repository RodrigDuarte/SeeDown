/* FILE: list_utils.h
 * AUTHORS: Rodrigo Duarte, with some code from the "Algorithms and Data Structures" course lectured by Prof. Carlos Barrico
 * DESCRIPTION: This file contains the implementation of lists with thread safe operations.
 */


// List node structure
struct list_node {
    void *data;

    struct list_node *next;
};

typedef struct list_node *LIST_NODE;

// List structure
struct list {
    int size;

    LIST_NODE head;
    LIST_NODE tail;

    pthread_mutex_t mutex;
};

typedef struct list *LIST;

LIST create_list();
LIST_NODE create_list_node(void *data);
LIST_NODE free_list_node(LIST_NODE node);
void print_list(LIST list);
int is_in_list(LIST list, void *data);
LIST add_head_list(LIST list, void *data);
LIST add_tail_list(LIST list, void *data);
LIST add_after_list(LIST list, void *data, void *after);
LIST remove_list(LIST list, void *data);
LIST destroy_list(LIST list);

LIST create_list() {
    LIST list = (LIST) malloc(sizeof(struct list));
    if (list == NULL) {
        fprintf(stderr, "Error: could not create list\n");
        return NULL;
    }

    list->size = 0;
    list->head = NULL;
    list->tail = NULL;

    pthread_mutex_init(&list->mutex, NULL);
    return list;
}

LIST_NODE create_list_node(void *data) {
    LIST_NODE node = (LIST_NODE) malloc(sizeof(struct list_node));
    if (node == NULL) {
        fprintf(stderr, "Error: could not create list node\n");
        return NULL;
    }

    node->data = data;
    node->next = NULL;

    return node;
}

LIST_NODE free_list_node(LIST_NODE node) {
    free(node);
    return NULL;
}

void print_list(LIST list) {
    pthread_mutex_lock(&list->mutex);

    LIST_NODE node = list->head;

    while (node != NULL) {
        printf("%p\n", node->data);
        node = node->next;
    }

    pthread_mutex_unlock(&list->mutex);

    return;
}

int is_in_list(LIST list, void *data) {
    pthread_mutex_lock(&list->mutex);

    LIST_NODE node = list->head;

    while (node != NULL) {
        if (node->data == data) {
            return 1;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&list->mutex);

    return 0;
}

LIST add_head_list(LIST list, void *data) {
    LIST_NODE node = create_list_node(data);
    if (node == NULL) {
        fprintf(stderr, "Error: could not create list node\n");
        return NULL;
    }

    pthread_mutex_lock(&list->mutex);

    if (list->head == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        node->next = list->head;
        list->head = node;
    }
    list->size++;

    pthread_mutex_unlock(&list->mutex);

    return list;
}

LIST add_tail_list(LIST list, void *data) {
    LIST_NODE node = create_list_node(data);
    if (node == NULL) {
        fprintf(stderr, "Error: could not create list node\n");
        return NULL;
    }

    pthread_mutex_lock(&list->mutex);

    if (list->head == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }
    list->size++;

    pthread_mutex_unlock(&list->mutex);

    return list;
}

LIST add_after_list(LIST list, void *data, void *after) {
    LIST_NODE node = create_list_node(data);
    if (node == NULL) {
        fprintf(stderr, "Error: could not create list node\n");
        return NULL;
    }

    pthread_mutex_lock(&list->mutex);

    LIST_NODE current = list->head;
    while (current != NULL) {
        if (current->data == after) {
            node->next = current->next;
            current->next = node;
            list->size++;
            break;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&list->mutex);

    return list;
}

LIST remove_list(LIST list, void *data) {
    pthread_mutex_lock(&list->mutex);

    LIST_NODE current = list->head;
    LIST_NODE previous = NULL;
    while (current != NULL) {
        if (current->data == data) {
            if (previous == NULL) {
                list->head = current->next;
            } else {
                previous->next = current->next;
            }
            list->size--;
            break;
        }
        previous = current;
        current = current->next;
    }

    pthread_mutex_unlock(&list->mutex);

    return list;
}

LIST destroy_list(LIST list) {
    pthread_mutex_lock(&list->mutex);

    LIST_NODE current = list->head;
    while (current != NULL) {
        LIST_NODE next = current->next;
        free(current->data);
        current = free_list_node(current);
        current = next;
    }

    pthread_mutex_unlock(&list->mutex);

    pthread_mutex_destroy(&list->mutex);
    free(list);
    return NULL;
}