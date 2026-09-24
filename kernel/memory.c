#include "memory.h"
#include "klib.h"
#include "console.h"

/* The boot page tables identity-map the first 4 GiB, so that is all we manage. */
#define MAX_PAGES   (1ULL << 20)                /* 4 GiB / 4 KiB */
#define BITMAP_SIZE (MAX_PAGES / 8)

extern char __kernel_start[], __kernel_end[];   /* from boot/linker.ld */

static uint8_t  page_bitmap[BITMAP_SIZE];       /* 1 = in use / not RAM */
static uint64_t usable_bytes, free_pages, highest_addr;
static uint64_t scan_hint;

static inline bool bit_test(uint64_t p)  { return (page_bitmap[p >> 3] >> (p & 7)) & 1u; }
static inline void bit_set(uint64_t p)   { page_bitmap[p >> 3] |= (uint8_t)(1u << (p & 7)); }
static inline void bit_clear(uint64_t p) { page_bitmap[p >> 3] &= (uint8_t)~(1u << (p & 7)); }

static void reserve_range(uint64_t start, uint64_t end) {
    start &= ~(PAGE_SIZE - 1);                    /* round OUT: partially used pages are reserved */
    end = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    if (end > MAX_PAGES * PAGE_SIZE) end = MAX_PAGES * PAGE_SIZE;
    for (uint64_t a = start; a < end; a += PAGE_SIZE) {
        uint64_t p = a / PAGE_SIZE;
        if (!bit_test(p)) { bit_set(p); free_pages--; }
    }
}

void memory_init(const boot_info_t *bi) {
    memset(page_bitmap, 0xFF, sizeof(page_bitmap));
    usable_bytes = free_pages = highest_addr = scan_hint = 0;

    for (uint32_t i = 0; i < bi->mmap_count; i++) {
        const mb2_mmap_entry_t *e = &bi->mmap[i];
        if (e->type != 1) continue;
        uint64_t s = (e->addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);      /* round IN */
        uint64_t en = (e->addr + e->len) & ~(PAGE_SIZE - 1);
        if (en > MAX_PAGES * PAGE_SIZE) en = MAX_PAGES * PAGE_SIZE;
        if (s >= en) continue;
        for (uint64_t a = s; a < en; a += PAGE_SIZE) {
            uint64_t p = a / PAGE_SIZE;
            if (bit_test(p)) { bit_clear(p); free_pages++; usable_bytes += PAGE_SIZE; }
        }
        if (en > highest_addr) highest_addr = en;
    }

    /* Never hand out: low memory, the kernel image (incl. page tables, stack,
       this bitmap), or the bootloader's info block. */
    reserve_range(0, 0x100000);
    reserve_range((uint64_t)(uintptr_t)__kernel_start, (uint64_t)(uintptr_t)__kernel_end);
    if (bi->valid) reserve_range(bi->info_start, bi->info_end);
}

void *pmm_alloc_page(void) {
    for (uint64_t n = 0; n < MAX_PAGES; n++) {
        uint64_t p = (scan_hint + n) % MAX_PAGES;
        if (!bit_test(p)) {
            bit_set(p); free_pages--;
            scan_hint = p + 1;
            void *pg = (void *)(uintptr_t)(p * PAGE_SIZE);
            memset(pg, 0, PAGE_SIZE);
            return pg;
        }
    }
    return NULL;
}

void pmm_free_page(void *page) {
    uint64_t a = (uint64_t)(uintptr_t)page;
    if (a & (PAGE_SIZE - 1)) return;
    uint64_t p = a / PAGE_SIZE;
    if (p >= MAX_PAGES || !bit_test(p)) return;
    bit_clear(p); free_pages++;
    if (p < scan_hint) scan_hint = p;
}

void *pmm_alloc_contiguous(size_t pages) {
    if (pages == 0) return NULL;
    uint64_t run = 0, run_start = 0;
    for (uint64_t p = 0x100000 / PAGE_SIZE; p < MAX_PAGES; p++) {
        if (bit_test(p)) { run = 0; continue; }
        if (run == 0) run_start = p;
        if (++run == pages) {
            for (uint64_t q = run_start; q < run_start + pages; q++) bit_set(q);
            free_pages -= pages;
            void *base = (void *)(uintptr_t)(run_start * PAGE_SIZE);
            memset(base, 0, pages * PAGE_SIZE);
            return base;
        }
    }
    return NULL;
}

void pmm_free_contiguous(void *addr, size_t pages) {
    uint64_t a = (uint64_t)(uintptr_t)addr;
    for (size_t i = 0; i < pages; i++) pmm_free_page((void *)(uintptr_t)(a + i * PAGE_SIZE));
}

uint64_t memory_total_bytes(void)  { return highest_addr; }
uint64_t memory_usable_bytes(void) { return usable_bytes; }
uint64_t memory_free_bytes(void)   { return free_pages * PAGE_SIZE; }
