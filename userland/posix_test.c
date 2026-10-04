typedef unsigned long u64;

static long syscall1(long n, long a)
{
    long ret;
    __asm__ volatile("syscall"
        : "=a"(ret)
        : "a"(n), "D"(a)
        : "rcx", "r11", "memory");
    return ret;
}

static long syscall2(long n, long a, long b)
{
    long ret;
    __asm__ volatile("syscall"
        : "=a"(ret)
        : "a"(n), "D"(a), "S"(b)
        : "rcx", "r11", "memory");
    return ret;
}

static long syscall3(long n, long a, long b, long c)
{
    long ret;
    __asm__ volatile("syscall"
        : "=a"(ret)
        : "a"(n), "D"(a), "S"(b), "d"(c)
        : "rcx", "r11", "memory");
    return ret;
}

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

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
    /* --- uname --- */
    struct utsname u;
    long r = syscall1(63, (long)&u);
    if (r == 0) {
        puts_("uname: ");
        puts_(u.sysname);
        puts_(" ");
        puts_(u.release);
        puts_(" ");
        puts_(u.machine);
        puts_("\n");
    } else {
        puts_("uname: FAILED\n");
    }

    /* --- getcwd --- */
    char cwd[256];
    r = syscall2(79, (long)cwd, sizeof(cwd));
    if (r > 0) {
        puts_("cwd: ");
        puts_(cwd);
        puts_("\n");
    } else {
        puts_("getcwd: FAILED\n");
    }

    /* --- chdir("/docs") then getcwd --- */
    r = syscall1(80, (long)"/docs");
    if (r == 0) {
        r = syscall2(79, (long)cwd, sizeof(cwd));
        if (r > 0) {
            puts_("after chdir /docs, cwd: ");
            puts_(cwd);
            puts_("\n");
        }
    } else {
        puts_("chdir /docs: FAILED\n");
    }

    /* --- access() --- */
    r = syscall2(21, (long)"/hello.txt", 0);
    puts_("access /hello.txt: ");
    puts_(r == 0 ? "OK\n" : "FAILED\n");

    r = syscall2(21, (long)"/nope", 0);
    puts_("access /nope: ");
    puts_(r == 0 ? "OK\n" : "FAILED (correct)\n");

    syscall3(231, 0, 0, 0);
    for (;;) {}
}
