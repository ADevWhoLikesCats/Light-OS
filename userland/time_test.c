typedef unsigned long u64;

static long syscall2(long n, long a, long b)
{
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a), "S"(b)
        : "rcx", "r11", "memory"
    );
    return ret;
}

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

struct timespec { long tv_sec; long tv_nsec; };

static unsigned long strlen_(const char *s)
{
    unsigned long n = 0;
    while (s[n]) n++;
    return n;
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
    const char *msg;
    struct timespec ts;

    syscall2(228, 1, (long)&ts);
    p = buf;
    msg = "mono t0: ";
    while (*msg) *p++ = *msg++;
    hex8(&p, ts.tv_sec);
    *p++ = '.';
    hex8(&p, ts.tv_nsec);
    *p++ = '\n';
    syscall3(1, 1, (long)buf, p - buf);

    msg = "sleeping 1 sec...\n";
    syscall3(1, 1, (long)msg, strlen_(msg));

    ts.tv_sec = 1;
    ts.tv_nsec = 0;
    syscall2(35, (long)&ts, 0);

    syscall2(228, 1, (long)&ts);
    p = buf;
    msg = "mono t1: ";
    while (*msg) *p++ = *msg++;
    hex8(&p, ts.tv_sec);
    *p++ = '.';
    hex8(&p, ts.tv_nsec);
    *p++ = '\n';
    syscall3(1, 1, (long)buf, p - buf);

    syscall3(231, 0, 0, 0);
    for (;;) {}
}
