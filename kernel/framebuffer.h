#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

/* Framebuffer info, filled by stage2 and read at fb_init time. */
struct fb_info {
    uint64_t phys;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t  bpp;
};

#define FB_VIRT_BASE  0x50000000ULL
#define FB_MAX_WIDTH  1920
#define FB_MAX_HEIGHT 1080

/* Colors: 0x00RRGGBB (we convert to BGRA when writing). */
#define FB_BLACK    0x00000000
#define FB_WHITE    0x00FFFFFF
#define FB_RED      0x00FF0000
#define FB_GREEN    0x0000FF00
#define FB_BLUE     0x000000FF
#define FB_YELLOW   0x00FFFF00

void fb_init(void);
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_clear(uint32_t color);
struct fb_info *fb_get_info(void);

#endif
