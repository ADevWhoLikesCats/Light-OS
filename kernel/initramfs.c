#include "vfs.h"

/* --- File contents --- */
static const uint8_t hello_txt[]   = "hello from initramfs\n";
static const uint8_t readme_txt[]  =
    "mykernel v0.1\n"
    "a hobby OS written in C and asm\n";
static const uint8_t about_txt[]   = "written in C and asm, from scratch\n";

/* --- Directory entries --- */
static struct vfs_node docs_children[] = {
    { "about.txt", VFS_FILE, sizeof(about_txt) - 1, about_txt, 0, 0 },
};

/* --- Tree layout --- */
static struct vfs_node root_children[] = {
    { "hello.txt",  VFS_FILE, sizeof(hello_txt) - 1,  hello_txt,  0, &root_children[1] },
    { "readme.txt", VFS_FILE, sizeof(readme_txt) - 1, readme_txt, 0, &root_children[2] },
    { "docs",       VFS_DIR,  0,                      0,          docs_children, 0 },
};

struct vfs_node initramfs_root = {
    "/", VFS_DIR, 0, 0, root_children, 0
};
