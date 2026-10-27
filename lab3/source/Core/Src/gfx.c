#include "gfx.h"

#include "font.h"

#include <string.h>

static uint8_t framebuffer[GFX_BUFFER_SIZE];
static int cursor_x;
static int cursor_y;
static uint8_t scale = 1;

void gfx_fill(uint8_t color)
{
	memset(framebuffer, color ? 0xFF : 0x00, sizeof(framebuffer));
}

void gfx_draw_pixel(int x, int y, uint8_t color)
{
	uint16_t index;
	uint8_t mask;

	if (x < 0 || y < 0 || x >= GFX_WIDTH || y >= GFX_HEIGHT)
		return;

	index = (uint16_t)((y / 8) * GFX_WIDTH + x);
	mask = (uint8_t)(1u << (y % 8));
	if (color)
		framebuffer[index] |= mask;
	else
		framebuffer[index] &= (uint8_t)~mask;
}

void gfx_set_cursor(int x, int y)
{
	cursor_x = x;
	cursor_y = y;
}

void gfx_set_scale(uint8_t value)
{
	scale = value == 0 ? 1 : value;
}

void gfx_write_char(char c)
{
	const uint8_t *glyph = font_glyph(c);
	uint8_t step = scale;

	for (int row = 0; row < 8; row++) {
		for (int col = 0; col < 8; col++) {
			if ((glyph[row] & (1u << col)) == 0)
				continue;
			for (uint8_t dy = 0; dy < step; dy++) {
				for (uint8_t dx = 0; dx < step; dx++) {
					gfx_draw_pixel(cursor_x + col * step + dx,
						       cursor_y + row * step + dy, 1);
				}
			}
		}
	}
	cursor_x += 8 * step;
}

void gfx_write_string(const char *text)
{
	if (text == NULL)
		return;
	while (*text != '\0')
		gfx_write_char(*text++);
}

const uint8_t *gfx_framebuffer(void)
{
	return framebuffer;
}
