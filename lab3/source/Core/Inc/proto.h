#ifndef PROTO_H
#define PROTO_H

#include "clock.h"

/*
 * Текстовый протокол UART, строки заканчиваются '\n'.
 *
 * Компьютер -> стенд:
 *   PING
 *   GET
 *   SET HH:MM:SS DD/MM/YY W
 *     W = 1..7, 1 — понедельник. Год — две цифры.
 *
 * Стенд -> компьютер:
 *   PONG
 *   OK
 *   ERR
 *   TIME HH:MM:SS DD/MM/YY W RUN|STOP|ERR
 *     Если показания недостоверны: TIME --:--:-- --/--/-- 0 ERR
 *
 * Скорость USART6 — 57600 8N1.
 */

typedef enum {
	PROTO_PING = 0,
	PROTO_GET,
	PROTO_SET,
	PROTO_BAD
} proto_kind_t;

typedef struct {
	proto_kind_t kind;
	clock_view_t set_value;
} proto_request_t;

void proto_parse(const char *line, proto_request_t *request);
int proto_format_time(char *dst, size_t n, const clock_view_t *view);
int proto_format_pong(char *dst, size_t n);
int proto_format_ok(char *dst, size_t n);
int proto_format_err(char *dst, size_t n);

#endif
