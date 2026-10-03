#include "mm.h"
#include "vmm.h"
#include "pmm.h"
#include "serial.h"

#define PAGE 4096ULL

/* Per-process (for now, global — single process model) break pointer. */
static uint64_t brk_start = 0;
static uint64_t brk_current = 0;

/* Simple bump allocator for mmap. */
static uint64_t mmap_next = MMAP_BASE;

void mm_init(void)
{
    /* Place the initial break just past the loaded ELF + stack.
       We'll set it explicitly from enter_userspace_elf, but default
       to something safe in case mm_init runs standalone. */
    brk_start   = 0x50000000ULL;
    brk_current = brk_start;
    mmap_next   = MMAP_BASE;
}

uint64_t mm_brk(uint64_t new_brk)
{
    /* Lazy init in case mm_init wasn't called. */
    if (brk_current == 0) {
        brk_start = 0x50000000ULL;
        brk_current = brk_start;
    }

    if (new_brk == 0) return brk_current;

    /* Grow: allocate pages and map them. */
    if (new_brk > brk_current) {
        uint64_t old_page = (brk_current + PAGE - 1) / PAGE;
        uint64_t new_page = (new_brk + PAGE - 1) / PAGE;

        for (uint64_t p = old_page; p < new_page; p++) {
            void *phys = pmm_alloc_page();
            if (!phys) {
                serial_print("mm_brk: out of memory\n");
                return brk_current;
            }
            /* Zero the page. */
            uint8_t *dst = (uint8_t *)phys;
            for (int i = 0; i < 4096; i++) dst[i] = 0;
            vmm_map_page_user(p * PAGE, (uint64_t)phys,
                              PTE_WRITE | PTE_USER);
        }
    }

    brk_current = new_brk;
    return brk_current;
}

uint64_t mm_mmap(uint64_t addr, uint64_t length, uint64_t prot,
                 uint64_t flags, int fd, uint64_t offset)
{
    (void)fd; (void)offset;
    if (length == 0) return (uint64_t)-1;
    if (!(flags & MAP_ANONYMOUS)) {
        serial_print("mmap: only MAP_ANONYMOUS supported\n");
        return (uint64_t)-1;
    }

    uint64_t len_pages = (length + PAGE - 1) / PAGE;
    uint64_t virt;

    if (flags & MAP_FIXED) {
        virt = addr & ~(PAGE - 1);
    } else {
        virt = mmap_next;
        mmap_next += len_pages * PAGE;
        if (mmap_next > MMAP_LIMIT) {
            serial_print("mmap: out of address space\n");
            return (uint64_t)-1;
        }
    }

    for (uint64_t i = 0; i < len_pages; i++) {
        void *phys = pmm_alloc_page();
        if (!phys) {
            serial_print("mmap: out of physical memory\n");
            return (uint64_t)-1;
        }
        uint8_t *dst = (uint8_t *)phys;
        for (int j = 0; j < 4096; j++) dst[j] = 0;

        uint64_t pte_flags = PTE_USER;
        if (prot & PROT_WRITE) pte_flags |= PTE_WRITE;
        vmm_map_page_user(virt + i * PAGE, (uint64_t)phys, pte_flags);
    }

    return virt;
}

int mm_munmap(uint64_t addr, uint64_t length)
{
    (void)addr; (void)length;
    /* No-op for now. Free list comes later. */
    return 0;
}
