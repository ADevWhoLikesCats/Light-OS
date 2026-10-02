#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITE    (1ULL << 1)
#define PTE_USER     (1ULL << 2)

#define VMM_BASE     0x40000000ULL
#define VMM_LIMIT    0x80000000ULL

void  vmm_init(void);
int   vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
int   vmm_map_page_user(uint64_t virt, uint64_t phys, uint64_t flags);
void  vmm_unmap_page(uint64_t virt);

#endif
