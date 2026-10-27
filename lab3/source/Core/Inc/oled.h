#ifndef OLED_H
#define OLED_H

#include "gfx.h"

#include <stdbool.h>

/* Сброс через PCA9538 0xE0 (линия P0) и инициализация SSD1306 по таблице пособия. */
bool oled_init(void);
/* Отправляет текущий буфер gfx на дисплей. */
bool oled_flush(void);

#endif
