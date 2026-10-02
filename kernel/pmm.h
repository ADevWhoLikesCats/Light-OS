#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PAGE_SIZE 4096

void     pmm_init(void);
void    *pmm_alloc_page(void);
void     pmm_free_page(void *page);
uint64_t pmm_total_pages(void);
uint64_t pmm_free_pages(void);

#endif
