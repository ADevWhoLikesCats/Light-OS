#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define VFS_NAME_MAX 64
#define VFS_MAX_FDS  32

#define VFS_FILE 1
#define VFS_DIR  2

struct vfs_node {
    char              name[VFS_NAME_MAX];
    uint32_t          type;
    uint32_t          size;
    const uint8_t    *data;         /* FILE: contents */
    struct vfs_node  *children;     /* DIR: linked list */
    struct vfs_node  *next;         /* sibling link */
};

void vfs_init(void);

/* Path lookup. Returns NULL if not found. */
struct vfs_node *vfs_lookup(const char *path);

/* Open returns an fd (>= 0) or -1. */
int  vfs_open(const char *path);
int  vfs_read(int fd, void *buf, size_t n);
int  vfs_close(int fd);
int  vfs_lseek(int fd, uint64_t offset, int whence);
/* Returns the next entry after *cookie; updates *cookie. NULL when done. */
struct vfs_node *vfs_readdir(struct vfs_node *dir, uint64_t *cookie);
struct vfs_node *vfs_fd_node(int fd);

/* Utility: print a directory listing to console. */
void vfs_list(const char *path);

#endif
