#pragma once

typedef struct {
    void* data;
    int elem_size;
    int capacity;
    int length;
} Vec;

Vec Vec_new(int elem_size);

Vec Vec_with_capacity(int elem_size, int capacity);

void Vec_push(Vec* v, void* elem);

void* Vec_get(Vec* v, int i);

void Vec_free(Vec* v);

void Vec_free_data(Vec* v);