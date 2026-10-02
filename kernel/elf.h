#ifndef ELF_H
#define ELF_H

#include <stdint.h>

/* Return: entry point virtual address on success, 0 on failure. */
uint64_t elf_load(const void *elf_data, uint64_t elf_size, uint64_t load_bias);

#endif
