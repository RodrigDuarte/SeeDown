/* FILE: array_utils.h
 * AUTHORS: Rodrigo Duarte
 * DESCRIPTION: This file contains the implementation of dynamic arrays.
 */


// Array structure
struct array {
    void **data;

    int step;
    int size;
    int capacity;
};

typedef struct array *ARRAY;

ARRAY create_array(int step);
ARRAY increase_array(ARRAY array);
ARRAY reduce_array(ARRAY array);
ARRAY add_array(ARRAY array, void *data);
ARRAY remove_array(ARRAY array, int index);
ARRAY clear_array(ARRAY array);
ARRAY destroy_array(ARRAY array);

/* Function to create a new array */
ARRAY create_array(int step) {
    ARRAY test = (ARRAY) malloc(sizeof(struct array));

    test->data = (void **) malloc(sizeof(void *) * step);
    test->step = step;
    test->size = 0;
    test->capacity = step;

    return test;
}

/* Makes the array bigger by step value */
ARRAY increase_array(ARRAY array) {
    array->capacity += array->step;

    void **data = (void **) malloc(sizeof(void *) * array->capacity);
    memcpy(data, array->data, sizeof(void *) * array->size);
    free(array->data);
    array->data = data;

    return array;
}

ARRAY reduce_array(ARRAY array) {
    array->capacity -= array->step;

    void **data = (void **) malloc(sizeof(void *) * array->capacity);
    memcpy(data, array->data, sizeof(void *) * array->size);

    free(array->data);
    array->data = data;

    return array;
}

ARRAY add_array(ARRAY array, void *data) {
    if (array->size == array->capacity) {
        array = increase_array(array);
    }

    array->data[array->size++] = data;

    return array;
}

ARRAY remove_array(ARRAY array, int index) {
    if (index < 0 || index >= array->size) {
        return array;
    }

    free(array->data[index]);
    array->data[index] = NULL;

    for (int i = index; i < array->size - 1; i++) {
        array->data[i] = array->data[i + 1];
    }

    array->size--;

    if (array->size > 0 && array->size == (array->capacity - array->step)) {
        array = reduce_array(array);
    }

    return array;
}

ARRAY clear_array(ARRAY array) {
    int step = array->step;

    destroy_array(array);

    array = create_array(step);

    return array;
}

ARRAY destroy_array(ARRAY array) {
    for (int i = 0; i < array->size; i++) {
        free(array->data[i]);
        array->data[i] = NULL;
    }

    free(array->data);
    array->data = NULL;
    free(array);
    array = NULL;

    return array;
}