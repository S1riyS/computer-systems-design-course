#ifndef CLOCK_H
#define CLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Состояние часов MCP79411, которое видят и дисплей, и клиент. */
typedef enum {
	CLOCK_STOP = 0,
	CLOCK_RUN,
	CLOCK_ERR
} clock_state_t;

/* weekday: 1 — понедельник … 7 — воскресенье. Чипу день недели задаёт пользователь. */
typedef struct {
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t day;
	uint8_t month;
	uint8_t year;
	uint8_t weekday;
	clock_state_t state;
	bool valid;
} clock_view_t;

void clock_view_invalid(clock_view_t *view);
bool clock_fields_ok(unsigned hour, unsigned min, unsigned sec,
		     unsigned day, unsigned month, unsigned year,
		     unsigned weekday);
bool clock_view_same(const clock_view_t *a, const clock_view_t *b);

/* Короткие имена для протокола: RUN, STOP, ERR. */
const char *clock_state_name(clock_state_t state);
/* Слова для дисплея: RUNNING, STOPPED, ERROR. */
const char *clock_state_label(clock_state_t state);
/* MON … SUN, либо "---" если дня нет. */
const char *clock_weekday_name(unsigned weekday);

void clock_format_time(char *dst, size_t n, const clock_view_t *view);
void clock_format_date(char *dst, size_t n, const clock_view_t *view);

#endif
