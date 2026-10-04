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
#include "mm.h"
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
        extern char initramfs_root[];
        serial_print("EARLY CHECK initramfs_root @ ");
        serial_hex((uint64_t)initramfs_root);
        serial_print(" type=");
        serial_hex(*(unsigned int *)(initramfs_root + 64));
        serial_print(" children=");
        serial_hex(*(uint64_t *)(initramfs_root + 96));
        serial_print("\n");
    }


    gdt_init();
    syscall_init();
    idt_init();
    pic_remap();

    pit_init(100);
    pic_clear_mask(0);

    keyboard_init();
    pic_clear_mask(1);

    __asm__ volatile("sti");
    pmm_init();
    vmm_init();
    heap_init();
    fb_init();
    console_init();
    vfs_init();



    shell_loop();

    /* Shell never returns. */
    for (;;) __asm__ volatile("hlt");
}
