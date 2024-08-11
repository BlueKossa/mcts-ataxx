#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "vec.h"

Vec Vec_new(int elem_size) {
    Vec v;
    v.data = malloc(elem_size * 8);
    v.elem_size = elem_size;
    v.capacity = 8;
    v.length = 0;
    return v;
}

Vec Vec_with_capacity(int elem_size, int capacity) {
    Vec v;
    v.data = malloc(elem_size * capacity);
    v.elem_size = elem_size;
    v.capacity = capacity;
    v.length = 0;
    return v;
}

void Vec_push(Vec* v, void* elem) {
    if (v->length + 1 > v->capacity) {
        v->data = realloc(v->data, v->elem_size * v->capacity * 2);
        v->capacity *= 2;
    }
    memcpy(v->data + v->elem_size * v->length, elem, v->elem_size);
    v->length++;
}

void* Vec_get(Vec* v, int i) {
    return v->data + v->elem_size * i;
}

void Vec_free(Vec* v) {
    free(v->data);
    free(v);
}

void Vec_free_data(Vec* v) {
    free(v->data);
}