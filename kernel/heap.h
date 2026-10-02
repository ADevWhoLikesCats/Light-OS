#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

#define HEAP_BASE   0x40000000ULL
#define HEAP_SIZE   (4ULL * 1024 * 1024)

void  heap_init(void);
void *kmalloc(size_t n);
void  kfree(void *p);
void  heap_stats(void);

#endif
