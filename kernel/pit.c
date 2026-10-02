#include "pit.h"

static volatile uint64_t ticks = 0;

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

void pit_init(uint32_t freq)
{
    uint32_t divisor = PIT_BASE_FREQ / freq;
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

void pit_tick(void)
{
    ticks++;
}

uint64_t pit_ticks(void)
{
    return ticks;
}
