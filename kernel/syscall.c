#include "syscall.h"
#include "serial.h"
#include "keyboard.h"
#include "console.h"
#include "vfs.h"
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

    serial_print("syscall: MSRs configured\n");
}

uint64_t syscall_handler(struct syscall_regs *r)
{
    uint64_t num = r->rax;

    switch (num) {
        case SYS_read: {
            serial_print("sys: read fd=");
            serial_hex(r->rdi);
            serial_print(" buf=");
            serial_hex(r->rsi);
            serial_print(" count=");
            serial_hex(r->rdx);
            serial_print("\n");
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
            serial_print("sys: read returned ");
            serial_hex(n);
            serial_print("\n");
            return (n < 0) ? (uint64_t)-1 : (uint64_t)n;
        }
        case SYS_write: {
            serial_print("sys: write fd=");
            serial_hex(r->rdi);
            serial_print(" buf=");
            serial_hex(r->rsi);
            serial_print(" count=");
            serial_hex(r->rdx);
            serial_print("\n");
            int fd = (int)r->rdi;
            const char *buf = (const char *)r->rsi;
            uint64_t count = r->rdx;

            serial_print("sys: write first 8 bytes: ");
            for (int i = 0; i < 8 && i < (int)count; i++) {
                serial_hex((unsigned char)buf[i]);
                serial_print(" ");
            }
            serial_print("\n");

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
            serial_print("sys: open path=");
            serial_print((const char *)r->rdi);
            serial_print("\n");
            const char *path = (const char *)r->rdi;
            int fd = vfs_open(path);
            return (fd < 0) ? (uint64_t)-1 : (uint64_t)fd;
        }
        case SYS_close: {
            serial_print("sys: close fd=");
            serial_hex(r->rdi);
            serial_print("\n");
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
            (void)fd; (void)buf; (void)count;
            return (uint64_t)-1;   /* TODO: implement */
        }
        case SYS_exit:
            serial_print("sys: exit\n");
            return 0;

        default:
            serial_print("syscall: unknown ");
            serial_hex(num);
            serial_print("\n");
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

    serial_print("enter_userspace: loading embedded ELF (");
    serial_hex(hello_elf_len);
    serial_print(" bytes)\n");

    uint64_t entry = elf_load(hello_elf, hello_elf_len, USER_LOAD_BIAS);
    if (!entry) {
        serial_print("enter_userspace: elf_load failed\n");
        for (;;) __asm__ volatile("hlt");
    }

    /* Map a user stack. */
    for (uint64_t i = 0; i < USER_STACK_PAGES; i++) {
        void *phys = pmm_alloc_page();
        vmm_map_page_user(USER_STACK_VIRT + i * PAGE_SIZE, (uint64_t)phys, PTE_WRITE);
    }
    uint64_t user_stack_top = USER_STACK_VIRT + USER_STACK_PAGES * PAGE_SIZE;

    serial_print("enter_userspace: jumping to entry ");
    serial_hex(entry);
    serial_print("\n");

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
        serial_print("enter_userspace_elf: elf_load failed\n");
        return;
    }

    for (uint64_t i = 0; i < USER_STACK_PAGES; i++) {
        void *phys = pmm_alloc_page();
        vmm_map_page_user(USER_STACK_VIRT + i * PAGE_SIZE, (uint64_t)phys, PTE_WRITE);
    }
    uint64_t user_stack_top = USER_STACK_VIRT + USER_STACK_PAGES * PAGE_SIZE;

    {
        const volatile uint8_t *p = (const volatile uint8_t *)0x40001000;
        serial_print("elf: bytes at 0x40001000: ");
        for (int k = 0; k < 32; k++) { serial_hex(p[k]); serial_print(" "); }
        serial_print("\n");
    }

    serial_print("enter_userspace_elf: jumping to entry ");
    serial_hex(entry);
    serial_print("\n");

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
