#include "syscall.h"
#include "serial.h"
#include "keyboard.h"
#include "console.h"
#include "vfs.h"
#include "mm.h"
#include "pit.h"

static char kernel_cwd[256];
static void strcpy_(char *dst, const char *s, int max) {
    int i = 0;
    while (s[i] && i < max - 1) { dst[i] = s[i]; i++; }
    dst[i] = 0;
}


extern void exit_ctx_save(uint64_t *ctx);
extern void exit_ctx_restore(uint64_t *ctx);

static uint64_t exit_ctx[8];
#include "vmm.h"
#include "pmm.h"
#include "heap.h"

#define MSR_EFER    0xC0000080
#define MSR_STAR    0xC0000081
#define MSR_LSTAR   0xC0000082
#define MSR_FMASK   0xC0000084

#define KERNEL_CS   0x08
#define KERNEL_DS   0x10
#define USER_CS     0x20
#define USER_DS     0x18

extern void syscall_entry(void);

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" :: "c"(msr), "a"(lo), "d"(hi));
}

void syscall_init(void)
{
    kernel_cwd[0] = '/';
    kernel_cwd[1] = 0;

    /* Enable SCE (System Call Extension) in EFER. Without this,
       the SYSCALL instruction raises #UD. */
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= (1ULL << 0);
    wrmsr(MSR_EFER, efer);

    /* STAR: kernel CS in bits 47-32, user CS base in bits 63-48. */
    /* STAR[63:48] = USER_CS - 16 = 0x10 (SYSRET does +16 for CS, +8 for SS)
       With STAR=0x10: SYSRET CS = 0x10+16 = 0x20 (USER_CS), SS = 0x10+8 = 0x18 (USER_DS) */
    uint64_t star = ((uint64_t)0x10 << 48) | ((uint64_t)KERNEL_CS << 32);
    wrmsr(MSR_STAR, star);

    /* LSTAR: entry point for SYSCALL. */
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* FMASK: clear IF on entry so interrupts are disabled. */
    wrmsr(MSR_FMASK, 0x200);

}


/* Fill a struct mykernel_stat from a VFS node.
   Returns 0 on success, negative errno on failure. */
static int fill_stat(const struct vfs_node *n, struct mykernel_stat *st)
{
    if (!n || !st) return -14;   /* EFAULT */

    for (int i = 0; i < (int)sizeof(*st); i++) ((char *)st)[i] = 0;

    st->st_dev      = 1;
    st->st_ino      = (uint64_t)n;
    st->st_nlink    = (n->type == VFS_DIR) ? 2 : 1;
    st->st_mode     = n->mode;
    st->st_uid      = n->uid;
    st->st_gid      = n->gid;
    st->st_size     = n->size;
    st->st_blksize  = 4096;
    st->st_blocks   = (n->size + 511) / 512;
    st->st_atime    = 0;
    st->st_mtime    = 0;
    st->st_ctime    = 0;
    return 0;
}


uint64_t syscall_handler(struct syscall_regs *r)
{
    uint64_t num = r->rax;

    switch (num) {
        case SYS_read: {
            int fd = (int)r->rdi;
            char *buf = (char *)r->rsi;
            uint64_t count = r->rdx;

            extern struct vfs_node *vfs_fd_node(int fd);
            if (fd == 0 && vfs_fd_node(0) == 0) {
                /* stdin: read one char at a time from keyboard */
                uint64_t got = 0;
                while (got < count) {
                    char c = keyboard_getchar_blocking();
                    if (c == '\n') {
                        buf[got++] = '\n';
                        console_putchar('\n');
                        break;
                    }
                    buf[got++] = c;
                    console_putchar(c);
                }
                return got;
            }
            int n = vfs_read(fd, buf, count);
            return (n < 0) ? (uint64_t)-1 : (uint64_t)n;
        }
        case SYS_write: {
            int fd = (int)r->rdi;
            const char *buf = (const char *)r->rsi;
            uint64_t count = r->rdx;

            if (fd == 1 || fd == 2) {
                for (uint64_t i = 0; i < count; i++) {
                    console_putchar(buf[i]);
                    serial_putc(buf[i]);
                }
                return count;
            }
            return (uint64_t)-1;
        }
        case SYS_open: {
            const char *path = (const char *)r->rdi;
            int fd = vfs_open(path);
            return (fd < 0) ? (uint64_t)-1 : (uint64_t)fd;
        }
        case SYS_close: {
            int fd = (int)r->rdi;
            int rv = vfs_close(fd);
            return (rv < 0) ? (uint64_t)-1 : 0;
        }
        case SYS_lseek: {
            int fd = (int)r->rdi;
            uint64_t off = (uint64_t)r->rsi;
            int whence = (int)r->rdx;
            int rv = vfs_lseek(fd, off, whence);
            return (rv < 0) ? (uint64_t)-1 : (uint64_t)rv;
        }
        case SYS_getdents64: {
            int fd = (int)r->rdi;
            void *buf = (void *)r->rsi;
            uint64_t count = r->rdx;
            int n = vfs_getdents64(fd, buf, count);
            return (n < 0) ? (uint64_t)-1 : (uint64_t)n;
        }
        case SYS_brk: {
            uint64_t new_brk = mm_brk(r->rdi);
            return new_brk;
        }
        case SYS_mmap: {
            uint64_t addr   = r->rdi;
            uint64_t length = r->rsi;
            uint64_t prot   = r->rdx;
            uint64_t flags  = r->r10;
            int      fd     = (int)r->r8;
            uint64_t off    = r->r9;
            uint64_t v = mm_mmap(addr, length, prot, flags, fd, off);
            return v;
        }
        case SYS_munmap: {
            int rv = mm_munmap(r->rdi, r->rsi);
            return (rv < 0) ? (uint64_t)-1 : 0;
        }
        case SYS_mprotect: {
            /* accept and ignore for now */
            return 0;
        }
        case SYS_getpid:  return 1;
        case SYS_getppid: return 0;
        case SYS_gettid:  return 1;
        case SYS_getuid:  return 0;
        case SYS_getgid:  return 0;
        case SYS_geteuid: return 0;
        case SYS_getegid: return 0;
        case SYS_arch_prctl: {
            /* arch_prctl(ARCH_SET_FS=0x1002, addr) or ARCH_GET_FS=0x1003 */
            uint64_t code = r->rdi;
            uint64_t addr = r->rsi;
            if (code == 0x1002) {
                /* set FS base */
                __asm__ volatile("wrmsr" :: "c"(0xC0000100),
                                 "a"((uint32_t)addr),
                                 "d"((uint32_t)(addr >> 32)));
                return 0;
            } else if (code == 0x1003) {
                /* get FS base */
                uint32_t lo, hi;
                __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000100));
                uint64_t fsbase = ((uint64_t)hi << 32) | lo;
                *(uint64_t *)addr = fsbase;
                return 0;
            }
            return (uint64_t)-22;   /* -EINVAL */
        }
        case SYS_set_tid_address: return 1;
        case SYS_set_robust_list: return 0;
        case SYS_rseq:            return 0;
        case SYS_futex:           return 0;   /* stub — no threads yet */
        case SYS_exit_group:
            exit_ctx_restore(exit_ctx);
            /* unreachable */
            for (;;) __asm__ volatile("hlt");
        case SYS_getrandom: {
            /* Fill the buffer with pseudo-random bytes derived from a counter. */
            static uint64_t seed = 0x123456789ABCDEF0ULL;
            uint8_t *buf = (uint8_t *)r->rdi;
            uint64_t n   = r->rsi;
            for (uint64_t i = 0; i < n; i++) {
                seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
                buf[i] = (uint8_t)(seed >> 33);
            }
            return n;
        }
        case SYS_prlimit64: {
            /* prlimit64(pid, resource, new, old) */
            uint64_t oldp = r->r10;
            if (oldp) {
                /* zero the rlimit struct — 16 bytes */
                uint64_t *p = (uint64_t *)oldp;
                p[0] = 0xFFFFFFFFFFFFFFFFULL;
                p[1] = 0xFFFFFFFFFFFFFFFFULL;
            }
            return 0;
        }
        case SYS_stat:
        case SYS_lstat: {
            const char *path = (const char *)r->rdi;
            struct mykernel_stat *st = (struct mykernel_stat *)r->rsi;
            struct vfs_node *n = vfs_lookup(path);
            if (!n) return (uint64_t)-2;   /* ENOENT */
            int rv = fill_stat(n, st);
            return (rv < 0) ? (uint64_t)rv : 0;
        }
        case SYS_fstat: {
            int fd = (int)r->rdi;
            struct mykernel_stat *st = (struct mykernel_stat *)r->rsi;
            struct vfs_node *n = vfs_fd_node(fd);
            if (!n) return (uint64_t)-9;   /* EBADF */
            int rv = fill_stat(n, st);
            return (rv < 0) ? (uint64_t)rv : 0;
        }
        case SYS_newfstatat: {
            const char *path = (const char *)r->rsi;
            struct mykernel_stat *st = (struct mykernel_stat *)r->rdx;
            struct vfs_node *n = vfs_lookup(path);
            if (!n) return (uint64_t)-2;
            int rv = fill_stat(n, st);
            return (rv < 0) ? (uint64_t)rv : 0;
        }
        case SYS_clock_gettime: {
            int clockid = (int)r->rdi;
            struct mykernel_timespec *tp = (struct mykernel_timespec *)r->rsi;
            if (clockid != CLOCK_REALTIME && clockid != CLOCK_MONOTONIC)
                return (uint64_t)-22;   /* -EINVAL */
            uint64_t t = pit_ticks();
            tp->tv_sec  = t / 100;
            tp->tv_nsec = (t % 100) * 10000000ULL;
            return 0;
        }
        case SYS_clock_getres: {
            int clockid = (int)r->rdi;
            struct mykernel_timespec *tp = (struct mykernel_timespec *)r->rsi;
            if (clockid != CLOCK_REALTIME && clockid != CLOCK_MONOTONIC)
                return (uint64_t)-22;
            if (tp) {
                tp->tv_sec  = 0;
                tp->tv_nsec = 10000000ULL;   /* 10 ms */
            }
            return 0;
        }
        case SYS_nanosleep: {
            const struct mykernel_timespec *req = (const struct mykernel_timespec *)r->rdi;
            uint64_t ticks = (uint64_t)req->tv_sec * 100
                           + (uint64_t)req->tv_nsec / 10000000ULL;
            uint64_t target = pit_ticks() + ticks;
            /* Interrupts are disabled at syscall entry — enable them so
               the PIT can tick, then disable again before returning. */
            __asm__ volatile("sti");
            while (pit_ticks() < target) {
                __asm__ volatile("hlt");
            }
            __asm__ volatile("cli");
            return 0;
        }
        case SYS_gettimeofday: {
            struct mykernel_timeval *tv = (struct mykernel_timeval *)r->rdi;
            uint64_t t = pit_ticks();
            if (tv) {
                tv->tv_sec  = t / 100;
                tv->tv_usec = (t % 100) * 10000ULL;
            }
            return 0;
        }
        case SYS_rt_sigaction: return 0;
        case SYS_rt_sigprocmask: return 0;
        case SYS_rt_sigreturn:  return 0;

        case SYS_uname: {
            struct mykernel_utsname *u = (struct mykernel_utsname *)r->rdi;
            if (!u) return (uint64_t)-14;
            const char *sysname  = "mykernel";
            const char *nodename = "localhost";
            const char *release  = "0.1";
            const char *version  = "#1 SMP";
            const char *machine  = "x86_64";
            const char *domain   = "(none)";
            strcpy_(u->sysname,   sysname,  65);
            strcpy_(u->nodename,  nodename, 65);
            strcpy_(u->release,   release,  65);
            strcpy_(u->version,   version,  65);
            strcpy_(u->machine,   machine,  65);
            strcpy_(u->domainname, domain,  65);
            return 0;
        }

        case SYS_getcwd: {
            char *buf = (char *)r->rdi;
            uint64_t size = r->rsi;
            uint64_t len = 0;
            while (kernel_cwd[len]) len++;
            if (size < len + 1) return (uint64_t)-34;   /* ERANGE */
            for (uint64_t i = 0; i < len; i++) buf[i] = kernel_cwd[i];
            buf[len] = 0;
            return len + 1;
        }

        case SYS_chdir: {
            const char *path = (const char *)r->rdi;
            struct vfs_node *n = vfs_lookup(path);
            if (!n) return (uint64_t)-2;        /* ENOENT */
            if (n->type != VFS_DIR) return (uint64_t)-20; /* ENOTDIR */
            strcpy_(kernel_cwd, path, 256);
            return 0;
        }

        case SYS_access: {
            const char *path = (const char *)r->rdi;
            struct vfs_node *n = vfs_lookup(path);
            return n ? 0 : (uint64_t)-2;
        }

        case SYS_readlink: {
            /* We have no symlinks; return -EINVAL. */
            return (uint64_t)-22;
        }

        case SYS_ioctl: {
            /* TODO: termios, winsize. For now, not-a-tty. */
            return (uint64_t)-25;   /* -ENOTTY */
        }

        case SYS_pipe: {
            /* TODO: real pipe. Return ENOSYS for now. */
            return (uint64_t)-38;
        }

        case SYS_dup: {
            /* Stub — return a new fd that aliases the same file. */
            int fd = (int)r->rdi;
            struct vfs_node *n = vfs_fd_node(fd);
            if (!n) return (uint64_t)-9;    /* EBADF */
            /* Find a free fd >= 3 and "open" the same file at offset 0. */
            /* Simplified: we don't actually track per-fd offsets separately. */
            return (uint64_t)-38;   /* ENOSYS for now */
        }

        case SYS_dup2: {
            return (uint64_t)-38;
        }

        case SYS_fcntl: {
            /* F_GETFD=1, F_SETFD=2, F_GETFL=3, F_SETFL=4, F_DUPFD=0. */
            int cmd = (int)r->rsi;
            switch (cmd) {
                case 1: return 0;               /* F_GETFD */
                case 3: return 0;               /* F_GETFL — O_RDONLY */
                case 2: case 4: return 0;       /* F_SETFD/F_SETFL — nop */
                default: return (uint64_t)-22;  /* EINVAL */
            }
        }

        case SYS_exit:
            exit_ctx_restore(exit_ctx);
            /* unreachable */
            for (;;) __asm__ volatile("hlt");

        default:
            return (uint64_t)-1;
    }
}


/* Symbols from userspace.asm */
extern uint8_t user_blob_start[];
extern uint8_t user_blob_end[];




#include "elf.h"
#include "hello_elf.h"

extern uint64_t elf_load(const void *, uint64_t, uint64_t);

void enter_userspace(void)
{
    const uint64_t USER_LOAD_BIAS  = 0x0ULL;
    const uint64_t USER_STACK_VIRT = 0x7F000000ULL;
    const uint64_t USER_STACK_PAGES = 4;


    uint64_t entry = elf_load(hello_elf, hello_elf_len, USER_LOAD_BIAS);
    if (!entry) {
        for (;;) __asm__ volatile("hlt");
    }

    /* Map a user stack. */
    for (uint64_t i = 0; i < USER_STACK_PAGES; i++) {
        void *phys = pmm_alloc_page();
        vmm_map_page_user(USER_STACK_VIRT + i * PAGE_SIZE, (uint64_t)phys, PTE_WRITE);
    }
    uint64_t user_stack_top = USER_STACK_VIRT + USER_STACK_PAGES * PAGE_SIZE;


    __asm__ volatile(
        "pushq $0x1B\n"          /* SS = USER_DS | 3 */
        "pushq %1\n"
        "pushq $0x202\n"
        "pushq $0x23\n"          /* CS = USER_CS | 3 */
        "pushq %0\n"
        "iretq\n"
        :
        : "r"(entry), "r"(user_stack_top)
        : "memory"
    );

    for (;;) __asm__ volatile("hlt");
}


#include "elf.h"

void enter_userspace_elf(const void *elf, uint64_t len)
{
    const uint64_t USER_LOAD_BIAS  = 0x0ULL;
    const uint64_t USER_STACK_VIRT = 0x7F000000ULL;
    const uint64_t USER_STACK_PAGES = 4;

    uint64_t entry = elf_load(elf, len, USER_LOAD_BIAS);
    if (!entry) {
        return;
    }

    for (uint64_t i = 0; i < USER_STACK_PAGES; i++) {
        void *phys = pmm_alloc_page();
        vmm_map_page_user(USER_STACK_VIRT + i * PAGE_SIZE, (uint64_t)phys, PTE_WRITE);
    }
    uint64_t user_stack_top = USER_STACK_VIRT + USER_STACK_PAGES * PAGE_SIZE;

    {
        const volatile uint8_t *p = (const volatile uint8_t *)0x40001000;
        for (int k = 0; k < 32; k++) { serial_hex(p[k]); serial_print(" "); }
    }


    __asm__ volatile(
        "pushq $0x1B\n"          /* SS = USER_DS | 3 */
        "pushq %1\n"
        "pushq $0x202\n"
        "pushq $0x23\n"          /* CS = USER_CS | 3 */
        "pushq %0\n"
        "iretq\n"
        :
        : "r"(entry), "r"(user_stack_top)
        : "memory"
    );

    for (;;) __asm__ volatile("hlt");
}
