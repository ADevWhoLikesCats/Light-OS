#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define VFS_NAME_MAX 64
#define VFS_MAX_FDS  32

#define VFS_FILE 1
#define VFS_DIR  2

/* POSIX st_mode bits */
#define S_IFREG  0x8000
#define S_IFDIR  0x4000
#define S_IFMT   0xF000

#define VFS_MODE_FILE  (S_IFREG | 0644)
#define VFS_MODE_DIR   (S_IFDIR | 0755)
#define VFS_MODE_EXEC  (S_IFREG | 0755)

struct vfs_node {
    char              name[VFS_NAME_MAX];
    uint32_t          type;       /* VFS_FILE or VFS_DIR */
    uint32_t          mode;       /* POSIX st_mode */
    uint32_t          uid;
    uint32_t          gid;
    uint32_t          size;
    const uint8_t    *data;       /* FILE: contents */
    struct vfs_node  *children;   /* DIR: linked list */
    struct vfs_node  *next;       /* sibling link */
};

void vfs_init(void);

struct vfs_node *vfs_lookup(const char *path);

int  vfs_open(const char *path);
int  vfs_read(int fd, void *buf, size_t n);
int  vfs_close(int fd);
int  vfs_lseek(int fd, uint64_t offset, int whence);
int  vfs_getdents64(int fd, void *buf, uint64_t count);
uint64_t vfs_get_fd_offset(int fd);
int  vfs_set_fd_offset(int fd, uint64_t offset);

struct vfs_node *vfs_readdir(struct vfs_node *dir, uint64_t *cookie);
struct vfs_node *vfs_fd_node(int fd);

/* Read an entire file into a caller-provided pointer. Returns 0 on
   success, -1 if not found or not a regular file. */
int vfs_read_file(const char *path, const uint8_t **data, uint64_t *size);

void vfs_list(const char *path);

#endif
