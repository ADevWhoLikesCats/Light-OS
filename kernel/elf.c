#include "elf.h"
#include "vmm.h"
#include "pmm.h"

#define EI_NIDENT 16

typedef struct {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) Elf64_Phdr;

#define PT_LOAD 1
#define PF_X 1
#define PF_W 2
#define PF_R 4
#define PAGE_SIZE 4096

uint64_t elf_load(const void *elf_data, uint64_t elf_size, uint64_t load_bias)
{
    const uint8_t *base = (const uint8_t *)elf_data;

    if (elf_size < sizeof(Elf64_Ehdr)) return 0;
    if (base[0] != 0x7F || base[1] != 'E' || base[2] != 'L' || base[3] != 'F') return 0;
    if (base[4] != 2 || base[5] != 1) return 0;

    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)base;
    const Elf64_Phdr *ph = (const Elf64_Phdr *)(base + eh->e_phoff);

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        const Elf64_Phdr *p = &ph[i];
        if (p->p_type != PT_LOAD) continue;

        uint64_t vaddr  = p->p_vaddr + load_bias;
        uint64_t memsz  = p->p_memsz;
        uint64_t filesz = p->p_filesz;
        uint64_t offset = p->p_offset;

        uint64_t page_start = vaddr & ~0xFFFULL;
        uint64_t page_end   = (vaddr + memsz + 0xFFF) & ~0xFFFULL;
        uint64_t npages     = (page_end - page_start) / PAGE_SIZE;

        for (uint64_t pg = 0; pg < npages; pg++) {
            uint64_t page_va = page_start + pg * PAGE_SIZE;
            void *phys = pmm_alloc_page();
            if (!phys) return 0;

            uint8_t *dst = (uint8_t *)phys;
            for (int j = 0; j < PAGE_SIZE; j++) dst[j] = 0;

            for (int j = 0; j < PAGE_SIZE; j++) {
                uint64_t abs_va = page_va + j;
                if (abs_va < vaddr) continue;
                if (abs_va >= vaddr + filesz) continue;
                uint64_t file_off = offset + (abs_va - vaddr);
                if (file_off >= elf_size) continue;
                dst[j] = base[file_off];
            }

            if (vmm_map_page_user(page_va, (uint64_t)phys, PTE_WRITE) != 0) {
                return 0;
            }
        }
    }
    return eh->e_entry + load_bias;
}
