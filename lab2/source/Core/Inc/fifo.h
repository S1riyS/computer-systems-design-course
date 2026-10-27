#ifndef FIFO_H
#define FIFO_H

#include <stdbool.h>
#include <stdint.h>

/* Кольцевой буфер на одного писателя и одного читателя.
 * Писатель — либо обработчик USART, либо главный цикл, но не оба сразу
 * для одной и той же очереди. Вмещает size - 1 байт: одно состояние
 * индексов оставлено, чтобы отличить «пусто» от «полно». */
typedef struct {
	volatile uint8_t *data;
	uint16_t size;
	volatile uint16_t head; /* индекс следующей записи */
	volatile uint16_t tail; /* индекс следующего чтения */
} fifo_t;

void fifo_init(fifo_t *fifo, uint8_t *storage, uint16_t size);
bool fifo_push(fifo_t *fifo, uint8_t byte); /* false — буфер полон */
bool fifo_pop(fifo_t *fifo, uint8_t *byte); /* false — буфер пуст */
bool fifo_is_empty(const fifo_t *fifo);

#endif
