/* kernel/alloc.h  —  Simple memory allocator */

#ifndef ALLOC_H
#define ALLOC_H

#include <stddef.h>

void alloc_init(void *heap_start, size_t heap_size);
void *kmalloc(size_t size);
void kfree(void *ptr);

#endif
