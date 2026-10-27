#include "console.h"
#include "traffic.h"
#include "uart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMD_MAX_LEN    32
#define TIMEOUT_MIN_S  1u
#define TIMEOUT_MAX_S  3600u
#define TIMEOUT_PREFIX "set timeout "

static char line[CMD_MAX_LEN];
static size_t line_len;

static void reply(const char *msg)
{
	uart_print(msg);
	uart_print("\r\n");
}

static void print_status(void)
{
	char buf[64];

	/* Протокол варианта 1: цвет, режим кнопки, красный в секундах, I или P. */
	snprintf(buf, sizeof(buf), "%s, mode %d, timeout %lu, %c",
	         traffic_light_name(),
	         traffic_button_enabled() ? 1 : 2,
	         (unsigned long)(traffic_red_ms() / 1000u),
	         uart_interrupts_enabled() ? 'I' : 'P');
	reply(buf);
}

static void set_timeout(const char *arg)
{
	char *end = NULL;
	unsigned long seconds = strtoul(arg, &end, 10);

	if (end == arg || *end != '\0' || seconds < TIMEOUT_MIN_S || seconds > TIMEOUT_MAX_S) {
		reply("invalid timeout, expected 1..3600 seconds");
		return;
	}

	traffic_set_red_ms((uint32_t)seconds * 1000u);
	reply("OK");
}

static void execute(const char *cmd)
{
	if (strcmp(cmd, "?") == 0) {
		print_status();
	} else if (strcmp(cmd, "set mode 1") == 0) {
		traffic_set_button_enabled(true);
		reply("OK");
	} else if (strcmp(cmd, "set mode 2") == 0) {
		traffic_set_button_enabled(false);
		reply("OK");
	} else if (strcmp(cmd, "set interrupts on") == 0) {
		uart_set_interrupts(true);
		reply("OK");
	} else if (strcmp(cmd, "set interrupts off") == 0) {
		uart_set_interrupts(false);
		reply("OK");
	} else if (strncmp(cmd, TIMEOUT_PREFIX, strlen(TIMEOUT_PREFIX)) == 0) {
		set_timeout(cmd + strlen(TIMEOUT_PREFIX));
	} else {
		reply("unknown command");
	}
}

static void handle_char(char c)
{
	if (c == '\r' || c == '\n') {
		/* Вторая половина CRLF не должна выполнять пустую команду. */
		if (line_len == 0)
			return;
		uart_print("\r\n");
		line[line_len] = '\0';
		execute(line);
		line_len = 0;
	} else if (c == '\b' || c == 0x7F) {
		if (line_len > 0) {
			line_len--;
			uart_print("\b \b");
		}
	} else if (c >= ' ' && c < 0x7F && line_len < CMD_MAX_LEN - 1) {
		line[line_len++] = c;
		uart_write(&c, 1);
	}
}

void console_init(void)
{
	uart_init(true);
	reply("");
	reply("Traffic light. Commands: ?, set mode 1|2, set timeout X, set interrupts on|off");
}

void console_update(void)
{
	uint8_t byte;

	while (uart_read_byte(&byte))
		handle_char((char)byte);
}
