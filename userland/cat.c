/* userspace cat: opens a file and prints it via write syscall */
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

static long syscall1(long n, long a)
{
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static unsigned long strlen_(const char *s)
{
    unsigned long n = 0;
    while (s[n]) n++;
    return n;
}

void _start(void)
{
    const char *path = "/hello.txt";
    long fd = syscall3(2, (long)path, 0, 0);   /* open */
    if (fd < 0) {
        const char *err = "cat: open failed\n";
        syscall3(1, 1, (long)err, strlen_(err));
        syscall1(60, 0);
    }

    char buf[128];
    long n;
    while ((n = syscall3(0, fd, (long)buf, sizeof(buf))) > 0) {
        syscall3(1, 1, (long)buf, n);
    }

    syscall1(3, fd);   /* close */
    syscall1(60, 0);   /* exit */
    for (;;) {}
}
