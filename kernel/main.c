/* kernel/main.c — flat 64-bit kernel entry */

#include <stdint.h>
#include "idt.h"
#include "gdt.h"
#include "syscall.h"
#include "pic.h"
#include "pit.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "thread.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "console.h"
#include "vfs.h"
#include "shell.h"
#include "serial.h"

#define COM1 0x3F8

/* ------------------------------------------------------------ */
/* Serial helpers                                                */
/* ------------------------------------------------------------ */

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

void serial_hex(uint64_t v)
{
    serial_putc('0');
    serial_putc('x');
    for (int i = 60; i >= 0; i -= 4) {
        int d = (v >> i) & 0xF;
        serial_putc(d < 10 ? '0' + d : 'a' + d - 10);
    }
}

/* ------------------------------------------------------------ */
/* kputs — writes to both console (framebuffer) and serial       */
/* ------------------------------------------------------------ */

void kputs(const char *s)
{
    while (*s) {
        console_putchar(*s);
        serial_putc(*s);
        s++;
    }
}

/* ------------------------------------------------------------ */
/* Entry                                                         */
/* ------------------------------------------------------------ */

__attribute__((section(".text._start"), used))
void _start(void)
{
    serial_init();
    {
        volatile unsigned char *p = (volatile unsigned char *)0x108640;
        extern char __kernel_end[];
        serial_print("boot: [0x108640]=");
        serial_hex(p[0]);
        serial_print(" [0x108641]=");
        serial_hex(p[1]);
        serial_print(" __kernel_end=");
        serial_hex((uint64_t)__kernel_end);
        serial_print("\n");
    }
    serial_print("mykernel: entry\n");

    serial_print("mykernel: installing GDT+TSS...\n");
    gdt_init();

    serial_print("mykernel: syscall_init...\n");
    syscall_init();

    serial_print("mykernel: installing IDT...\n");
    idt_init();

    serial_print("mykernel: PIC remap...\n");
    pic_remap();

    pit_init(100);
    pic_clear_mask(0);

    keyboard_init();
    pic_clear_mask(1);

    __asm__ volatile("sti");

    serial_print("mykernel: pmm_init...\n");
    pmm_init();

    serial_print("mykernel: vmm_init...\n");
    vmm_init();

    serial_print("mykernel: heap_init...\n");
    heap_init();

    serial_print("mykernel: fb_init...\n");
    fb_init();

    serial_print("mykernel: console_init...\n");
    console_init();

    serial_print("mykernel: vfs_init...\n");
    vfs_init();

    kputs("mykernel: console ready\n");

    kputs("mykernel: idle loop, type on the keyboard\n");

    shell_loop();

    /* Shell never returns. */
    for (;;) __asm__ volatile("hlt");
}
