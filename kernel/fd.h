#ifndef FD_H
#define FD_H

#include <stdint.h>
#include <stddef.h>

#define FD_MAX    256

#define FDOBJ_FILE 1
#define FDOBJ_PIPE 2

struct vfs_node;   /* forward decl */

struct pipe {
    uint8_t  buf[4096];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    int      readers;
    int      writers;
};

struct fdobj {
    int              kind;
    int              refcount;

    /* FILE */
    struct vfs_node *node;
    uint64_t         offset;

    /* PIPE */
    struct pipe     *pipe;
    int              write_end;
};

struct fdent {
    int             used;
    struct fdobj   *obj;
};

void          fd_init(void);
int           fd_alloc(struct fdobj *obj);     /* returns fd slot; refcount++ */
struct fdobj *fd_get(int fd);
int           fd_close(int fd);
int           fd_dup(int oldfd, int min_fd);
int           fd_dup2(int oldfd, int newfd);

struct fdobj *fdobj_file(struct vfs_node *node);

int  pipe_create(int fds[2]);
int  pipe_read(struct pipe *p, void *buf, uint64_t count);
int  pipe_write(struct pipe *p, const void *buf, uint64_t count);

#endif
