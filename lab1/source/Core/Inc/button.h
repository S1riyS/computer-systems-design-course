#ifndef BUTTON_H
#define BUTTON_H

typedef enum {
	BUTTON_EVENT_NONE,
	BUTTON_EVENT_PRESS,
} button_event_t;

void button_init(void);

/* Вызывать из главного цикла. Не блокирует.
 * Возвращает PRESS один раз, когда нажатие подтвердилось после антидребезга. */
button_event_t button_poll(void);

#endif
