#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "vfs.h"
#include "syscall.h"
#include "serial.h"
#include "cat_elf.h"
#include "memtest_elf.h"
#include "stat_test_elf.h"
#include "time_test_elf.h"

void enter_userspace_elf(const void *elf, uint64_t len);

#define LINE_MAX 128

extern uint64_t elf_load(const void *elf_data, uint64_t elf_size, uint64_t load_bias);

static char line_buf[LINE_MAX];
static int  line_len = 0;

/* Print prompt */
static void prompt(void)
{
    console_puts("> ");
}

/* Read one line from keyboard, with echo and basic editing.
   Returns length of line (not counting null terminator). */
static int read_line(void)
{
    line_len = 0;
    for (;;) {
        char c = keyboard_getchar_blocking();
        if (!c) continue;

        if (c == '\n') {
            console_putchar('\n');
            line_buf[line_len] = 0;
            return line_len;
        }

        if (c == '\b') {
            if (line_len > 0) {
                line_len--;
                console_putchar('\b');
                console_putchar(' ');
                console_putchar('\b');
            }
            continue;
        }

        if (c == '\t') continue;    /* tab: ignore for now */

        if (line_len < LINE_MAX - 1) {
            line_buf[line_len++] = c;
            console_putchar(c);
        }
    }
}

/* Split line into argv tokens. Modifies line in-place.
   Returns argc. */
static int tokenize(char *s, char **argv, int max_args)
{
    int argc = 0;
    while (*s && argc < max_args) {
        while (*s == ' ' || *s == '\t') s++;
        if (!*s) break;
        argv[argc++] = s;
        while (*s && *s != ' ' && *s != '\t') s++;
        if (*s) *s++ = 0;
    }
    return argc;
}

static int strcmp_(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/* ---- Built-in commands ---- */

static void cmd_help(void)
{
    console_puts("commands:\n");
    console_puts("  help         show this\n");
    console_puts("  echo ARGS    print arguments\n");
    console_puts("  ls [PATH]    list directory\n");
    console_puts("  cat PATH     print file\n");
    console_puts("  clear        clear screen\n");
    console_puts("  peek         report kernel state\n");
    console_puts("  runelf       run embedded cat.elf in userspace\n");
    console_puts("  runmem       run embedded memtest.elf in userspace\n");
    console_puts("  runstat      run embedded stat_test.elf in userspace\n");
    console_puts("  runtime      run embedded time_test.elf in userspace\n");
}

static void cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        console_puts(argv[i]);
        if (i < argc - 1) console_putchar(' ');
    }
    console_putchar('\n');
}

static void cmd_ls(int argc, char **argv)
{
    const char *path = (argc >= 2) ? argv[1] : "/";
    vfs_list(path);
}

static void cmd_cat(int argc, char **argv)
{
    if (argc < 2) {
        console_puts("cat: missing file argument\n");
        return;
    }
    int fd = vfs_open(argv[1]);
    if (fd < 0) {
        console_puts("cat: cannot open ");
        console_puts(argv[1]);
        console_putchar('\n');
        return;
    }
    char buf[256];
    int n;
    while ((n = vfs_read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) console_putchar(buf[i]);
    }
    vfs_close(fd);
}

static void cmd_runtime(void)
{
    console_puts("runtime: loading embedded time_test.elf\n");
    enter_userspace_elf(time_test_elf, time_test_elf_len);
    console_puts("runtime: returned\n");
}

static void cmd_runstat(void)
{
    console_puts("runstat: loading embedded stat_test.elf\n");
    enter_userspace_elf(stat_test_elf, stat_test_elf_len);
    console_puts("runstat: returned\n");
}

static void cmd_runmem(void)
{
    console_puts("runmem: loading embedded memtest.elf\n");
    enter_userspace_elf(memtest_elf, memtest_elf_len);
    console_puts("runmem: returned\n");
}

static void cmd_runelf(void)
{
    console_puts("runelf: loading embedded cat.elf (");
    console_puts("...)\n");
    console_puts("runelf: entering userspace — kernel will halt on exit\n");
    for (volatile int i = 0; i < 1000000; i++) {}

    /* Map + jump using a modified enter_userspace path.
       For simplicity we invoke the existing syscall API. */
    {
        volatile const unsigned char *p = (volatile const unsigned char *)0x108600;
        serial_print("v: [0x108600]=");
        serial_hex(p[0]);
        serial_print(" [0x108601]=");
        serial_hex(p[1]);
        serial_print("\n");
    }
    serial_print("shell: cat_elf[0x2000]=");
    serial_hex(cat_elf[0x2000]);
    serial_print(" [0x2001]=");
    serial_hex(cat_elf[0x2001]);
    serial_print(" len=");
    serial_hex(cat_elf_len);
    serial_print("\n");

    enter_userspace_elf(cat_elf, cat_elf_len);

    console_puts("runelf: returned from userspace (unexpected)\n");
}

static void cmd_clear(void)
{
    console_clear();
}

static void cmd_peek(void)
{
    console_puts("kernel state:\n");
    console_puts("  (add counters here as the kernel grows)\n");
}

/* ---- Dispatch ---- */

static void run_command(int argc, char **argv)
{
    if (argc == 0) return;

    if (!strcmp_(argv[0], "help"))  { cmd_help();  return; }
    if (!strcmp_(argv[0], "echo"))  { cmd_echo(argc, argv); return; }
    if (!strcmp_(argv[0], "ls"))    { cmd_ls(argc, argv);   return; }
    if (!strcmp_(argv[0], "cat"))   { cmd_cat(argc, argv);  return; }
    if (!strcmp_(argv[0], "clear")) { cmd_clear();  return; }
    if (!strcmp_(argv[0], "peek"))  { cmd_peek();   return; }
    if (!strcmp_(argv[0], "runelf")){ cmd_runelf(); return; }
    if (!strcmp_(argv[0], "runmem")){ cmd_runmem(); return; }
    if (!strcmp_(argv[0], "runstat")){ cmd_runstat(); return; }
    if (!strcmp_(argv[0], "runtime")){ cmd_runtime(); return; }

    console_puts("unknown command: ");
    console_puts(argv[0]);
    console_puts("\n  (try 'help')\n");
}

void shell_loop(void)
{
    console_puts("mykernel shell — type 'help'\n\n");

    char *argv[16];
    for (;;) {
        prompt();
        int n = read_line();
        (void)n;
        int argc = tokenize(line_buf, argv, 16);
        run_command(argc, argv);
    }
}
