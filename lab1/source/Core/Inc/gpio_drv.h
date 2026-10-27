#ifndef GPIO_DRV_H
#define GPIO_DRV_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx.h"

typedef enum {
	GPIO_DRV_INPUT = 0,
	GPIO_DRV_OUTPUT = 1,
	GPIO_DRV_ALTERNATE = 2,
	GPIO_DRV_ANALOG = 3,
} gpio_drv_mode_t;

typedef enum {
	GPIO_DRV_PUSH_PULL = 0,
	GPIO_DRV_OPEN_DRAIN = 1,
} gpio_drv_output_t;

typedef enum {
	GPIO_DRV_SPEED_LOW = 0,
	GPIO_DRV_SPEED_MEDIUM = 1,
	GPIO_DRV_SPEED_HIGH = 2,
	GPIO_DRV_SPEED_VERY_HIGH = 3,
} gpio_drv_speed_t;

typedef enum {
	GPIO_DRV_NO_PULL = 0,
	GPIO_DRV_PULL_UP = 1,
	GPIO_DRV_PULL_DOWN = 2,
} gpio_drv_pull_t;

typedef struct {
	gpio_drv_mode_t mode;
	gpio_drv_output_t output;
	gpio_drv_speed_t speed;
	gpio_drv_pull_t pull;
	bool level; /* уровень выхода; записывается до смены режима */
} gpio_drv_config_t;

/* pins — маска GPIO_PIN_x, один пин или несколько пинов одного порта. */
void gpio_drv_init(GPIO_TypeDef *port, uint16_t pins, gpio_drv_config_t config);
gpio_drv_config_t gpio_drv_get(GPIO_TypeDef *port, uint16_t pin);

bool gpio_drv_read(GPIO_TypeDef *port, uint16_t pin);
uint16_t gpio_drv_read_port(GPIO_TypeDef *port, uint16_t mask);

void gpio_drv_write(GPIO_TypeDef *port, uint16_t pin, bool on);
void gpio_drv_write_port(GPIO_TypeDef *port, uint16_t mask, uint16_t values);

#endif
