#ifndef MEMMAP_H
#define MEMMAP_H

#include <stdint.h>

struct e820_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_attrs;
} __attribute__((packed));

#define E820_USABLE     1
#define E820_RESERVED   2
#define E820_ACPI_RECLM 3
#define E820_ACPI_NVS   4

#define E820_PHYS_ADDR   0x7000
#define E820_MAX_ENTRIES 32

struct e820_map {
    uint32_t count;
    uint32_t reserved;
    struct e820_entry entries[E820_MAX_ENTRIES];
};

#endif
