#include "alloc.h"
#include "klib.h"
#include <stdint.h>

#define ALLOC_MAGIC 0xB0A1CAFEu
#define ALIGN       16u

typedef struct block {
    uint32_t      magic;
    uint32_t      is_free;
    size_t        size;          /* payload bytes */
    struct block *next;          /* address-ordered list of ALL blocks */
    struct block *prev;
} block_t;                        /* 32 bytes: payload stays 16-byte aligned */

#define HDR ((size_t)sizeof(block_t))

static block_t *first;
static size_t   heap_total;

void alloc_init(void *start, size_t size) {
    uintptr_t s = ((uintptr_t)start + ALIGN - 1) & ~(uintptr_t)(ALIGN - 1);
    size -= (size_t)(s - (uintptr_t)start);
    size &= ~(size_t)(ALIGN - 1);
    first = 0; heap_total = 0;
    if (size <= HDR + ALIGN) return;
    first = (block_t *)s;
    first->magic = ALLOC_MAGIC;
    first->is_free = 1;
    first->size = size - HDR;
    first->next = first->prev = 0;
    heap_total = size;
}

void *kmalloc(size_t size) {
    if (size == 0 || !first) return 0;
    size = (size + ALIGN - 1) & ~(size_t)(ALIGN - 1);
    for (block_t *b = first; b; b = b->next) {
        if (!b->is_free || b->size < size) continue;
        if (b->size >= size + HDR + ALIGN) {           /* split */
            block_t *n = (block_t *)((uint8_t *)b + HDR + size);
            n->magic = ALLOC_MAGIC;
            n->is_free = 1;
            n->size = b->size - size - HDR;
            n->next = b->next;
            n->prev = b;
            if (b->next) b->next->prev = n;
            b->next = n;
            b->size = size;
        }
        b->is_free = 0;
        return (uint8_t *)b + HDR;
    }
    return 0;
}

void *kcalloc(size_t count, size_t size) {
    if (size && count > (size_t)-1 / size) return 0;
    size_t total = count * size;
    void *p = kmalloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_t *b = (block_t *)((uint8_t *)ptr - HDR);
    if (b->magic != ALLOC_MAGIC || b->is_free) return;    /* bad or double free: ignore */
    b->is_free = 1;
    if (b->next && b->next->is_free) {                    /* coalesce forward */
        b->size += HDR + b->next->size;
        b->next = b->next->next;
        if (b->next) b->next->prev = b;
    }
    if (b->prev && b->prev->is_free) {                    /* coalesce backward */
        b->prev->size += HDR + b->size;
        b->prev->next = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

size_t alloc_free_bytes(void) {
    size_t n = 0;
    for (block_t *b = first; b; b = b->next) if (b->is_free) n += b->size;
    return n;
}
