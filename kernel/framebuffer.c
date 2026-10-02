#include "framebuffer.h"
#include "vmm.h"
#include "pmm.h"
#include "serial.h"

#define PAGE_SIZE 4096

struct fb_info fb;

/* Original 0x6000 layout from stage2:
   0x6000: u32 framebuffer_phys
   0x6004: u32 width
   0x6008: u32 height
   0x600C: u32 pitch
   0x6010: u8  bpp
*/
struct stage2_fb_info {
    uint32_t phys;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t  bpp;
} __attribute__((packed));

void fb_init(void)
{
    const struct stage2_fb_info *s = (const struct stage2_fb_info *)0x6000;

    fb.phys   = s->phys;
    fb.width  = s->width;
    fb.height = s->height;
    fb.pitch  = s->pitch;
    fb.bpp    = s->bpp;

    serial_print("fb: phys=");
    serial_hex(fb.phys);
    serial_print(" w=");
    serial_hex(fb.width);
    serial_print(" h=");
    serial_hex(fb.height);
    serial_print(" pitch=");
    serial_hex(fb.pitch);
    serial_print(" bpp=");
    serial_hex(fb.bpp);
    serial_print("\n");

    /* Map the framebuffer at FB_VIRT_BASE. */
    uint64_t fb_size = (uint64_t)fb.pitch * fb.height;
    uint64_t pages   = (fb_size + PAGE_SIZE - 1) / PAGE_SIZE;

    uint64_t phys = fb.phys & ~0xFFFULL;
    for (uint64_t i = 0; i < pages; i++) {
        vmm_map_page(FB_VIRT_BASE + i * PAGE_SIZE,
                     phys + i * PAGE_SIZE,
                     PTE_WRITE);
    }

    serial_print("fb: mapped ");
    serial_hex(pages);
    serial_print(" pages at ");
    serial_hex(FB_VIRT_BASE);
    serial_print("\n");
}

struct fb_info *fb_get_info(void) { return &fb; }

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= fb.width || y >= fb.height) return;
    uint8_t *base = (uint8_t *)FB_VIRT_BASE;
    uint32_t *pixel = (uint32_t *)(base + y * fb.pitch + x * 4);
    *pixel = color;
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    if (x >= fb.width || y >= fb.height) return;
    if (x + w > fb.width)  w = fb.width  - x;
    if (y + h > fb.height) h = fb.height - y;

    uint8_t *base = (uint8_t *)FB_VIRT_BASE;
    for (uint32_t row = 0; row < h; row++) {
        uint32_t *line = (uint32_t *)(base + (y + row) * fb.pitch + x * 4);
        for (uint32_t col = 0; col < w; col++) {
            line[col] = color;
        }
    }
}

void fb_clear(uint32_t color)
{
    fb_fill_rect(0, 0, fb.width, fb.height, color);
}
