/* kernel/main.c — flat 64-bit kernel entry */

#include <stdint.h>
#include "idt.h"

#define VGA_BASE   0xB8000ULL
#define VGA_COLS   80
#define VGA_ROWS   25
#define VGA_ATTR   0x0F          /* white on black */

#define COM1       0x3F8

static volatile uint16_t *const vga = (uint16_t *)VGA_BASE;
static unsigned vga_row = 0;
static unsigned vga_col = 0;

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

static void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static void serial_putc(char c)
{
    while (!(inb(COM1 + 5) & 0x20)) { }
    outb(COM1, (uint8_t)c);
}

static void vga_putc(char c)
{
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else {
        vga[vga_row * VGA_COLS + vga_col] =
            (uint16_t)((VGA_ATTR << 8) | (uint8_t)c);
        vga_col++;
        if (vga_col >= VGA_COLS) {
            vga_col = 0;
            vga_row++;
        }
    }
    if (vga_row >= VGA_ROWS) {
        vga_row = 0;   /* simple wraparound, no scroll */
    }
}

static void kputs(const char *s)
{
    while (*s) {
        vga_putc(*s);
        serial_putc(*s);
        s++;
    }
}

__attribute__((section(".text._start"), used))
void _start(void)
{
    serial_init();

    kputs("mykernel: booted on custom bootloader\n");
    kputs("mykernel: running in 64-bit long mode\n");
    kputs("mykernel: installing IDT...\n");

    idt_init();

    kputs("mykernel: IDT installed, triggering int3\n");

    __asm__ volatile("int $3");

    kputs("mykernel: this line should NOT print\n");

    for (;;) {
        __asm__ volatile("hlt");
    }
}
