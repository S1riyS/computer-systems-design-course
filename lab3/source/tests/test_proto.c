#include "clock.h"
#include "gfx.h"
#include "proto.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(int condition, const char *file, int line, const char *text)
{
	if (condition)
		return;
	fprintf(stderr, "fail %s:%d %s\n", file, line, text);
	failures++;
}

#define EXPECT(condition) expect((condition), __FILE__, __LINE__, #condition)

static clock_view_t sample(void)
{
	clock_view_t view;

	memset(&view, 0, sizeof(view));
	view.hour = 9;
	view.min = 5;
	view.sec = 3;
	view.day = 27;
	view.month = 10;
	view.year = 26;
	view.weekday = 2;
	view.state = CLOCK_RUN;
	view.valid = true;
	return view;
}

static void test_calendar(void)
{
	EXPECT(clock_fields_ok(0, 0, 0, 1, 1, 0, 1));
	EXPECT(clock_fields_ok(23, 59, 59, 29, 2, 24, 7));
	EXPECT(clock_fields_ok(12, 0, 0, 29, 2, 0, 1));
	EXPECT(!clock_fields_ok(24, 0, 0, 1, 1, 26, 1));
	EXPECT(!clock_fields_ok(12, 60, 0, 1, 1, 26, 1));
	EXPECT(!clock_fields_ok(12, 0, 0, 31, 4, 26, 1));
	EXPECT(!clock_fields_ok(12, 0, 0, 29, 2, 26, 1));
	EXPECT(!clock_fields_ok(12, 0, 0, 0, 1, 26, 1));
	EXPECT(!clock_fields_ok(12, 0, 0, 1, 1, 26, 0));
	EXPECT(!clock_fields_ok(12, 0, 0, 1, 1, 100, 1));
	EXPECT(strcmp(clock_weekday_name(2), "TUE") == 0);
	EXPECT(strcmp(clock_weekday_name(0), "---") == 0);
	EXPECT(strcmp(clock_state_name(CLOCK_STOP), "STOP") == 0);
	EXPECT(strcmp(clock_state_label(CLOCK_ERR), "ERROR") == 0);
}

static void test_protocol(void)
{
	proto_request_t request;
	clock_view_t view = sample();
	char line[64];

	proto_parse("PING", &request);
	EXPECT(request.kind == PROTO_PING);
	proto_parse("  GET\r", &request);
	EXPECT(request.kind == PROTO_GET);
	proto_parse("SET 09:05:03 27/10/26 2", &request);
	EXPECT(request.kind == PROTO_SET);
	EXPECT(request.set_value.valid);
	EXPECT(request.set_value.hour == view.hour);
	EXPECT(request.set_value.min == view.min);
	EXPECT(request.set_value.sec == view.sec);
	EXPECT(request.set_value.day == view.day);
	EXPECT(request.set_value.month == view.month);
	EXPECT(request.set_value.year == view.year);
	EXPECT(request.set_value.weekday == view.weekday);
	proto_parse("SET 9:5:3 27/10/26 2", &request);
	EXPECT(request.kind == PROTO_SET);
	EXPECT(request.set_value.hour == 9 && request.set_value.min == 5);

	proto_parse("SET 09:05:03 29/02/26 1", &request);
	EXPECT(request.kind == PROTO_BAD);
	proto_parse("SET 09:05:03 29/02/24 1", &request);
	EXPECT(request.kind == PROTO_SET);
	proto_parse("SET 09:05:03 27/10/26 2 extra", &request);
	EXPECT(request.kind == PROTO_BAD);
	proto_parse("NOPE", &request);
	EXPECT(request.kind == PROTO_BAD);
	proto_parse("", &request);
	EXPECT(request.kind == PROTO_BAD);

	proto_format_time(line, sizeof(line), &view);
	EXPECT(strcmp(line, "TIME 09:05:03 27/10/26 2 RUN\n") == 0);
	view.state = CLOCK_STOP;
	proto_format_time(line, sizeof(line), &view);
	EXPECT(strcmp(line, "TIME 09:05:03 27/10/26 2 STOP\n") == 0);
	clock_view_invalid(&view);
	proto_format_time(line, sizeof(line), &view);
	EXPECT(strcmp(line, "TIME --:--:-- --/--/-- 0 ERR\n") == 0);
	proto_format_pong(line, sizeof(line));
	EXPECT(strcmp(line, "PONG\n") == 0);
	proto_format_ok(line, sizeof(line));
	EXPECT(strcmp(line, "OK\n") == 0);
	proto_format_err(line, sizeof(line));
	EXPECT(strcmp(line, "ERR\n") == 0);
}

static void test_glyph(void)
{
	const uint8_t *fb;

	gfx_fill(0);
	gfx_set_scale(1);
	gfx_set_cursor(0, 0);
	gfx_write_string("0");
	fb = gfx_framebuffer();
	/* Верхняя строка нуля — 0x3E: пиксель x=1 включён, x=0 выключен. */
	EXPECT((fb[0] & 1u) == 0);
	EXPECT((fb[1] & 1u) != 0);
	gfx_fill(0);
	gfx_set_scale(2);
	gfx_set_cursor(0, 0);
	gfx_write_char('0');
	fb = gfx_framebuffer();
	EXPECT((fb[2] & 1u) != 0);
	EXPECT((fb[3] & 1u) != 0);
}

int main(void)
{
	test_calendar();
	test_protocol();
	test_glyph();
	if (failures != 0) {
		fprintf(stderr, "%d checks failed\n", failures);
		return 1;
	}
	puts("ok");
	return 0;
}
