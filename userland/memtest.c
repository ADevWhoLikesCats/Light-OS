typedef unsigned long u64;

static long syscall6(long n, long a, long b, long c, long d, long e, long f)
{
    long ret;
    register long r10 __asm__("r10") = d;
    register long r8  __asm__("r8")  = e;
    register long r9  __asm__("r9")  = f;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9)
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

void _start(void)
{
    char buf[128];
    char *p;
    const char *msg;
    int i;

    /* --- brk test --- */
    long cur = syscall3(12, 0, 0, 0);
    long newbrk = syscall3(12, cur + 8192, 0, 0);

    p = buf;
    msg = "brk: ";
    while (*msg) *p++ = *msg++;
    for (i = 60; i >= 0; i -= 4) {
        int d = (cur >> i) & 0xF;
        *p++ = d < 10 ? '0'+d : 'a'+d-10;
    }
    *p++ = ' ';
    for (i = 60; i >= 0; i -= 4) {
        int d = (newbrk >> i) & 0xF;
        *p++ = d < 10 ? '0'+d : 'a'+d-10;
    }
    *p++ = '\n';
    syscall3(1, 1, (long)buf, p - buf);

    /* --- mmap test --- */
    long addr = syscall6(9, 0, 4096, 3 /*PROT_R|W*/, 0x22 /*PRIVATE|ANON*/, -1, 0);
    p = buf;
    msg = "mmap: ";
    while (*msg) *p++ = *msg++;
    for (i = 60; i >= 0; i -= 4) {
        int d = (addr >> i) & 0xF;
        *p++ = d < 10 ? '0'+d : 'a'+d-10;
    }
    *p++ = '\n';
    syscall3(1, 1, (long)buf, p - buf);

    /* Write to the mmap'd page and read it back */
    if (addr > 0 && addr != (long)-1) {
        char *mem = (char *)addr;
        mem[0] = 'H';
        mem[1] = 'i';
        mem[2] = '\n';
        syscall3(1, 1, addr, 3);
    }

    /* --- getpid / getuid --- */
    long pid = syscall3(39, 0, 0, 0);
    long uid = syscall3(102, 0, 0, 0);
    p = buf;
    msg = "pid=";
    while (*msg) *p++ = *msg++;
    for (i = 60; i >= 0; i -= 4) {
        int d = (pid >> i) & 0xF;
        *p++ = d < 10 ? '0'+d : 'a'+d-10;
    }
    *p++ = ' ';
    msg = "uid=";
    while (*msg) *p++ = *msg++;
    for (i = 60; i >= 0; i -= 4) {
        int d = (uid >> i) & 0xF;
        *p++ = d < 10 ? '0'+d : 'a'+d-10;
    }
    *p++ = '\n';
    syscall3(1, 1, (long)buf, p - buf);

    /* --- getrandom --- */
    unsigned char rnd[8];
    syscall3(318, (long)rnd, 8, 0);
    p = buf;
    msg = "random: ";
    while (*msg) *p++ = *msg++;
    for (i = 0; i < 8; i++) {
        int hi = (rnd[i] >> 4) & 0xF;
        int lo = rnd[i] & 0xF;
        *p++ = hi < 10 ? '0'+hi : 'a'+hi-10;
        *p++ = lo < 10 ? '0'+lo : 'a'+lo-10;
        *p++ = ' ';
    }
    *p++ = '\n';
    syscall3(1, 1, (long)buf, p - buf);

    syscall3(231, 0, 0, 0);   /* exit_group */
    for (;;) {}
}
