#ifndef GFX_H
#define GFX_H

#include <stdint.h>

#define GFX_WIDTH 128
#define GFX_HEIGHT 64
#define GFX_BUFFER_SIZE (GFX_WIDTH * GFX_HEIGHT / 8)

/* Буфер кадра: страница SSD1306 (8 строк) подряд, внутри страницы — столбцы слева направо.
 * Младший бит байта — верхний пиксель столбца. */
void gfx_fill(uint8_t color);
void gfx_draw_pixel(int x, int y, uint8_t color);
void gfx_set_cursor(int x, int y);
void gfx_set_scale(uint8_t scale);
void gfx_write_char(char c);
void gfx_write_string(const char *text);
const uint8_t *gfx_framebuffer(void);

#endif
