#ifndef I2C_H
#define I2C_H

#include "main.h"

#include <stdbool.h>
#include <stdint.h>

extern I2C_HandleTypeDef hi2c1;

void MX_I2C1_Init(void);

/* Адрес устройства — 8 бит, младший бит R/W равен 0, как ждёт HAL.
 * OLED SSD1306 — 0x78, расширитель сброса — 0xE0, MCP79411 — 0xDE. */
bool i2c_mem_write(uint8_t addr, uint8_t reg, const uint8_t *data, uint16_t len);
bool i2c_mem_read(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len);
bool i2c_master_write(uint8_t addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms);

#endif
