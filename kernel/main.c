/* kernel/main.c — flat 64-bit kernel entry */

#include <stdint.h>
#include "idt.h"
#include "gdt.h"
#include "syscall.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "pic.h"
#include "pit.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "thread.h"

static void thread_a(void);
static void thread_b(void);
#include "serial.h"

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

void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_putc(char c)
{
    while (!(inb(COM1 + 5) & 0x20)) { }
    outb(COM1, (uint8_t)c);
}

void serial_print(const char *s)
{
    while (*s) serial_putc(*s++);
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


void serial_hex(uint64_t v)
{
    serial_putc('0');
    serial_putc('x');
    for (int i = 60; i >= 0; i -= 4) {
        int d = (v >> i) & 0xF;
        serial_putc(d < 10 ? '0' + d : 'a' + d - 10);
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


static void thread_a(void)
{
    for (;;) {
        serial_putc('A');
        serial_putc('\n');
        for (volatile int i = 0; i < 2000000; i++) { }
    }
}

static void thread_b(void)
{
    for (;;) {
        serial_putc('B');
        serial_putc('\n');
        for (volatile int i = 0; i < 2000000; i++) { }
    }
}

__attribute__((section(".text._start"), used))
void _start(void)
{
    serial_init();

    kputs("mykernel: booted on custom bootloader\n");
    kputs("mykernel: running in 64-bit long mode\n");
    kputs("mykernel: installing GDT+TSS...\n");
    gdt_init();

    kputs("mykernel: syscall_init...\n");
    syscall_init();

    kputs("mykernel: installing IDT...\n");
    idt_init();
    kputs("mykernel: IDT installed\n");

    pic_remap();
    kputs("mykernel: PIC remapped\n");

    kputs("mykernel: calling pit_init\n");
    pit_init(100);
    kputs("mykernel: pit_init returned\n");

    kputs("mykernel: calling pic_clear_mask\n");
    pic_clear_mask(0);
    kputs("mykernel: pic_clear_mask returned\n");

    kputs("mykernel: enabling interrupts\n");
    __asm__ volatile("sti");
    kputs("mykernel: interrupts enabled, waiting for ticks\n");

    kputs("mykernel: pmm_init...\n");
    pmm_init();

    serial_print("mykernel: total pages = ");
    serial_hex(pmm_total_pages());
    serial_print(", free = ");
    serial_hex(pmm_free_pages());
    serial_print("\n");

    void *p1 = pmm_alloc_page();
    void *p2 = pmm_alloc_page();
    void *p3 = pmm_alloc_page();
    serial_print("alloc: ");
    serial_hex((uint64_t)p1); serial_print(" ");
    serial_hex((uint64_t)p2); serial_print(" ");
    serial_hex((uint64_t)p3); serial_print("\n");

    pmm_free_page(p2);
    void *p4 = pmm_alloc_page();
    serial_print("after free, alloc: ");
    serial_hex((uint64_t)p4); serial_print("\n");

    kputs("mykernel: vmm_init...\n");
    vmm_init();
    kputs("mykernel: vmm_init returned\n");

    void *phys = pmm_alloc_page();
    serial_print("vmm test: phys page = ");
    serial_hex((uint64_t)phys);
    serial_print("\n");

    uint64_t virt = VMM_BASE;
    vmm_map_page(virt, (uint64_t)phys, PTE_WRITE);

    volatile char *vp = (volatile char *)virt;
    vp[0] = 'H';
    vp[1] = 'i';
    vp[2] = '!';
    vp[3] = 0;

    serial_print("vmm test: at 0x40000000 = ");
    serial_print((const char *)virt);
    serial_print("\n");

    vmm_unmap_page(virt);
    serial_print("vmm test: unmapped\n");

    kputs("mykernel: heap_init...\n");
    heap_init();
    kputs("mykernel: heap_init returned\n");

    void *ha = kmalloc(100);
    void *hb = kmalloc(200);
    void *hc = kmalloc(50);

    serial_print("kmalloc: a=");
    serial_hex((uint64_t)ha);
    serial_print(" b=");
    serial_hex((uint64_t)hb);
    serial_print(" c=");
    serial_hex((uint64_t)hc);
    serial_print("\n");

    for (int i = 0; i < 100; i++) ((char *)ha)[i] = (char)i;
    for (int i = 0; i < 200; i++) ((char *)hb)[i] = (char)(i ^ 0xAA);
    for (int i = 0; i < 50;  i++) ((char *)hc)[i] = (char)(i + 1);

    int ok = 1;
    for (int i = 0; i < 100; i++) if (((char *)ha)[i] != (char)i)        ok = 0;
    for (int i = 0; i < 200; i++) if (((char *)hb)[i] != (char)(i^0xAA)) ok = 0;
    for (int i = 0; i < 50;  i++) if (((char *)hc)[i] != (char)(i + 1))  ok = 0;

    serial_print("heap test: patterns ");
    serial_print(ok ? "OK" : "CORRUPT");
    serial_print("\n");

    heap_stats();

    kfree(hb);
    serial_print("after kfree(b):\n");
    heap_stats();

    void *hd = kmalloc(150);
    serial_print("kmalloc(150) after free: ");
    serial_hex((uint64_t)hd);
    serial_print("\n");

    heap_stats();

    /* Spawn two threads and start the scheduler. */

kputs("mykernel: fb_init...\n");
    fb_init();

    /* Quick visual test: fill the screen with colors. */
    fb_clear(FB_BLACK);
    fb_fill_rect(  0,   0, 200, 200, FB_RED);
    fb_fill_rect(200,   0, 200, 200, FB_GREEN);
    fb_fill_rect(400,   0, 200, 200, FB_BLUE);
    fb_fill_rect(  0, 200, 600, 200, FB_YELLOW);
    fb_fill_rect(  0, 400, 600, 100, FB_WHITE);
    kputs("mykernel: fb test drawn\n");
    kputs("mykernel: idle loop, type on the keyboard\n");

    for (;;) {
        __asm__ volatile("hlt");
        char c = keyboard_poll();
        if (c) {
            serial_print("key: ");
            if (c == '\n')      serial_print("<enter>");
            else if (c == '\b') serial_print("<backspace>");
            else if (c == '\t') serial_print("<tab>");
            else if (c == 27)    serial_print("<esc>");
            else                 serial_putc(c);
            serial_print("\n");
        }
    }
/* Should never return. */
    for (;;) __asm__ volatile("hlt");
}
