#include "led.h"
#include "gpio_drv.h"
#include "main.h"


static const struct {
	GPIO_TypeDef *port;
	uint16_t pin;
} leds[LED_COUNT] = {
	[LED_GREEN]  = { GPIOD, GPIO_PIN_13 },
	[LED_RED]    = { GPIOD, GPIO_PIN_15 },
	[LED_YELLOW] = { GPIOD, GPIO_PIN_14 },
};

void led_init(void)
{
	gpio_drv_config_t config = {
		.mode = GPIO_DRV_OUTPUT,
		.output = GPIO_DRV_PUSH_PULL,
		.speed = GPIO_DRV_SPEED_LOW,
		.pull = GPIO_DRV_NO_PULL,
		.level = false,
	};

	gpio_drv_init(GPIOD, GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15, config);
}

void led_set(led_id_t led, bool on)
{
	if ((led == LED_RED || led == LED_YELLOW) && on) {
		led_id_t other = (led == LED_RED) ? LED_YELLOW : LED_RED;
		/* HAL: HAL_GPIO_WritePin(leds[other].port, leds[other].pin, GPIO_PIN_RESET); */
		gpio_drv_write(leds[other].port, leds[other].pin, false);
	}
	/* HAL: HAL_GPIO_WritePin(leds[led].port, leds[led].pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET); */
	gpio_drv_write(leds[led].port, leds[led].pin, on);
}

void led_all_off(void)
{
	for (int i = 0; i < LED_COUNT; i++)
		led_set((led_id_t)i, false);
}
