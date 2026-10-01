#include "led.h"
#include "main.h"


static const struct {
	GPIO_TypeDef *port;
	uint16_t pin;
} leds[LED_COUNT] = {
	[LED_GREEN]  = { GPIOD, GPIO_PIN_13 },
	[LED_RED]    = { GPIOD, GPIO_PIN_15 },
	[LED_YELLOW] = { GPIOD, GPIO_PIN_14 },
};

void led_set(led_id_t led, bool on)
{
	if ((led == LED_RED || led == LED_YELLOW) && on) {
		led_id_t other = (led == LED_RED) ? LED_YELLOW : LED_RED;
		HAL_GPIO_WritePin(leds[other].port, leds[other].pin, GPIO_PIN_RESET);
	}
	HAL_GPIO_WritePin(leds[led].port, leds[led].pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void led_all_off(void)
{
	for (int i = 0; i < LED_COUNT; i++)
		led_set((led_id_t)i, false);
}
