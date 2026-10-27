#include "uart.h"
#include "fifo.h"
#include "usart.h"
#include <string.h>

#define UART_HANDLE huart6
#define UART_IRQ    USART6_IRQn

static uint8_t rx_storage[UART_RX_BUFFER_SIZE];
static uint8_t tx_storage[UART_TX_BUFFER_SIZE];
static fifo_t rx_fifo;
static fifo_t tx_fifo;

static volatile bool interrupts_enabled;
static volatile bool tx_busy;
static uint8_t rx_byte;
static uint8_t tx_byte;

static void tx_start_next(void)
{
	if (!fifo_pop(&tx_fifo, &tx_byte)) {
		tx_busy = false;
		return;
	}

	tx_busy = true;
	if (HAL_UART_Transmit_IT(&UART_HANDLE, &tx_byte, 1) != HAL_OK)
		tx_busy = false;
}

static void rx_start(void)
{
	HAL_UART_Receive_IT(&UART_HANDLE, &rx_byte, 1);
}

void uart_init(bool use_interrupts)
{
	fifo_init(&rx_fifo, rx_storage, (uint16_t)sizeof(rx_storage));
	fifo_init(&tx_fifo, tx_storage, (uint16_t)sizeof(tx_storage));
	tx_busy = false;
	interrupts_enabled = false;

	/* CubeMX включает USART6_IRQn в HAL_UART_MspInit ещё до запуска приёма.
	 * Пока режим не выбран, вектор держим выключенным. */
	HAL_NVIC_DisableIRQ(UART_IRQ);
	uart_set_interrupts(use_interrupts);
}

void uart_set_interrupts(bool enabled)
{
	if (enabled == interrupts_enabled)
		return;

	if (enabled) {
		interrupts_enabled = true;
		HAL_NVIC_EnableIRQ(UART_IRQ);
		rx_start();
		return;
	}

	/* Сначала опустошаем очередь передачи: прерывание ещё живо.
	 * Если выключить его посреди HAL_UART_Transmit_IT, gState останется
	 * BUSY_TX, и опросный HAL_UART_Transmit больше не стартует. */
	while (tx_busy) {
	}

	interrupts_enabled = false;
	HAL_NVIC_DisableIRQ(UART_IRQ);
	/* Иначе незавершённый Receive_IT оставляет RxState занятым,
	 * и HAL_UART_Receive в режиме опроса сразу возвращает HAL_BUSY. */
	HAL_UART_AbortReceive(&UART_HANDLE);
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

	HAL_StatusTypeDef status = HAL_UART_Receive(&UART_HANDLE, byte, 1, 0);
	if (status == HAL_OK)
		return true;
	/* Таймаут 0 — это нормальный «байта нет», состояние HAL уже READY.
	 * Любой другой код значит, что приём застрял: сбрасываем его. */
	if (status != HAL_TIMEOUT)
		HAL_UART_AbortReceive(&UART_HANDLE);
	return false;
}

void uart_write(const char *data, size_t len)
{
	if (!interrupts_enabled) {
		HAL_UART_Transmit(&UART_HANDLE, (uint8_t *)data, (uint16_t)len, HAL_MAX_DELAY);
		return;
	}

	for (size_t i = 0; i < len; i++) {
		/* Место кончилось — ждём, пока прерывание заберёт байт.
		 * Терять ответ консоли нельзя, а строка короче размера очереди. */
		while (!fifo_push(&tx_fifo, (uint8_t)data[i])) {
		}

		/* Кто запускает передачу, решаем при выключенном векторе USART:
		 * иначе TxCplt может забрать байт между проверкой tx_busy и стартом. */
		HAL_NVIC_DisableIRQ(UART_IRQ);
		if (!tx_busy)
			tx_start_next();
		HAL_NVIC_EnableIRQ(UART_IRQ);
	}
}

void uart_print(const char *str)
{
	uart_write(str, strlen(str));
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart != &UART_HANDLE)
		return;

	/* Очередь полна — байт теряется, но приём нужно запустить снова,
	 * иначе следующий байт уже никто не примет. */
	(void)fifo_push(&rx_fifo, rx_byte);
	rx_start();
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart == &UART_HANDLE)
		tx_start_next();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	/* ORE и прочие ошибки останавливают Receive_IT. Без повторного
	 * запуска консоль замолкает до перезагрузки. */
	if (huart == &UART_HANDLE && interrupts_enabled)
		rx_start();
}
