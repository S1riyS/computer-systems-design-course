#ifndef PCA9538_H
#define PCA9538_H

#include <stdbool.h>
#include <stdint.h>

#define PCA9538_REG_INPUT 0x00
#define PCA9538_REG_OUTPUT 0x01
#define PCA9538_REG_POLARITY 0x02
#define PCA9538_REG_CONFIG 0x03

/* addr — адресный байт HAL: 0xE0 у расширителя сброса дисплея, 0xE2 у клавиатуры. */
bool pca9538_write(uint8_t addr, uint8_t reg, uint8_t value);
bool pca9538_read(uint8_t addr, uint8_t reg, uint8_t *value);

#endif
