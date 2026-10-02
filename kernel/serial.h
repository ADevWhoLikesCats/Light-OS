#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

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

void serial_init(void);
void serial_putc(char c);
void serial_print(const char *s);
void serial_hex(uint64_t v);

#endif
