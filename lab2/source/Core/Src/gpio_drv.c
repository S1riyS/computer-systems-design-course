#include "gpio_drv.h"

static unsigned pin_index(uint16_t pin)
{
	unsigned index = 0;

	while (index < 16u && (pin & (uint16_t)(1u << index)) == 0u)
		index++;
	return index;
}

static uint32_t field_mask(uint16_t pins)
{
	uint32_t mask = 0;

	for (unsigned index = 0; index < 16u; index++) {
		if (pins & (uint16_t)(1u << index))
			mask |= 3u << (index * 2u);
	}
	return mask;
}

static uint32_t field_value(uint16_t pins, uint32_t value)
{
	uint32_t bits = 0;

	for (unsigned index = 0; index < 16u; index++) {
		if (pins & (uint16_t)(1u << index))
			bits |= (value & 3u) << (index * 2u);
	}
	return bits;
}

static void write_field(volatile uint32_t *reg, uint16_t pins, uint32_t value)
{
	uint32_t mask = field_mask(pins);

	*reg = (*reg & ~mask) | field_value(pins, value);
}

static void enable_clock(GPIO_TypeDef *port)
{
	if (port == GPIOA)
		RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	else if (port == GPIOB)
		RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
	else if (port == GPIOC)
		RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
	else if (port == GPIOD)
		RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
	else if (port == GPIOE)
		RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
	else if (port == GPIOH)
		RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN;

	/* Чтение фиксирует запись в RCC до обращения к регистрам порта. */
	(void)RCC->AHB1ENR;
}

static void write_level(GPIO_TypeDef *port, uint16_t pins, bool on)
{
	/* BSRR меняет выбранные биты одной записью и не трогает остальные. */
	if (on)
		port->BSRR = pins;
	else
		port->BSRR = (uint32_t)pins << 16;
}

void gpio_drv_init(GPIO_TypeDef *port, uint16_t pins, gpio_drv_config_t config)
{
	enable_clock(port);

	if (config.mode == GPIO_DRV_OUTPUT) {
		/* Сначала нужный уровень, и только потом перевод пина в выход.
		 * Иначе в момент смены MODER на ножке кратко окажется старое значение. */
		write_level(port, pins, config.level);
		port->OTYPER = (port->OTYPER & ~pins) |
		               (config.output == GPIO_DRV_OPEN_DRAIN ? pins : 0u);
		write_field(&port->OSPEEDR, pins, config.speed);
		write_field(&port->PUPDR, pins, config.pull);
		write_field(&port->MODER, pins, config.mode);
	} else {
		/* С выхода на вход: сначала перестать выдавать уровень наружу. */
		write_field(&port->MODER, pins, config.mode);
		port->OTYPER = (port->OTYPER & ~pins) |
		               (config.output == GPIO_DRV_OPEN_DRAIN ? pins : 0u);
		write_field(&port->OSPEEDR, pins, config.speed);
		write_field(&port->PUPDR, pins, config.pull);
	}
}

gpio_drv_config_t gpio_drv_get(GPIO_TypeDef *port, uint16_t pin)
{
	unsigned index = pin_index(pin);
	gpio_drv_config_t config;

	config.mode = (gpio_drv_mode_t)((port->MODER >> (index * 2u)) & 3u);
	config.output = (port->OTYPER & pin) ? GPIO_DRV_OPEN_DRAIN : GPIO_DRV_PUSH_PULL;
	config.speed = (gpio_drv_speed_t)((port->OSPEEDR >> (index * 2u)) & 3u);
	config.pull = (gpio_drv_pull_t)((port->PUPDR >> (index * 2u)) & 3u);
	config.level = (port->ODR & pin) != 0;
	return config;
}

bool gpio_drv_read(GPIO_TypeDef *port, uint16_t pin)
{
	return (port->IDR & pin) != 0;
}

uint16_t gpio_drv_read_port(GPIO_TypeDef *port, uint16_t mask)
{
	return (uint16_t)(port->IDR & mask);
}

void gpio_drv_write(GPIO_TypeDef *port, uint16_t pin, bool on)
{
	write_level(port, pin, on);
}

void gpio_drv_write_port(GPIO_TypeDef *port, uint16_t mask, uint16_t values)
{
	uint16_t set = (uint16_t)(mask & values);
	uint16_t reset = (uint16_t)(mask & (uint16_t)~values);

	port->BSRR = (uint32_t)set | ((uint32_t)reset << 16);
}
