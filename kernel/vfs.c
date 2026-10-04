#include "vfs.h"
#include "serial.h"
#include "console.h"

/* Root node of the in-memory filesystem. */
extern struct vfs_node initramfs_root;

/* File descriptor table. Global for now — becomes per-process later. */
static struct vfs_fd {
    struct vfs_node *node;
    uint64_t         offset;
    int              used;
} fd_table[VFS_MAX_FDS];

void vfs_init(void)
{
    for (int i = 0; i < VFS_MAX_FDS; i++) {
        fd_table[i].used = 0;
        fd_table[i].node = 0;
        fd_table[i].offset = 0;
    }
}

/* Compare path segment (up to next '/' or '\0') against node name. */
static int segment_matches(const char *seg, int seg_len, const char *name)
{
    for (int i = 0; i < seg_len; i++) {
        if (name[i] == 0) return 0;
        if (name[i] != seg[i]) return 0;
    }
    return name[seg_len] == 0;
}

struct vfs_node *vfs_lookup(const char *path)
{
    if (!path || path[0] != '/') { serial_print("VL bad prefix\n"); return 0; }

    struct vfs_node *cur = &initramfs_root;
    const char *p = path + 1;

    while (*p) {
        /* Skip multiple slashes */
        while (*p == '/') p++;
        if (!*p) break;

        /* Find end of this segment */
        const char *start = p;
        while (*p && *p != '/') p++;
        int seg_len = (int)(p - start);

        if (cur->type != VFS_DIR) {
            serial_print("VL: not dir; cur=");
            serial_hex((uint64_t)cur);
            serial_print(" type=");
            serial_hex(cur->type);
            serial_print(" children=");
            serial_hex((uint64_t)cur->children);
            serial_print("\n");
            return 0;
        }

        /* Search children */
        struct vfs_node *child = cur->children;
        int found = 0;
        while (child) {
            if (segment_matches(start, seg_len, child->name)) {
                cur = child;
                found = 1;
                break;
            }
            child = child->next;
        }
        if (!found) return 0;
    }

    return cur;
}

int vfs_open(const char *path)
{
    struct vfs_node *n = vfs_lookup(path);
    if (!n) return -1;
    if (n->type != VFS_FILE) return -1;

    for (int i = 3; i < VFS_MAX_FDS; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used   = 1;
            fd_table[i].node   = n;
            fd_table[i].offset = 0;
            return i;
        }
    }
    return -1;
}

int vfs_read(int fd, void *buf, size_t n)
{
    if (fd < 0 || fd >= VFS_MAX_FDS) return -1;
    if (!fd_table[fd].used) return -1;

    struct vfs_fd *f = &fd_table[fd];
    struct vfs_node *node = f->node;

    if (f->offset >= node->size) return 0;

    size_t remain = node->size - f->offset;
    if (n > remain) n = remain;

    const uint8_t *src = node->data + f->offset;
    uint8_t *dst = (uint8_t *)buf;
    for (size_t i = 0; i < n; i++) dst[i] = src[i];

    f->offset += n;
    return (int)n;
}

int vfs_close(int fd)
{
    if (fd < 0 || fd >= VFS_MAX_FDS) return -1;
    if (!fd_table[fd].used) return -1;
    fd_table[fd].used = 0;
    fd_table[fd].node = 0;
    fd_table[fd].offset = 0;
    return 0;
}


int vfs_lseek(int fd, uint64_t offset, int whence)
{
    if (fd < 0 || fd >= VFS_MAX_FDS) return -1;
    if (!fd_table[fd].used) return -1;

    struct vfs_fd *f = &fd_table[fd];
    struct vfs_node *node = f->node;

    uint64_t new_off = f->offset;
    if (whence == 0) new_off = offset;                     /* SEEK_SET */
    else if (whence == 1) new_off = f->offset + offset;    /* SEEK_CUR */
    else if (whence == 2) new_off = node->size + offset;   /* SEEK_END */
    else return -1;

    f->offset = new_off;
    return (int)new_off;
}

struct vfs_node *vfs_fd_node(int fd)
{
    if (fd < 0 || fd >= VFS_MAX_FDS) return 0;
    if (!fd_table[fd].used) return 0;
    return fd_table[fd].node;
}

/* Iterate a directory. *cookie starts at 0 and is updated to point at
   the returned entry. Returns NULL when iteration is complete. */
struct vfs_node *vfs_readdir(struct vfs_node *dir, uint64_t *cookie)
{
    if (!dir || dir->type != VFS_DIR) return 0;

    uint64_t idx = *cookie;
    struct vfs_node *child = dir->children;
    for (uint64_t i = 0; child && i < idx; i++) child = child->next;
    if (!child) return 0;

    *cookie = idx + 1;
    return child;
}

void vfs_list(const char *path)
{
    struct vfs_node *n = vfs_lookup(path);
    if (!n) {
        console_puts("vfs_list: not found: ");
        console_puts(path);
        console_puts("\n");
        return;
    }
    if (n->type == VFS_FILE) {
        console_puts(path);
        console_puts(" (file, ");
        console_puts("\n");
        return;
    }

    console_puts(path);
    console_puts(":\n");
    for (struct vfs_node *c = n->children; c; c = c->next) {
        console_puts("  ");
        console_puts(c->name);
        console_puts(c->type == VFS_DIR ? "/\n" : "\n");
    }
}
