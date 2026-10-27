#include "proto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *s)
{
	char *end;

	while (*s == ' ' || *s == '\t' || *s == '\r')
		s++;
	end = s + strlen(s);
	while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
		*--end = '\0';
	return s;
}

static bool parse_set(const char *p, clock_view_t *out)
{
	unsigned vals[7];
	static const char sep[] = {':', ':', ' ', '/', '/', ' '};

	for (int i = 0; i < 7; i++) {
		char *end = NULL;
		unsigned long value;

		if (*p == '\0')
			return false;
		value = strtoul(p, &end, 10);
		if (end == p || value > 99)
			return false;
		vals[i] = (unsigned)value;
		p = end;
		if (i < 6) {
			if (*p != sep[i])
				return false;
			p++;
		}
	}
	if (*p != '\0')
		return false;
	if (!clock_fields_ok(vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6]))
		return false;

	memset(out, 0, sizeof(*out));
	out->hour = (uint8_t)vals[0];
	out->min = (uint8_t)vals[1];
	out->sec = (uint8_t)vals[2];
	out->day = (uint8_t)vals[3];
	out->month = (uint8_t)vals[4];
	out->year = (uint8_t)vals[5];
	out->weekday = (uint8_t)vals[6];
	out->valid = true;
	return true;
}

void proto_parse(const char *line, proto_request_t *request)
{
	char buf[80];
	char *text;

	memset(request, 0, sizeof(*request));
	request->kind = PROTO_BAD;
	if (line == NULL)
		return;

	strncpy(buf, line, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	text = trim(buf);

	if (strcmp(text, "PING") == 0) {
		request->kind = PROTO_PING;
		return;
	}
	if (strcmp(text, "GET") == 0) {
		request->kind = PROTO_GET;
		return;
	}
	if (strncmp(text, "SET ", 4) == 0 && parse_set(text + 4, &request->set_value))
		request->kind = PROTO_SET;
}

int proto_format_time(char *dst, size_t n, const clock_view_t *view)
{
	char time_s[9];
	char date_s[9];
	unsigned weekday = view->valid ? view->weekday : 0;

	clock_format_time(time_s, sizeof(time_s), view);
	clock_format_date(date_s, sizeof(date_s), view);
	return snprintf(dst, n, "TIME %s %s %u %s\n",
			time_s, date_s, weekday, clock_state_name(view->state));
}

int proto_format_pong(char *dst, size_t n)
{
	return snprintf(dst, n, "PONG\n");
}

int proto_format_ok(char *dst, size_t n)
{
	return snprintf(dst, n, "OK\n");
}

int proto_format_err(char *dst, size_t n)
{
	return snprintf(dst, n, "ERR\n");
}
