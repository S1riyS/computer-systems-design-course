#include "uart.h"
#include "fifo.h"
#include "usart_drv.h"
#include <string.h>

/* Отладочный UART стенда: USART6 (PC6/PC7) -> программатор -> виртуальный COM-порт. */

static uint8_t rx_storage[UART_RX_BUFFER_SIZE];
static uint8_t tx_storage[UART_TX_BUFFER_SIZE];
static fifo_t rx_fifo;
static fifo_t tx_fifo;

static volatile bool interrupts_enabled;
static volatile bool tx_busy;

/* Вызывается из прерывания или при уже запрещённом векторе USART6. */
static void tx_start_next(void)
{
	uint8_t byte;

	if (usart_drv_tx_ready() && fifo_pop(&tx_fifo, &byte)) {
		tx_busy = true;
		(void)usart_drv_write(byte);
	} else {
		tx_busy = false;
	}
	usart_drv_enable_tx_irq(tx_busy || !fifo_is_empty(&tx_fifo));
}

void uart_init(bool use_interrupts)
{
	fifo_init(&rx_fifo, rx_storage, (uint16_t)sizeof(rx_storage));
	fifo_init(&tx_fifo, tx_storage, (uint16_t)sizeof(tx_storage));
	tx_busy = false;
	interrupts_enabled = false;
	usart_drv_init(USART_DRV_POLLING, 57600);
	uart_set_interrupts(use_interrupts);
}

void uart_set_interrupts(bool enabled)
{
	if (enabled == interrupts_enabled)
		return;

	if (enabled) {
		interrupts_enabled = true;
		usart_drv_set_mode(USART_DRV_INTERRUPTS);
		return;
	}

	/* Очередь передачи доедаем, пока вектор ещё жив.
	 * Иначе последний байт так и останется в массиве: писать DR будет некому. */
	while (tx_busy || !fifo_is_empty(&tx_fifo)) {
		usart_drv_irq_disable();
		if (!tx_busy)
			tx_start_next();
		usart_drv_irq_enable();
	}
	usart_drv_flush();
	usart_drv_set_mode(USART_DRV_POLLING);
	interrupts_enabled = false;
}

bool uart_interrupts_enabled(void)
{
	return interrupts_enabled;
}

bool uart_read_byte(uint8_t *byte)
{
	/* Байты, принятые до перехода в опрос, остаются в очереди. */
	if (fifo_pop(&rx_fifo, byte))
		return true;
	if (interrupts_enabled)
		return false;
	return usart_drv_read(byte) == USART_DRV_OK;
}

void uart_write(const char *data, size_t len)
{
	if (!interrupts_enabled) {
		for (size_t i = 0; i < len; i++)
			usart_drv_write_blocking((uint8_t)data[i]);
		usart_drv_flush();
		return;
	}

	for (size_t i = 0; i < len; i++) {
		/* Место кончилось — ждём, пока прерывание заберёт байт.
		 * Терять ответ консоли нельзя, а строка короче размера очереди. */
		while (!fifo_push(&tx_fifo, (uint8_t)data[i])) {
		}

		/* Кто запускает передачу, решаем при выключенном векторе USART:
		 * иначе обработчик заберёт байт между проверкой tx_busy и стартом. */
		usart_drv_irq_disable();
		if (!tx_busy)
			tx_start_next();
		usart_drv_irq_enable();
	}
}

void uart_print(const char *str)
{
	uart_write(str, strlen(str));
}

void usart_drv_on_rx(uint8_t byte)
{
	/* Очередь полна — байт теряется. RXNEIE при этом не снимается,
	 * следующий байт по-прежнему вызовет прерывание. */
	(void)fifo_push(&rx_fifo, byte);
}

void usart_drv_on_tx_ready(void)
{
	tx_start_next();
}
