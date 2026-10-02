#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* Linux-compatible syscall numbers (x86_64). */
#define SYS_read    0
#define SYS_write   1
#define SYS_exit    60

void syscall_init(void);
void enter_userspace(void);

/* Register frame passed from syscall_entry.asm to the C handler. */
struct syscall_regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t rip, rflags, rsp, ss;
};

#endif
