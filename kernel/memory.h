/* kernel/memory.h - physical page-frame allocator (bitmap, first 4 GiB) */
#ifndef BORNOMALA_MEMORY_H
#define BORNOMALA_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "mb2.h"

#define PAGE_SIZE 4096ULL

void     memory_init(const boot_info_t *bi);
void    *pmm_alloc_page(void);
void     pmm_free_page(void *page);
/* Allocates `pages` physically contiguous, zeroed pages. Returns NULL on failure. */
void    *pmm_alloc_contiguous(size_t pages);
void     pmm_free_contiguous(void *addr, size_t pages);

uint64_t memory_total_bytes(void);      /* highest usable address seen */
uint64_t memory_usable_bytes(void);     /* usable RAM at boot */
uint64_t memory_free_bytes(void);

#endif
