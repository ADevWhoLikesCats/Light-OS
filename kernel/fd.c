#include "fd.h"
#include "vfs.h"
#include "pmm.h"      /* not really, but for hlt semantics */
#include "serial.h"

struct fdent fd_table[FD_MAX];

void fd_init(void)
{
    for (int i = 0; i < FD_MAX; i++) {
        fd_table[i].used = 0;
        fd_table[i].obj  = 0;
    }
}

static struct fdobj *fdobj_alloc(int kind)
{
    /* Very simple: allocate from a static pool of fdobjs. */
    static struct fdobj pool[FD_MAX];
    static int pool_next = 0;

    if (pool_next >= FD_MAX) return 0;
    struct fdobj *o = &pool[pool_next++];
    o->kind     = kind;
    o->refcount = 1;
    o->node     = 0;
    o->offset   = 0;
    o->pipe     = 0;
    o->write_end = 0;
    return o;
}

struct fdobj *fdobj_file(struct vfs_node *node)
{
    struct fdobj *o = fdobj_alloc(FDOBJ_FILE);
    if (o) o->node = node;
    return o;
}

int fd_alloc(struct fdobj *obj)
{
    if (!obj) return -1;
    for (int i = 3; i < FD_MAX; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = 1;
            fd_table[i].obj  = obj;
            obj->refcount++;
            return i;
        }
    }
    return -1;
}

struct fdobj *fd_get(int fd)
{
    if (fd < 0 || fd >= FD_MAX) return 0;
    if (!fd_table[fd].used) return 0;
    return fd_table[fd].obj;
}

int fd_close(int fd)
{
    if (fd < 0 || fd >= FD_MAX) return -1;
    if (!fd_table[fd].used) return -1;

    struct fdobj *o = fd_table[fd].obj;
    fd_table[fd].used = 0;
    fd_table[fd].obj  = 0;

    if (o) {
        o->refcount--;
        if (o->refcount <= 0 && o->kind == FDOBJ_PIPE && o->pipe) {
            if (o->write_end) o->pipe->writers--;
            else              o->pipe->readers--;
        }
    }
    return 0;
}

int fd_dup(int oldfd, int min_fd)
{
    struct fdobj *o = fd_get(oldfd);
    if (!o) return -1;

    for (int i = (min_fd < 3 ? 3 : min_fd); i < FD_MAX; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = 1;
            fd_table[i].obj  = o;
            o->refcount++;
            return i;
        }
    }
    return -1;
}

int fd_dup2(int oldfd, int newfd)
{
    if (oldfd == newfd) {
        if (!fd_get(oldfd)) return -1;
        return newfd;
    }
    if (newfd < 0 || newfd >= FD_MAX) return -1;

    struct fdobj *o = fd_get(oldfd);
    if (!o) return -1;

    if (fd_table[newfd].used) {
        fd_close(newfd);
    }
    fd_table[newfd].used = 1;
    fd_table[newfd].obj  = o;
    o->refcount++;
    return newfd;
}

/* ---------------- pipes ---------------- */

int pipe_create(int fds[2])
{
    struct pipe *p = (struct pipe *)pmm_alloc_page();   /* 4 KiB page = pipe buffer */
    if (!p) return -1;
    for (int i = 0; i < (int)sizeof(struct pipe); i++) ((uint8_t *)p)[i] = 0;
    p->readers = 0;
    p->writers = 0;

    struct fdobj *rd = fdobj_alloc(FDOBJ_PIPE);
    struct fdobj *wr = fdobj_alloc(FDOBJ_PIPE);
    if (!rd || !wr) return -1;
    rd->pipe = p; rd->write_end = 0;
    wr->pipe = p; wr->write_end = 1;

    int r = fd_alloc(rd);
    int w = fd_alloc(wr);
    if (r < 0 || w < 0) return -1;

    p->readers++;
    p->writers++;

    fds[0] = r;
    fds[1] = w;
    return 0;
}

int pipe_read(struct pipe *p, void *buf, uint64_t count)
{
    uint8_t *out = (uint8_t *)buf;
    uint64_t got = 0;

    while (got == 0) {
        if (p->count == 0) {
            if (p->writers == 0) return 0;   /* EOF */
            __asm__ volatile("hlt");
            continue;
        }
        break;
    }

    while (got < count && p->count > 0) {
        out[got++] = p->buf[p->head];
        p->head = (p->head + 1) & 4095;
        p->count--;
    }
    return (int)got;
}

int pipe_write(struct pipe *p, const void *buf, uint64_t count)
{
    const uint8_t *in = (const uint8_t *)buf;
    uint64_t put = 0;

    while (put < count) {
        if (p->count == 4096) {
            if (p->readers == 0) return -1;   /* EPIPE */
            __asm__ volatile("hlt");
            continue;
        }
        p->buf[p->tail] = in[put++];
        p->tail = (p->tail + 1) & 4095;
        p->count++;
    }
    return (int)put;
}
