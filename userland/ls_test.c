typedef unsigned long u64;

static long syscall3(long n, long a, long b, long c)
{
    long ret;
    __asm__ volatile("syscall"
        : "=a"(ret)
        : "a"(n), "D"(a), "S"(b), "d"(c)
        : "rcx", "r11", "memory");
    return ret;
}

static unsigned long strlen_(const char *s)
{
    unsigned long n = 0;
    while (s[n]) n++;
    return n;
}

static void puts_(const char *s)
{
    syscall3(1, 1, (long)s, strlen_(s));
}

struct linux_dirent64 {
    unsigned long  d_ino;
    long           d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[256];
};

void _start(void)
{
    /* open("/", O_RDONLY) */
    long fd = syscall3(2, (long)"/", 0, 0);
    if (fd < 0) {
        puts_("ls: open / failed\n");
        syscall3(231, 0, 0, 0);
        for (;;) {}
    }

    char buf[1024];
    long n;
    while ((n = syscall3(217, fd, (long)buf, sizeof(buf))) > 0) {
        long off = 0;
        while (off < n) {
            struct linux_dirent64 *d = (struct linux_dirent64 *)(buf + off);
            puts_(d->d_name);
            puts_(d->d_type == 4 ? "/\n" : "\n");
            off += d->d_reclen;
        }
    }

    syscall3(3, fd, 0, 0);
    syscall3(231, 0, 0, 0);
    for (;;) {}
}
