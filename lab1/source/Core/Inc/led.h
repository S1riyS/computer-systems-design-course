#ifndef LED_H
#define LED_H

#include <stdbool.h>

typedef enum {
	LED_GREEN,
	LED_RED,
	LED_YELLOW,
	LED_COUNT
} led_id_t;

void led_set(led_id_t led, bool on);
void led_all_off(void);

#endif
