#include "console.h"
#include "framebuffer.h"
#include "font8x16.h"

#define TEXT_COLOR  0x00FFFFFF
#define BG_COLOR    0x00000000

static uint32_t cursor_col = 0;
static uint32_t cursor_row = 0;
static uint32_t cols = 0;
static uint32_t rows = 0;

/* How many scanlines one text row occupies. Scale up if screen is huge. */
static uint32_t scale = 1;

static void draw_glyph(uint32_t col, uint32_t row, char c)
{
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = font8x16[c - 32];

    uint32_t px = col * FONT_W * scale;
    uint32_t py = row * FONT_H * scale;

    for (uint32_t y = 0; y < FONT_H; y++) {
        uint8_t bits = glyph[y];
        for (uint32_t x = 0; x < FONT_W; x++) {
            uint32_t color = (bits & (0x80 >> x)) ? TEXT_COLOR : BG_COLOR;
            /* Draw the scaled-up pixel block */
            for (uint32_t sy = 0; sy < scale; sy++) {
                for (uint32_t sx = 0; sx < scale; sx++) {
                    fb_putpixel(px + x * scale + sx, py + y * scale + sy, color);
                }
            }
        }
    }
}

static void erase_cell(uint32_t col, uint32_t row)
{
    uint32_t px = col * FONT_W * scale;
    uint32_t py = row * FONT_H * scale;
    for (uint32_t y = 0; y < FONT_H * scale; y++) {
        for (uint32_t x = 0; x < FONT_W * scale; x++) {
            fb_putpixel(px + x, py + y, BG_COLOR);
        }
    }
}

static void scroll_up(void)
{
    struct fb_info *fb = fb_get_info();
    uint8_t *base = (uint8_t *)FB_VIRT_BASE;

    uint32_t line_bytes = FONT_H * scale * fb->pitch;
    uint32_t screen_lines = fb->height;
    uint32_t first_line = FONT_H * scale;
    uint32_t copy_lines = screen_lines - first_line;

    /* memmove rows 1..N up by one text row */
    uint8_t *dst = base;
    uint8_t *src = base + line_bytes;
    for (uint32_t y = 0; y < copy_lines; y++) {
        uint32_t *d = (uint32_t *)(dst + y * fb->pitch);
        uint32_t *s = (uint32_t *)(src + y * fb->pitch);
        for (uint32_t x = 0; x < fb->width; x++) d[x] = s[x];
    }

    /* Clear bottom text row */
    uint32_t blank_y0 = (rows - 1) * FONT_H * scale;
    for (uint32_t y = 0; y < FONT_H * scale; y++) {
        uint32_t *line = (uint32_t *)(base + (blank_y0 + y) * fb->pitch);
        for (uint32_t x = 0; x < fb->width; x++) line[x] = BG_COLOR;
    }
}

void console_init(void)
{
    struct fb_info *fb = fb_get_info();

    /* Choose a scale that fits about 40 rows of text vertically.
       For 768px tall and 16px font: 768/16 = 48 rows at scale 1.
       That's fine — keep scale 1. */
    scale = 1;

    cols = fb->width  / (FONT_W * scale);
    rows = fb->height / (FONT_H * scale);
    cursor_col = 0;
    cursor_row = 0;

    console_clear();
}

void console_clear(void)
{
    fb_clear(BG_COLOR);
    cursor_col = 0;
    cursor_row = 0;
}

void console_putchar(char c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = cols - 1;
        }
        erase_cell(cursor_col, cursor_row);
        return;
    } else if (c == '\t') {
        uint32_t next = (cursor_col + 8) & ~7u;
        while (cursor_col < next && cursor_col < cols) {
            console_putchar(' ');
        }
        return;
    } else {
        draw_glyph(cursor_col, cursor_row, c);
        cursor_col++;
        if (cursor_col >= cols) {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= rows) {
        scroll_up();
        cursor_row = rows - 1;
    }
}

void console_puts(const char *s)
{
    while (*s) console_putchar(*s++);
}

void console_puts_at(uint32_t col, uint32_t row, const char *s)
{
    cursor_col = col;
    cursor_row = row;
    console_puts(s);
}
