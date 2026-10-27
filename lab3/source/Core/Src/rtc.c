#include "rtc.h"

#include "i2c.h"
#include "main.h"

#define RTC_ADDR 0xDE

#define REG_SEC 0x00
#define REG_MIN 0x01

#define SEC_ST 0x80u
#define HOUR_12H 0x40u
#define WKDAY_OSCRUN 0x20u
#define WKDAY_VBATEN 0x08u

/* После установки времени осциллятору нужна доля секунды, чтобы поднять OSCRUN. */
#define STARTUP_GRACE_MS 2000u

static uint32_t startup_tick;
static bool startup_pending;

static uint8_t bin_to_bcd(uint8_t value)
{
	return (uint8_t)(((value / 10) << 4) | (value % 10));
}

static bool bcd_to_bin(uint8_t value, uint8_t *out)
{
	uint8_t tens = (uint8_t)(value >> 4);
	uint8_t ones = (uint8_t)(value & 0x0F);

	if (tens > 9 || ones > 9)
		return false;
	*out = (uint8_t)(tens * 10 + ones);
	return true;
}

static bool decode(const uint8_t raw[7], clock_view_t *view)
{
	uint8_t sec;
	uint8_t min;
	uint8_t hour;
	uint8_t day;
	uint8_t month;
	uint8_t year;
	uint8_t weekday = (uint8_t)(raw[3] & 0x07);

	if (!bcd_to_bin((uint8_t)(raw[0] & 0x7F), &sec) || sec > 59)
		return false;
	if (!bcd_to_bin((uint8_t)(raw[1] & 0x7F), &min) || min > 59)
		return false;
	if ((raw[2] & HOUR_12H) != 0)
		return false;
	if (!bcd_to_bin((uint8_t)(raw[2] & 0x3F), &hour) || hour > 23)
		return false;
	if (!bcd_to_bin((uint8_t)(raw[4] & 0x3F), &day))
		return false;
	if (!bcd_to_bin((uint8_t)(raw[5] & 0x1F), &month))
		return false;
	if (!bcd_to_bin(raw[6], &year))
		return false;
	if (!clock_fields_ok(hour, min, sec, day, month, year, weekday))
		return false;

	view->hour = hour;
	view->min = min;
	view->sec = sec;
	view->day = day;
	view->month = month;
	view->year = year;
	view->weekday = weekday;
	view->valid = true;

	if ((raw[0] & SEC_ST) == 0) {
		view->state = CLOCK_STOP;
		startup_pending = false;
	} else if ((raw[3] & WKDAY_OSCRUN) != 0) {
		view->state = CLOCK_RUN;
		startup_pending = false;
	} else if (startup_pending && (HAL_GetTick() - startup_tick) < STARTUP_GRACE_MS) {
		view->state = CLOCK_RUN;
	} else {
		view->state = CLOCK_ERR;
	}
	return true;
}

void rtc_read(clock_view_t *view)
{
	uint8_t raw[7];
	uint8_t second_again = 0;

	clock_view_invalid(view);
	/* Секунда могла переключиться посреди чтения семи регистров.
	 * Если повторное чтение секунды не совпало, берём набор ещё раз. */
	for (int attempt = 0; attempt < 3; attempt++) {
		if (!i2c_mem_read(RTC_ADDR, REG_SEC, raw, sizeof(raw)))
			return;
		if (!i2c_mem_read(RTC_ADDR, REG_SEC, &second_again, 1))
			return;
		if (second_again == raw[0])
			break;
	}
	if (!decode(raw, view))
		clock_view_invalid(view);
}

bool rtc_set(const clock_view_t *view)
{
	uint8_t second;
	uint8_t block[6];
	bool was_running;

	if (view == NULL || !view->valid ||
	    !clock_fields_ok(view->hour, view->min, view->sec, view->day,
			     view->month, view->year, view->weekday))
		return false;

	/* Пока ST = 1, регистры могут измениться прямо во время записи. */
	if (!i2c_mem_read(RTC_ADDR, REG_SEC, &second, 1))
		return false;
	was_running = (second & SEC_ST) != 0;
	second &= (uint8_t)~SEC_ST;
	if (!i2c_mem_write(RTC_ADDR, REG_SEC, &second, 1))
		return false;

	block[0] = bin_to_bcd(view->min);
	block[1] = bin_to_bcd(view->hour);
	block[2] = (uint8_t)(WKDAY_VBATEN | view->weekday);
	block[3] = bin_to_bcd(view->day);
	block[4] = bin_to_bcd(view->month);
	block[5] = bin_to_bcd(view->year);
	if (!i2c_mem_write(RTC_ADDR, REG_MIN, block, sizeof(block))) {
		if (was_running) {
			second |= SEC_ST;
			(void)i2c_mem_write(RTC_ADDR, REG_SEC, &second, 1);
		}
		return false;
	}

	second = (uint8_t)(bin_to_bcd(view->sec) | SEC_ST);
	if (!i2c_mem_write(RTC_ADDR, REG_SEC, &second, 1))
		return false;

	startup_pending = true;
	startup_tick = HAL_GetTick();
	return true;
}
