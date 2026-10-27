#ifndef UART_H
#define UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Отладочный USART6 стенда (PC6 TX, PC7 RX), 57600 8N1.
 * Размеры очередей можно переопределить при сборке через -D. */
#ifndef UART_RX_BUFFER_SIZE
#define UART_RX_BUFFER_SIZE 64
#endif
#ifndef UART_TX_BUFFER_SIZE
#define UART_TX_BUFFER_SIZE 256
#endif

void uart_init(bool use_interrupts);

/* Переключение на лету. В режиме прерываний обмен идёт через очереди
 * и не блокирует цикл, пока в очереди передачи есть место.
 * В режиме опроса прерывание USART6 выключено: приём проверяет RXNE
 * один раз и сразу возвращается, передача дожидается конца байтов. */
void uart_set_interrupts(bool enabled);
bool uart_interrupts_enabled(void);

/* false — принятого байта сейчас нет. */
bool uart_read_byte(uint8_t *byte);

void uart_write(const char *data, size_t len);
void uart_print(const char *str);

#endif
