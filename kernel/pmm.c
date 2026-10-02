#include "pmm.h"
#include "memmap.h"

/* Bitmap: 1 = used, 0 = free. Sized for up to 256 MiB of RAM. */
#define MAX_PAGES       (256ULL * 1024 * 1024 / PAGE_SIZE)   /* 65536 */
#define BITMAP_BYTES    (MAX_PAGES / 8)                       /* 8192 */

static uint8_t pmm_bitmap[BITMAP_BYTES];
static uint64_t total_pages = 0;
static uint64_t free_pages  = 0;

static inline void bitmap_set(uint64_t idx)
{
    pmm_bitmap[idx >> 3] |= (uint8_t)(1u << (idx & 7));
}

static inline void bitmap_clear(uint64_t idx)
{
    pmm_bitmap[idx >> 3] &= (uint8_t)~(1u << (idx & 7));
}

static inline int bitmap_test(uint64_t idx)
{
    return (pmm_bitmap[idx >> 3] >> (idx & 7)) & 1;
}

extern char __kernel_end[];

void pmm_init(void)
{
    for (uint64_t i = 0; i < BITMAP_BYTES; i++) {
        pmm_bitmap[i] = 0xFF;
    }

    const struct e820_map *map = (const struct e820_map *)E820_PHYS_ADDR;

    for (uint32_t i = 0; i < map->count && i < E820_MAX_ENTRIES; i++) {
        const struct e820_entry *e = &map->entries[i];
        if (e->type != E820_USABLE) continue;

        uint64_t start = (e->base + PAGE_SIZE - 1) / PAGE_SIZE;
        uint64_t end   = (e->base + e->length) / PAGE_SIZE;

        for (uint64_t p = start; p < end && p < MAX_PAGES; p++) {
            bitmap_clear(p);
        }
    }

    /* Reserve low 1 MiB. */
    for (uint64_t p = 0; p < (0x100000 / PAGE_SIZE); p++) {
        bitmap_set(p);
    }

    /* Reserve kernel + bitmap (up to __kernel_end). */
    uint64_t kernel_last = ((uint64_t)__kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t p = 0x100000 / PAGE_SIZE; p < kernel_last && p < MAX_PAGES; p++) {
        bitmap_set(p);
    }

    total_pages = MAX_PAGES;
    free_pages  = 0;
    for (uint64_t p = 0; p < MAX_PAGES; p++) {
        if (!bitmap_test(p)) free_pages++;
    }
}

void *pmm_alloc_page(void)
{
    for (uint64_t p = 0; p < MAX_PAGES; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            free_pages--;
            return (void *)(p * PAGE_SIZE);
        }
    }
    return 0;
}

void pmm_free_page(void *page)
{
    uint64_t idx = (uint64_t)page / PAGE_SIZE;
    if (idx >= MAX_PAGES) return;
    if (bitmap_test(idx)) {
        bitmap_clear(idx);
        free_pages++;
    }
}

uint64_t pmm_total_pages(void) { return total_pages; }
uint64_t pmm_free_pages(void)  { return free_pages; }
