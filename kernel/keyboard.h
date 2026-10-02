#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

/* Ring buffer size for pending keys (power of two). */
#define KEYBOARD_BUFFER_SIZE 64

void     keyboard_init(void);
int      keyboard_has_key(void);
char     keyboard_getchar(void);      /* blocking; returns 0 if none (non-blocking poll) */
char     keyboard_poll(void);         /* returns 0 if no key waiting */

/* IRQ1 handler entry — called from irq_handler. */
void     keyboard_irq(void);

#endif
