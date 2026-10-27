#include "button.h"
#include "gpio_drv.h"
#include "main.h"
#include <stdbool.h>

#define BUTTON_PORT GPIOC
#define BUTTON_PIN  GPIO_PIN_15
#define DEBOUNCE_MS 20

static bool stable_pressed;
static bool last_raw;
static uint32_t raw_changed_at;

void button_init(void)
{
	gpio_drv_config_t config = {
		.mode = GPIO_DRV_INPUT,
		.output = GPIO_DRV_PUSH_PULL,
		.speed = GPIO_DRV_SPEED_LOW,
		.pull = GPIO_DRV_NO_PULL,
		.level = false,
	};

	gpio_drv_init(BUTTON_PORT, BUTTON_PIN, config);
}

static bool read_raw(void)
{
	/* HAL: return HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN) == GPIO_PIN_RESET; */
	return !gpio_drv_read(BUTTON_PORT, BUTTON_PIN);
}

button_event_t button_poll(void)
{
	uint32_t now = HAL_GetTick();
	bool raw = read_raw();

	if (raw != last_raw) {
		last_raw = raw;
		raw_changed_at = now;
		return BUTTON_EVENT_NONE;
	}
	if (raw == stable_pressed || (now - raw_changed_at) < DEBOUNCE_MS)
		return BUTTON_EVENT_NONE;

	stable_pressed = raw;
	return stable_pressed ? BUTTON_EVENT_PRESS : BUTTON_EVENT_NONE;
}
