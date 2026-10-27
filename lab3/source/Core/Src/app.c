#include "app.h"

#include "clock.h"
#include "main.h"
#include "oled.h"
#include "proto.h"
#include "rtc.h"
#include "uart.h"

#include <string.h>

static clock_view_t current;
static clock_view_t drawn;
static bool display_ok;
static uint32_t next_report;
static char rx_line[80];
static size_t rx_len;
static bool rx_overflow;

static void draw_centered(int y, const char *text, uint8_t text_scale)
{
	int width = (int)strlen(text) * 8 * text_scale;
	int x = (GFX_WIDTH - width) / 2;

	if (x < 0)
		x = 0;
	gfx_set_scale(text_scale);
	gfx_set_cursor(x, y);
	gfx_write_string(text);
}

static void draw_current(void)
{
	char time_s[9];
	char date_s[9];
	unsigned weekday = current.valid ? current.weekday : 0;

	clock_format_time(time_s, sizeof(time_s), &current);
	clock_format_date(date_s, sizeof(date_s), &current);
	gfx_fill(0);
	draw_centered(0, time_s, 2);
	draw_centered(22, date_s, 1);
	draw_centered(36, clock_weekday_name(weekday), 1);
	draw_centered(50, clock_state_label(current.state), 1);
}

static void present(void)
{
	if (display_ok && clock_view_same(&current, &drawn))
		return;
	draw_current();
	display_ok = oled_flush();
	if (display_ok)
		drawn = current;
}

static void send_time(void)
{
	char line[64];

	proto_format_time(line, sizeof(line), &current);
	uart_print(line);
}

static void reply(int (*format)(char *, size_t))
{
	char line[16];

	format(line, sizeof(line));
	uart_print(line);
}

static void handle_line(void)
{
	proto_request_t request;

	if (rx_overflow) {
		reply(proto_format_err);
		return;
	}
	if (rx_line[0] == '\0')
		return;

	proto_parse(rx_line, &request);
	switch (request.kind) {
	case PROTO_PING:
		reply(proto_format_pong);
		send_time();
		break;
	case PROTO_GET:
		send_time();
		break;
	case PROTO_SET:
		if (!rtc_set(&request.set_value)) {
			reply(proto_format_err);
			break;
		}
		rtc_read(&current);
		present();
		reply(proto_format_ok);
		send_time();
		break;
	default:
		reply(proto_format_err);
		break;
	}
}

static void read_uart(void)
{
	uint8_t byte;

	while (uart_read_byte(&byte)) {
		if (byte == '\r')
			continue;
		if (byte != '\n') {
			if (rx_len + 1 < sizeof(rx_line))
				rx_line[rx_len++] = (char)byte;
			else
				rx_overflow = true;
			continue;
		}
		rx_line[rx_len] = '\0';
		handle_line();
		rx_len = 0;
		rx_overflow = false;
	}
}

void app_init(void)
{
	uart_init(true);
	oled_init();
	clock_view_invalid(&current);
	clock_view_invalid(&drawn);
	display_ok = false;
	rtc_read(&current);
	present();
	next_report = HAL_GetTick() + 1000;
}

void app_poll(void)
{
	uint32_t now;

	read_uart();
	now = HAL_GetTick();
	if ((int32_t)(now - next_report) < 0)
		return;
	next_report = now + 1000;
	rtc_read(&current);
	present();
	send_time();
}
