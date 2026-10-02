#include "idt.h"

#define IDT_ENTRIES 256
#define COM1        0x3F8

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

extern void *isr_stub_table[32];

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static void serial_putc(char c)
{
    while (!(inb(COM1 + 5) & 0x20)) { }
    outb(COM1, (uint8_t)c);
}

static void serial_print(const char *s)
{
    while (*s) serial_putc(*s++);
}

static void serial_hex(uint64_t v)
{
    serial_print("0x");
    for (int i = 60; i >= 0; i -= 4) {
        int d = (v >> i) & 0xF;
        serial_putc(d < 10 ? '0' + d : 'a' + d - 10);
    }
}

static void idt_set_gate(int n, uint64_t handler)
{
    idt[n].offset_low  = handler & 0xFFFF;
    idt[n].selector    = 0x08;          /* kernel code segment */
    idt[n].ist         = 0;             /* no IST for now */
    idt[n].type_attr   = 0x8E;          /* present, ring 0, interrupt gate */
    idt[n].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[n].zero        = 0;
}

void idt_init(void)
{
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;

    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0);
    }

    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, (uint64_t)isr_stub_table[i]);
    }

    __asm__ volatile("lidt %0" :: "m"(idtp));
}

static const char *exception_name(uint64_t v)
{
    static const char *names[] = {
        "divide error", "debug", "NMI", "breakpoint",
        "overflow", "bound range", "invalid opcode", "device not available",
        "double fault", "coprocessor segment", "invalid TSS", "segment not present",
        "stack fault", "general protection", "page fault", "reserved",
        "x87 FP", "alignment check", "machine check", "SIMD FP",
        "virtualization", "control protection", "reserved", "reserved",
        "reserved", "reserved", "reserved", "reserved",
        "hypervisor", "VMM communication", "security", "reserved"
    };
    if (v < 32) return names[v];
    return "unknown";
}

void isr_handler(struct regs *r)
{
    serial_print("\n=== EXCEPTION ===\n");
    serial_print("vector: ");
    serial_hex(r->vector);
    serial_print(" (");
    serial_print(exception_name(r->vector));
    serial_print(")\n");

    serial_print("error:  ");
    serial_hex(r->error_code);
    serial_print("\n");

    serial_print("rip:    ");
    serial_hex(r->rip);
    serial_print("\n");

    serial_print("cs:     ");
    serial_hex(r->cs);
    serial_print("\n");

    serial_print("rflags: ");
    serial_hex(r->rflags);
    serial_print("\n");

    serial_print("rsp:    ");
    serial_hex(r->rsp);
    serial_print("\n");

    serial_print("rax:    ");
    serial_hex(r->rax);
    serial_print("  rbx: ");
    serial_hex(r->rbx);
    serial_print("\n");

    serial_print("rcx:    ");
    serial_hex(r->rcx);
    serial_print("  rdx: ");
    serial_hex(r->rdx);
    serial_print("\n");

    if (r->vector == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        serial_print("cr2:    ");
        serial_hex(cr2);
        serial_print("\n");
    }

    serial_print("=== HALT ===\n");
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}
