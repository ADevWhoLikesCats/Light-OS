#include "gdt.h"
#include "serial.h"

#define GDT_ENTRIES 7

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct tss {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gdtp;
static struct tss       tss;

/* Kernel stack for user-mode entry. 16 KiB, 16-byte aligned. */
static uint8_t kernel_stack[16384] __attribute__((aligned(16)));

static void gdt_set(int i, uint32_t base, uint32_t limit,
                    uint8_t access, uint8_t gran)
{
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].base_high   = (base >> 24) & 0xFF;
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access      = access;
}

/* TSS descriptor occupies two GDT slots (system descriptor, 16 bytes). */
static void gdt_set_tss(int i, uint64_t base, uint32_t limit)
{
    /* Slot i: low half */
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].access      = 0x89;                /* present, type=9 (64-bit TSS) */
    gdt[i].granularity = ((limit >> 16) & 0x0F);
    gdt[i].base_high   = (base >> 24) & 0xFF;

    /* Slot i+1: high half — base bits 32..63 in low 4 bytes, rest zero */
    uint32_t base_hi = (uint32_t)(base >> 32);
    gdt[i+1].limit_low   = (uint16_t)(base_hi & 0xFFFF);
    gdt[i+1].base_low    = (uint16_t)((base_hi >> 16) & 0xFFFF);
    gdt[i+1].base_mid    = 0;
    gdt[i+1].access      = 0;
    gdt[i+1].granularity = 0;
    gdt[i+1].base_high   = 0;
}

extern void gdt_load(struct gdt_ptr *p);

void gdt_init(void)
{
    gdt_set(0, 0, 0, 0, 0);

    /* Kernel code: base=0, limit=0xFFFFF, access=0x9A, gran=0xA0 (long mode) */
    gdt_set(1, 0, 0xFFFFF, 0x9A, 0xA0);

    /* Kernel data: access=0x92, gran=0xC0 */
    gdt_set(2, 0, 0xFFFFF, 0x92, 0xC0);

    /* User code: access=0xFA (ring 3), gran=0xA0 */
    gdt_set(3, 0, 0xFFFFF, 0xFA, 0xA0);

    /* User data: access=0xF2 (ring 3), gran=0xC0 */
    gdt_set(4, 0, 0xFFFFF, 0xF2, 0xC0);

    /* TSS at selector 0x28 (index 5, occupying slots 5 and 6) */
    uint64_t tss_base = (uint64_t)&tss;
    gdt_set_tss(5, tss_base, sizeof(tss) - 1);

    /* Zero the TSS */
    uint8_t *p = (uint8_t *)&tss;
    for (unsigned i = 0; i < sizeof(tss); i++) p[i] = 0;

    /* RSP0 = top of kernel stack for ring-0 entry */
    tss.rsp0 = (uint64_t)(kernel_stack + sizeof(kernel_stack));
    tss.iomap_base = sizeof(tss);

    /* Load the new GDT */
    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base  = (uint64_t)&gdt;
    gdt_load(&gdtp);

    /* Load the TSS into TR */
    __asm__ volatile("ltr %0" :: "r"((uint16_t)GDT_TSS));

    serial_print("gdt: loaded GDT+TSS\n");
}

void tss_set_kernel_stack(uint64_t rsp0)
{
    tss.rsp0 = rsp0;
}
