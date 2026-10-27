#include "clock.h"

#include <stdio.h>
#include <string.h>

void clock_view_invalid(clock_view_t *view)
{
	memset(view, 0, sizeof(*view));
	view->state = CLOCK_ERR;
	view->valid = false;
}

static unsigned days_in_month(unsigned month, unsigned year)
{
	static const unsigned days[] = {
		0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};

	if (month < 1 || month > 12)
		return 0;
	if (month == 2 && (year % 4) == 0)
		return 29;
	return days[month];
}

bool clock_fields_ok(unsigned hour, unsigned min, unsigned sec,
		     unsigned day, unsigned month, unsigned year,
		     unsigned weekday)
{
	if (hour > 23 || min > 59 || sec > 59)
		return false;
	if (weekday < 1 || weekday > 7)
		return false;
	if (year > 99)
		return false;
	if (day < 1 || day > days_in_month(month, year))
		return false;
	return true;
}

bool clock_view_same(const clock_view_t *a, const clock_view_t *b)
{
	return a->hour == b->hour && a->min == b->min && a->sec == b->sec &&
	       a->day == b->day && a->month == b->month && a->year == b->year &&
	       a->weekday == b->weekday && a->state == b->state &&
	       a->valid == b->valid;
}

const char *clock_state_name(clock_state_t state)
{
	switch (state) {
	case CLOCK_RUN:
		return "RUN";
	case CLOCK_STOP:
		return "STOP";
	default:
		return "ERR";
	}
}

const char *clock_state_label(clock_state_t state)
{
	switch (state) {
	case CLOCK_RUN:
		return "RUNNING";
	case CLOCK_STOP:
		return "STOPPED";
	default:
		return "ERROR";
	}
}

const char *clock_weekday_name(unsigned weekday)
{
	static const char *names[] = {
		"---", "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"
	};

	if (weekday > 7)
		return names[0];
	return names[weekday];
}

void clock_format_time(char *dst, size_t n, const clock_view_t *view)
{
	if (!view->valid) {
		snprintf(dst, n, "--:--:--");
		return;
	}
	snprintf(dst, n, "%02u:%02u:%02u",
		 (unsigned)view->hour, (unsigned)view->min, (unsigned)view->sec);
}

void clock_format_date(char *dst, size_t n, const clock_view_t *view)
{
	if (!view->valid) {
		snprintf(dst, n, "--/--/--");
		return;
	}
	snprintf(dst, n, "%02u/%02u/%02u",
		 (unsigned)view->day, (unsigned)view->month, (unsigned)view->year);
}
