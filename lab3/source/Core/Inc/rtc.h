#ifndef RTC_H
#define RTC_H

#include "clock.h"

/* Чтение календаря MCP79411. При ошибке шины или мусоре в регистрах
 * view->valid == false и state == CLOCK_ERR. */
void rtc_read(clock_view_t *view);

/* Запись нового времени. Осциллятор после этого запускается (бит ST). */
bool rtc_set(const clock_view_t *view);

#endif
