#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* Register state pushed by the ISR stubs, in the order they appear
   on the stack (lowest address first = last pushed). */
struct regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector, error_code;
    /* pushed by CPU on interrupt entry: */
    uint64_t rip, cs, rflags, rsp, ss;
};

void idt_init(void);
void isr_handler(struct regs *r);

#endif
