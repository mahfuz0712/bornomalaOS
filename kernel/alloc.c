/* kernel/alloc.c  —  Simple memory allocator implementation */

#include "alloc.h"
#include <stdint.h>

/* Memory block header for tracking allocations */
typedef struct alloc_header {
    size_t size;
    int is_free;
    struct alloc_header *next;
} alloc_header_t;

static uint8_t *heap_start = NULL;
static size_t heap_size = 0;
static uint8_t *heap_ptr = NULL;
static alloc_header_t *free_list = NULL;

void alloc_init(void *start, size_t size) {
    heap_start = (uint8_t *)start;
    heap_size = size;
    heap_ptr = heap_start;
    free_list = NULL;
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    /* Align size to 8-byte boundary */
    size = (size + 7) & ~7;

    /* Search free list for suitable block */
    alloc_header_t *current = free_list;
    alloc_header_t *prev = NULL;

    while (current) {
        if (current->is_free && current->size >= size + sizeof(alloc_header_t)) {
            /* Found a suitable block */
            if (current->size > size + sizeof(alloc_header_t)) {
                /* Split block if too large */
                alloc_header_t *new_block = (alloc_header_t *)((uint8_t *)current + sizeof(alloc_header_t) + size);
                new_block->size = current->size - size - sizeof(alloc_header_t);
                new_block->is_free = 1;
                new_block->next = current->next;
                current->next = new_block;
            }
            current->size = size;
            current->is_free = 0;

            /* Remove from free list */
            if (prev) prev->next = current->next;
            else free_list = current->next;

            return (void *)((uint8_t *)current + sizeof(alloc_header_t));
        }
        prev = current;
        current = current->next;
    }

    /* No suitable block in free list, allocate from heap bump pointer */
    if (heap_ptr + sizeof(alloc_header_t) + size > heap_start + heap_size) {
        return NULL;
    }

    alloc_header_t *header = (alloc_header_t *)heap_ptr;
    header->size = size;
    header->is_free = 0;
    header->next = NULL;

    heap_ptr += sizeof(alloc_header_t) + size;

    return (void *)((uint8_t *)header + sizeof(alloc_header_t));
}

void kfree(void *ptr) {
    if (!ptr) return;

    alloc_header_t *header = (alloc_header_t *)((uint8_t *)ptr - sizeof(alloc_header_t));
    header->is_free = 1;

    /* Add to free list */
    header->next = free_list;
    free_list = header;
}
