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

static void hex8(char **p, unsigned long v)
{
    for (int i = 60; i >= 0; i -= 4) {
        int d = (v >> i) & 0xF;
        *(*p)++ = d < 10 ? '0'+d : 'a'+d-10;
    }
}

void _start(void)
{
    char buf[128];
    char *p;

    /* TIOCGWINSZ on fd 1 */
    struct { unsigned short r, c, xp, yp; } ws;
    long rv = syscall3(16 /*ioctl*/, 1, 0x5413 /*TIOCGWINSZ*/, (long)&ws);
    p = buf;
    puts_("winsize: ");
    if (rv == 0) {
        hex8(&p, ws.r); *p++ = 'x'; hex8(&p, ws.c); *p++ = '\n';
        syscall3(1, 1, (long)buf, p - buf);
    } else {
        puts_("failed\n");
    }

    /* Test FIONREAD on a pipe */
    int fds[2];
    syscall3(22 /*pipe*/, (long)fds, 0, 0);
    const char *msg = "hello pipe";
    syscall3(1, fds[1], (long)msg, strlen_(msg));

    int avail = 0;
    rv = syscall3(16, fds[0], 0x541B /*FIONREAD*/, (long)&avail);
    p = buf;
    puts_("bytes in pipe: ");
    if (rv == 0) {
        hex8(&p, avail); *p++ = '\n';
        syscall3(1, 1, (long)buf, p - buf);
    } else {
        puts_("failed\n");
    }

    syscall3(3, fds[0], 0, 0);
    syscall3(3, fds[1], 0, 0);
    syscall3(231, 0, 0, 0);
    for (;;) {}
}
