#ifndef MM_H
#define MM_H

#include <stdint.h>

/* mmap region: starts high, grows down. Stack is at 0x7F000000.
   mmap starts at 0x60000000 and bumps up. */
#define MMAP_BASE   0x60000000ULL
#define MMAP_LIMIT  0x70000000ULL

#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20

void     mm_init(void);
uint64_t mm_brk(uint64_t new_brk);
uint64_t mm_mmap(uint64_t addr, uint64_t length, uint64_t prot,
                 uint64_t flags, int fd, uint64_t offset);
int      mm_munmap(uint64_t addr, uint64_t length);

#endif
