#ifndef USART_DRV_H
#define USART_DRV_H

#include <stdbool.h>
#include <stdint.h>

/* Отладочный USART6, тактовая частота APB2. Можно переопределить при сборке. */
#ifndef USART_DRV_PCLK2_HZ
#define USART_DRV_PCLK2_HZ 90000000UL
#endif

typedef enum {
	USART_DRV_POLLING = 0,
	USART_DRV_INTERRUPTS = 1
} usart_drv_mode_t;

typedef enum {
	USART_DRV_OK = 0,
	USART_DRV_EMPTY,
	USART_DRV_BUSY
} usart_drv_status_t;

void usart_drv_init(usart_drv_mode_t mode, uint32_t baudrate);
void usart_drv_set_mode(usart_drv_mode_t mode);

/* Приём не ждёт байт: EMPTY, если в DR ничего нет. */
usart_drv_status_t usart_drv_read(uint8_t *byte);
/* Запись одного байта, только если TXE уже взведён. */
usart_drv_status_t usart_drv_write(uint8_t byte);
void usart_drv_write_blocking(uint8_t byte);
/* Ждёт, пока сдвиговый регистр передатчика опустеет (флаг TC). */
void usart_drv_flush(void);
bool usart_drv_tx_ready(void);

void usart_drv_enable_tx_irq(bool enable);
void usart_drv_irq_disable(void);
void usart_drv_irq_enable(void);

void usart_drv_irq_handler(void);

/* Реализует верхний слой (uart.c). Драйвер про очередь не знает. */
void usart_drv_on_rx(uint8_t byte);
void usart_drv_on_tx_ready(void);

#endif
