#include "vmm.h"
#include "pmm.h"

static inline uint64_t read_cr3(void)
{
    uint64_t v;
    __asm__ volatile("mov %%cr3, %0" : "=r"(v));
    return v;
}

static inline void invlpg(uint64_t v)
{
    __asm__ volatile("invlpg (%0)" :: "r"(v) : "memory");
}

static uint64_t get_or_create_pt(uint64_t virt)
{
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;

    uint64_t pml4_phys = read_cr3() & ~0xFFFULL;
    uint64_t *pml4 = (uint64_t *)pml4_phys;

    if (!(pml4[pml4_idx] & PTE_PRESENT)) return 0;

    uint64_t pdpt_phys = pml4[pml4_idx] & ~0xFFFULL;
    uint64_t *pdpt = (uint64_t *)pdpt_phys;

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        uint64_t new_pd = (uint64_t)pmm_alloc_page();
        if (!new_pd) return 0;
        for (int i = 0; i < 512; i++) ((uint64_t *)new_pd)[i] = 0;
        pdpt[pdpt_idx] = new_pd | PTE_PRESENT | PTE_WRITE;
    }

    uint64_t pd_phys = pdpt[pdpt_idx] & ~0xFFFULL;
    uint64_t *pd = (uint64_t *)pd_phys;

    if (!(pd[pd_idx] & PTE_PRESENT)) {
        uint64_t new_pt = (uint64_t)pmm_alloc_page();
        if (!new_pt) return 0;
        for (int i = 0; i < 512; i++) ((uint64_t *)new_pt)[i] = 0;
        pd[pd_idx] = new_pt | PTE_PRESENT | PTE_WRITE;
    }

    return pd[pd_idx] & ~0xFFFULL;
}

void vmm_init(void)
{
    (void)get_or_create_pt(VMM_BASE);
}

int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags)
{
    if (virt < VMM_BASE || virt >= VMM_LIMIT) return -1;

    uint64_t pt_phys = get_or_create_pt(virt);
    if (!pt_phys) return -1;

    uint64_t pt_idx = (virt >> 12) & 0x1FF;
    uint64_t *pt = (uint64_t *)pt_phys;

    pt[pt_idx] = (phys & ~0xFFFULL) | (flags & 0xFFF) | PTE_PRESENT;
    invlpg(virt);
    return 0;
}

void vmm_unmap_page(uint64_t virt)
{
    if (virt < VMM_BASE || virt >= VMM_LIMIT) return;

    uint64_t pt_phys = get_or_create_pt(virt);
    if (!pt_phys) return;

    uint64_t pt_idx = (virt >> 12) & 0x1FF;
    uint64_t *pt = (uint64_t *)pt_phys;

    pt[pt_idx] = 0;
    invlpg(virt);
}
