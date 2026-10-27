/* Проверка очереди на компьютере:
 * cc -I../Core/Inc test_fifo.c ../Core/Src/fifo.c && ./a.out */
#include "fifo.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
	uint8_t storage[4];
	fifo_t fifo;
	uint8_t byte = 0;

	fifo_init(&fifo, storage, sizeof(storage));
	assert(fifo_is_empty(&fifo));
	assert(!fifo_pop(&fifo, &byte));

	assert(fifo_push(&fifo, 1));
	assert(fifo_push(&fifo, 2));
	assert(fifo_push(&fifo, 3));
	assert(!fifo_push(&fifo, 4));

	for (uint8_t round = 0; round < 10; round++) {
		assert(fifo_pop(&fifo, &byte) && byte == (uint8_t)(round + 1));
		assert(fifo_push(&fifo, (uint8_t)(round + 4)));
	}

	assert(fifo_pop(&fifo, &byte) && byte == 11);
	assert(fifo_pop(&fifo, &byte) && byte == 12);
	assert(fifo_pop(&fifo, &byte) && byte == 13);
	assert(fifo_is_empty(&fifo));

	puts("fifo OK");
	return 0;
}
