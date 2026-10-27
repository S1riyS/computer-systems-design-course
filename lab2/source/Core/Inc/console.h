#ifndef CONSOLE_H
#define CONSOLE_H

/* Команды светофора по отладочному UART. Разбор строки не блокирует цикл:
 * за один вызов забираются только уже принятые байты. */
void console_init(void);
void console_update(void);

#endif
