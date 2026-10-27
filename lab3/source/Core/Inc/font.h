#ifndef FONT_H
#define FONT_H

#include <stdint.h>

/* Указатель на 8 байт глифа. Неизвестный символ рисуется как пробел. */
const uint8_t *font_glyph(char c);

#endif
