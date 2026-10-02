/* Freestanding user program: no libc, just syscalls. */
typedef unsigned long u64;

static long syscall3(long n, long a, long b, long c)
{
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a), "S"(b), "d"(c)
        : "rcx", "r11", "memory"
    );
    return ret;
}

void _start(void)
{
    const char msg[] = "hello from ELF\n";
    syscall3(1, 1, (long)msg, sizeof(msg) - 1);
    syscall3(60, 0, 0, 0);
    for (;;) {}
}
