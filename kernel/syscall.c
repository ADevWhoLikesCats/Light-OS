#include "syscall.h"
#include "serial.h"
#include "vmm.h"
#include "pmm.h"
#include "heap.h"

#define MSR_EFER    0xC0000080
#define MSR_STAR    0xC0000081
#define MSR_LSTAR   0xC0000082
#define MSR_FMASK   0xC0000084

#define KERNEL_CS   0x08
#define USER_CS     0x18

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
    uint64_t star = ((uint64_t)USER_CS << 48) | ((uint64_t)KERNEL_CS << 32);
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
    uint64_t ret = 0;

    switch (num) {
        case SYS_write: {
            int fd          = (int)r->rdi;
            const char *buf = (const char *)r->rsi;
            uint64_t count  = r->rdx;

            if (fd == 1 || fd == 2) {
                for (uint64_t i = 0; i < count; i++) {
                    serial_putc(buf[i]);
                }
                ret = count;
            }
            break;
        }
        case SYS_exit: {
            serial_print("syscall: exit called\n");
            for (;;) __asm__ volatile("hlt");
            break;
        }
        default:
            serial_print("syscall: unknown ");
            serial_hex(num);
            serial_print("\n");
            ret = (uint64_t)-1;
            break;
    }

    return ret;
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
        "pushq $0x23\n"
        "pushq %1\n"
        "pushq $0x202\n"
        "pushq $0x1B\n"
        "pushq %0\n"
        "iretq\n"
        :
        : "r"(entry), "r"(user_stack_top)
        : "memory"
    );

    for (;;) __asm__ volatile("hlt");
}
