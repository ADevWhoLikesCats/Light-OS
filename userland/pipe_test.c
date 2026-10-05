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

void _start(void)
{
    int fds[2];
    long rv = syscall3(22 /* pipe */, (long)fds, 0, 0);
    if (rv < 0) {
        puts_("pipe failed\n");
        syscall3(231, 0, 0, 0);
        for (;;) {}
    }

    const char *msg = "through the pipe\n";
    syscall3(1, fds[1], (long)msg, strlen_(msg));   /* write to pipe */
    syscall3(3, fds[1], 0, 0);                       /* close write end */

    char buf[64];
    long n = syscall3(0, fds[0], (long)buf, sizeof(buf));
    puts_("read from pipe: ");
    if (n > 0) {
        syscall3(1, 1, (long)buf, n);
    } else {
        puts_("(nothing)\n");
    }

    /* Test dup2: duplicate fds[0] to fd 5 */
    long d = syscall3(33 /* dup2 */, fds[0], 5, 0);
    puts_("dup2 returned: ");
    if (d == 5) puts_("5 (ok)\n"); else puts_("failed\n");

    syscall3(3, fds[0], 0, 0);
    syscall3(3, 5, 0, 0);
    syscall3(231, 0, 0, 0);
    for (;;) {}
}
