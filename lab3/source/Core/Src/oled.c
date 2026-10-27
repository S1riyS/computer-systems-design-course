#include "oled.h"

#include "i2c.h"
#include "main.h"
#include "pca9538.h"

#include <string.h>

#define OLED_ADDR 0x78
#define RESET_EXPANDER_ADDR 0xE0
#define RESET_MASK 0x01u

/* Управляющий байт 0x00: дальше идут команды, и это последнее слово посылки. */
static const uint8_t init_cmd[] = {
	0x00,
	0xAE,
	0x20, 0x00,
	0xC8,
	0x40,
	0x81, 0xFF,
	0xA1,
	0xA6,
	0xA8, 0x3F,
	0xA4,
	0xD3, 0x00,
	0xD5, 0xF0,
	0xD9, 0x22,
	0xDA, 0x12,
	0xDB, 0x20,
	0x8D, 0x14,
	0xAF
};

/* Столбцы 0..127 и страницы 0..7, указатель записи в начало. */
static const uint8_t window_cmd[] = {
	0x00,
	0x21, 0x00, 0x7F,
	0x22, 0x00, 0x07
};

static bool oled_reset(void)
{
	/* Бит конфигурации 1 — вход. Выходом делаем только P0, это сброс дисплея. */
	if (!pca9538_write(RESET_EXPANDER_ADDR, PCA9538_REG_CONFIG, (uint8_t)~RESET_MASK))
		return false;
	if (!pca9538_write(RESET_EXPANDER_ADDR, PCA9538_REG_OUTPUT, 0xFF))
		return false;
	HAL_Delay(1);
	if (!pca9538_write(RESET_EXPANDER_ADDR, PCA9538_REG_OUTPUT, (uint8_t)~RESET_MASK))
		return false;
	HAL_Delay(1);
	if (!pca9538_write(RESET_EXPANDER_ADDR, PCA9538_REG_OUTPUT, 0xFF))
		return false;
	HAL_Delay(1);
	return true;
}

bool oled_flush(void)
{
	static uint8_t packet[1 + GFX_BUFFER_SIZE];

	if (!i2c_master_write(OLED_ADDR, window_cmd, sizeof(window_cmd), 50))
		return false;
	packet[0] = 0x40;
	memcpy(packet + 1, gfx_framebuffer(), GFX_BUFFER_SIZE);
	return i2c_master_write(OLED_ADDR, packet, sizeof(packet), 200);
}

bool oled_init(void)
{
	if (!oled_reset())
		return false;
	if (!i2c_master_write(OLED_ADDR, init_cmd, sizeof(init_cmd), 50))
		return false;
	HAL_Delay(10);
	gfx_fill(0);
	return oled_flush();
}
