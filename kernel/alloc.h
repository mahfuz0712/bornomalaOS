/* kernel/alloc.h - kernel heap (first-fit, splitting + coalescing, 16-byte aligned) */
#ifndef BORNOMALA_ALLOC_H
#define BORNOMALA_ALLOC_H

#include <stddef.h>

void  alloc_init(void *heap_start, size_t heap_size);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void  kfree(void *ptr);
size_t alloc_free_bytes(void);

#endif
