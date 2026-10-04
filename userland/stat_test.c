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

struct mykernel_stat {
    unsigned long  st_dev;
    unsigned long  st_ino;
    unsigned long  st_nlink;
    unsigned int   st_mode;
    unsigned int   st_uid;
    unsigned int   st_gid;
    unsigned int   __pad0;
    unsigned long  st_rdev;
    long           st_size;
    long           st_blksize;
    long           st_blocks;
    unsigned long  st_atime;
    unsigned long  st_atime_nsec;
    unsigned long  st_mtime;
    unsigned long  st_mtime_nsec;
    unsigned long  st_ctime;
    unsigned long  st_ctime_nsec;
    long           __unused[3];
};

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

static void do_stat(const char *path)
{
    struct mykernel_stat st;
    char buf[256];
    char *p = buf;
    const char *msg;

    long rv = syscall3(4, (long)path, (long)&st, 0);

    msg = "stat ";
    while (*msg) *p++ = *msg++;
    while (*path) *p++ = *path++;
    *p++ = ':';
    *p++ = ' ';

    if (rv < 0) {
        msg = "FAILED (rv=";
        while (*msg) *p++ = *msg++;
        hex8(&p, (unsigned long)rv);
        *p++ = ')';
        *p++ = '\n';
        syscall3(1, 1, (long)buf, p - buf);
        return;
    }

    msg = "mode=";
    while (*msg) *p++ = *msg++;
    hex8(&p, st.st_mode);
    *p++ = ' ';

    msg = "size=";
    while (*msg) *p++ = *msg++;
    hex8(&p, (unsigned long)st.st_size);
    *p++ = ' ';

    msg = "nlink=";
    while (*msg) *p++ = *msg++;
    hex8(&p, st.st_nlink);
    *p++ = '\n';

    syscall3(1, 1, (long)buf, p - buf);
}

void _start(void)
{
    do_stat("/hello.txt");
    do_stat("/readme.txt");
    do_stat("/docs");
    do_stat("/nonexistent");

    syscall3(231, 0, 0, 0);
    for (;;) {}
}
