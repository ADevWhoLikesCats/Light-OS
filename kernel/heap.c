#include "heap.h"
#include "vmm.h"
#include "pmm.h"
#include "serial.h"

struct block {
    size_t        size;
    int           free;
    struct block *next;
    struct block *prev;
};

#define HDR_SIZE         sizeof(struct block)
#define ALIGN16(x)       (((x) + 15) & ~15ULL)
#define BLOCK_TOTAL(sz)  (ALIGN16(HDR_SIZE + (sz)))

static struct block *head = 0;


void heap_init(void)
{
    uint64_t pages = HEAP_SIZE / PAGE_SIZE;
    for (uint64_t i = 0; i < pages; i++) {
        void *phys = pmm_alloc_page();
        vmm_map_page(HEAP_BASE + i * PAGE_SIZE, (uint64_t)phys, PTE_WRITE);
    }

    head = (struct block *)HEAP_BASE;
    head->size = HEAP_SIZE - HDR_SIZE;
    head->free = 1;
    head->next = 0;
    head->prev = 0;
}

static void split_block(struct block *b, size_t n)
{
    size_t need = BLOCK_TOTAL(n);
    if (BLOCK_TOTAL(b->size) <= need) return;

    struct block *nb = (struct block *)((uint64_t)b + need);
    nb->size = (BLOCK_TOTAL(b->size) - need) - HDR_SIZE;
    nb->free = 1;
    nb->next = b->next;
    nb->prev = b;
    if (b->next) b->next->prev = nb;
    b->next = nb;
    b->size = n;
}

void *kmalloc(size_t n)
{
    if (n == 0) return 0;

    struct block *b = head;
    while (b) {
        if (b->free && b->size >= n) break;
        b = b->next;
    }
    if (!b) return 0;

    split_block(b, n);
    b->free = 0;

    return (void *)((uint64_t)b + HDR_SIZE);
}

static void coalesce_next(struct block *b)
{
    struct block *nb = b->next;
    if (!nb || !nb->free) return;

    size_t span = BLOCK_TOTAL(b->size) + BLOCK_TOTAL(nb->size);
    b->size = span - HDR_SIZE;
    b->next = nb->next;
    if (nb->next) nb->next->prev = b;
}

void kfree(void *p)
{
    if (!p) return;

    struct block *b = (struct block *)((uint64_t)p - HDR_SIZE);
    if (b->free) return;

    b->free = 1;

    coalesce_next(b);
    if (b->prev && b->prev->free) {
        coalesce_next(b->prev);
    }
}

void heap_stats(void)
{
    uint64_t used = 0, freeb = 0, blocks = 0, free_blocks = 0;

    for (struct block *b = head; b; b = b->next) {
        blocks++;
        if (b->free) { freeb += b->size; free_blocks++; }
        else         { used  += b->size; }
    }

}
