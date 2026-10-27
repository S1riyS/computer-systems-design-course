#include "fifo.h"

void fifo_init(fifo_t *fifo, uint8_t *storage, uint16_t size)
{
	fifo->data = storage;
	fifo->size = size;
	fifo->head = 0;
	fifo->tail = 0;
}

bool fifo_push(fifo_t *fifo, uint8_t byte)
{
	uint16_t next = (uint16_t)((fifo->head + 1u) % fifo->size);

	if (next == fifo->tail)
		return false;

	/* Сначала байт, потом индекс. Читатель не увидит новую голову,
	 * пока значение ячейки уже не записано. */
	fifo->data[fifo->head] = byte;
	fifo->head = next;
	return true;
}

bool fifo_pop(fifo_t *fifo, uint8_t *byte)
{
	if (fifo_is_empty(fifo))
		return false;

	*byte = fifo->data[fifo->tail];
	fifo->tail = (uint16_t)((fifo->tail + 1u) % fifo->size);
	return true;
}

bool fifo_is_empty(const fifo_t *fifo)
{
	return fifo->head == fifo->tail;
}
