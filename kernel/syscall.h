#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* Linux-compatible syscall numbers (x86_64). */
#define SYS_read    0
#define SYS_write   1
#define SYS_open    2
#define SYS_close   3
#define SYS_lseek   8
#define SYS_getdents64 217
#define SYS_exit    60

void syscall_init(void);

#define SYS_stat        4
#define SYS_fstat       5
#define SYS_mmap        9
#define SYS_mprotect    10
#define SYS_munmap      11
#define SYS_brk         12
#define SYS_ioctl       16
#define SYS_getpid      39
#define SYS_getcwd      79
#define SYS_getuid      102
#define SYS_getgid      104
#define SYS_geteuid     107
#define SYS_getegid     108
#define SYS_getppid     110
#define SYS_arch_prctl  158
#define SYS_gettid      186
#define SYS_futex       202
#define SYS_set_tid_address   218
#define SYS_exit_group  231
#define SYS_set_robust_list   273
#define SYS_prlimit64   302
#define SYS_getrandom   318
#define SYS_rseq        334

void enter_userspace(void);

/* Register frame passed from syscall_entry.asm to the C handler. */
struct syscall_regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t rip, rflags, rsp, ss;
};

#endif
