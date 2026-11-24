#include "usart_drv.h"
#include "stm32f4xx.h"

enum {
	GPIO_MODE_ALTERNATE = 2,
	GPIO_SPEED_VERY_HIGH = 3,
	USART6_GPIO_AF = 8,
};

/* OVER16: в BRR лежит round(PCLK / baud).
 * 90 000 000 / 57 600 = 1562,5, поэтому в регистр пишется 1563.
 * Это то же число, что HAL собирает как 97 + 11/16. */
static uint32_t brr_value(uint32_t pclk, uint32_t baudrate)
{
	return (pclk + baudrate / 2U) / baudrate;
}

static bool has_error_flags(uint32_t sr)
{
	return (sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0;
}

/* ORE на STM32F4 сбрасывается только последовательностью «прочитал SR, потом DR». */
static void clear_error_flags(void)
{
	(void)USART6->SR;
	(void)USART6->DR;
}

static void gpio_init(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
	(void)RCC->AHB1ENR;

	GPIOC->MODER &= ~(GPIO_MODER_MODER6 | GPIO_MODER_MODER7);
	GPIOC->MODER |= (GPIO_MODE_ALTERNATE << GPIO_MODER_MODER6_Pos) |
	                (GPIO_MODE_ALTERNATE << GPIO_MODER_MODER7_Pos);

	GPIOC->OTYPER &= ~(GPIO_OTYPER_OT6 | GPIO_OTYPER_OT7);

	GPIOC->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED6 | GPIO_OSPEEDR_OSPEED7);
	GPIOC->OSPEEDR |= (GPIO_SPEED_VERY_HIGH << GPIO_OSPEEDR_OSPEED6_Pos) |
	                  (GPIO_SPEED_VERY_HIGH << GPIO_OSPEEDR_OSPEED7_Pos);

	GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPDR6 | GPIO_PUPDR_PUPDR7);

	/* PC6 — USART6_TX, PC7 — USART6_RX, альтернативная функция AF8. */
	GPIOC->AFR[0] &= ~(GPIO_AFRL_AFSEL6 | GPIO_AFRL_AFSEL7);
	GPIOC->AFR[0] |= (USART6_GPIO_AF << GPIO_AFRL_AFSEL6_Pos) |
	                 (USART6_GPIO_AF << GPIO_AFRL_AFSEL7_Pos);
}

void usart_drv_init(usart_drv_mode_t mode, uint32_t baudrate)
{
	gpio_init();

	RCC->APB2ENR |= RCC_APB2ENR_USART6EN;
	(void)RCC->APB2ENR;

	/* Пока UE = 0, можно спокойно записать делитель и формат кадра 8N1. */
	USART6->CR1 = 0;
	USART6->CR2 = 0;
	USART6->CR3 = 0;
	USART6->BRR = brr_value(USART_DRV_PCLK2_HZ, baudrate);
	USART6->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

	usart_drv_set_mode(mode);
}

void usart_drv_set_mode(usart_drv_mode_t mode)
{
	if (mode == USART_DRV_INTERRUPTS) {
		uint8_t byte;

		/* Байт, уже лежащий в DR, забираем до разрешения прерывания,
		 * иначе он дождётся только следующего RXNE. */
		if (usart_drv_read(&byte) == USART_DRV_OK)
			usart_drv_on_rx(byte);

		USART6->CR1 |= USART_CR1_RXNEIE;
		NVIC_ClearPendingIRQ(USART6_IRQn);
		NVIC_SetPriority(USART6_IRQn, 0);
		NVIC_EnableIRQ(USART6_IRQn);
	} else {
		USART6->CR1 &= ~(USART_CR1_RXNEIE | USART_CR1_TXEIE);
		NVIC_DisableIRQ(USART6_IRQn);
		NVIC_ClearPendingIRQ(USART6_IRQn);
	}
}

usart_drv_status_t usart_drv_read(uint8_t *byte)
{
	uint32_t sr = USART6->SR;

	if (sr & USART_SR_RXNE) {
		*byte = (uint8_t)USART6->DR;
		return USART_DRV_OK;
	}

	if (has_error_flags(sr))
		clear_error_flags();

	return USART_DRV_EMPTY;
}

usart_drv_status_t usart_drv_write(uint8_t byte)
{
	if (!usart_drv_tx_ready())
		return USART_DRV_BUSY;
	USART6->DR = byte;
	return USART_DRV_OK;
}

void usart_drv_write_blocking(uint8_t byte)
{
	while (!usart_drv_tx_ready()) {
	}
	USART6->DR = byte;
}

void usart_drv_flush(void)
{
	while (!(USART6->SR & USART_SR_TC)) {
	}
}

bool usart_drv_tx_ready(void)
{
	return (USART6->SR & USART_SR_TXE) != 0;
}

void usart_drv_enable_tx_irq(bool enable)
{
	if (enable)
		USART6->CR1 |= USART_CR1_TXEIE;
	else
		USART6->CR1 &= ~USART_CR1_TXEIE;
}

void usart_drv_irq_disable(void)
{
	NVIC_DisableIRQ(USART6_IRQn);
}

void usart_drv_irq_enable(void)
{
	NVIC_EnableIRQ(USART6_IRQn);
}

void usart_drv_irq_handler(void)
{
	uint32_t sr = USART6->SR;

	if (sr & USART_SR_RXNE) {
		uint8_t byte = (uint8_t)USART6->DR;
		usart_drv_on_rx(byte);
	} else if (has_error_flags(sr)) {
		clear_error_flags();
	}

	if ((sr & USART_SR_TXE) && (USART6->CR1 & USART_CR1_TXEIE))
		usart_drv_on_tx_ready();
}

void USART6_IRQHandler(void)
{
	usart_drv_irq_handler();
}
