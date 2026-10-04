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


/* Linux x86_64 struct stat. Matches glibc/mlibc layout. */

/* POSIX time types. */
struct mykernel_timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct mykernel_timeval {
    int64_t tv_sec;
    int64_t tv_usec;
};

#define CLOCK_REALTIME   0
#define CLOCK_MONOTONIC  1

struct mykernel_stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t __pad0;
    uint64_t st_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;
    uint64_t st_atime;
    uint64_t st_atime_nsec;
    uint64_t st_mtime;
    uint64_t st_mtime_nsec;
    uint64_t st_ctime;
    uint64_t st_ctime_nsec;
    int64_t  __unused[3];
};

void syscall_init(void);

#define SYS_nanosleep   35
#define SYS_gettimeofday 96
#define SYS_clock_gettime 228
#define SYS_clock_getres  229
#define SYS_stat        4
#define SYS_fstat       5
#define SYS_lstat       6
#define SYS_fstat       5
#define SYS_mmap        9
#define SYS_mprotect    10
#define SYS_munmap      11
#define SYS_brk         12
#define SYS_ioctl       16
#define SYS_getpid      39
#define SYS_newfstatat  262
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
