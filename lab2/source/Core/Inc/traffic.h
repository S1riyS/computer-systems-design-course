#ifndef TRAFFIC_H
#define TRAFFIC_H

#include <stdbool.h>
#include <stdint.h>

/* Пешеходный светофор варианта 1.
 * Фазы: красный, зелёный, мигающий зелёный, жёлтый.
 * Зелёный горит вчетверо меньше текущего красного. */
void traffic_init(void);
void traffic_update(void);

/* Режим 1 — кнопка сокращает красный. Режим 2 — нажатия игнорируются. */
void traffic_set_button_enabled(bool enabled);
bool traffic_button_enabled(void);

/* Длительность красного. Уже идущая фаза доигрывает старое время,
 * новое значение берётся при следующем входе в фазу. */
void traffic_set_red_ms(uint32_t ms);
uint32_t traffic_red_ms(void);

/* "red", "green", "blinking green" или "yellow". */
const char *traffic_light_name(void);

#endif
