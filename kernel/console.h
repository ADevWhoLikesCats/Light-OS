#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

void console_init(void);
void console_clear(void);
void console_putchar(char c);
void console_puts(const char *s);
void console_puts_at(uint32_t col, uint32_t row, const char *s);

#endif
