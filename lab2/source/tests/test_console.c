/* Разбор команд без микроконтроллера: подменяются UART и светофор.
 * cc -I../Core/Inc test_console.c ../Core/Src/console.c && ./a.out */
#include "console.h"
#include "traffic.h"
#include "uart.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char tx[2048];
static size_t tx_len;
static const char *script;
static size_t script_pos;
static bool interrupts = true;
static bool button_enabled = true;
static uint32_t red_ms = 12000;
static const char *light = "red";

void uart_init(bool use_interrupts)
{
	interrupts = use_interrupts;
}

void uart_set_interrupts(bool enabled)
{
	interrupts = enabled;
}

bool uart_interrupts_enabled(void)
{
	return interrupts;
}

bool uart_read_byte(uint8_t *byte)
{
	if (script[script_pos] == '\0')
		return false;
	*byte = (uint8_t)script[script_pos++];
	return true;
}

void uart_write(const char *data, size_t len)
{
	assert(tx_len + len < sizeof(tx));
	memcpy(tx + tx_len, data, len);
	tx_len += len;
	tx[tx_len] = '\0';
}

void uart_print(const char *str)
{
	uart_write(str, strlen(str));
}

void traffic_set_button_enabled(bool enabled)
{
	button_enabled = enabled;
}

bool traffic_button_enabled(void)
{
	return button_enabled;
}

void traffic_set_red_ms(uint32_t ms)
{
	red_ms = ms;
}

uint32_t traffic_red_ms(void)
{
	return red_ms;
}

const char *traffic_light_name(void)
{
	return light;
}

static size_t mark;

static void feed(const char *input)
{
	mark = tx_len;
	script = input;
	script_pos = 0;
	console_update();
}

static const char *fresh(void)
{
	return tx + mark;
}

int main(void)
{
	script = "";
	console_init();
	assert(strstr(tx, "set timeout X") != NULL);

	feed("?\r\n");
	assert(strstr(fresh(), "red, mode 1, timeout 12, I\r\n") != NULL);

	feed("set mode 2\r");
	assert(strstr(fresh(), "OK\r\n") != NULL);
	assert(!button_enabled);

	feed("?\n");
	assert(strstr(fresh(), "red, mode 2, timeout 12, I\r\n") != NULL);

	feed("set mode 1\r");
	assert(button_enabled);

	feed("set timeout 5\r");
	assert(strstr(fresh(), "OK\r\n") != NULL);
	assert(red_ms == 5000);

	feed("?\r");
	assert(strstr(fresh(), "timeout 5, I\r\n") != NULL);

	feed("set timeout 0\r");
	assert(strstr(fresh(), "invalid timeout") != NULL);
	assert(red_ms == 5000);

	feed("set timeout 3601\r");
	assert(red_ms == 5000);

	feed("set timeout 3600\r");
	assert(red_ms == 3600000);

	feed("set timeout 12abc\r");
	assert(red_ms == 3600000);

	feed("seX\bt mode 2\r");
	assert(!button_enabled);

	feed("set interrupts off\r");
	assert(strstr(fresh(), "OK\r\n") != NULL);
	assert(!interrupts);

	feed("?\r");
	assert(strstr(fresh(), ", P\r\n") != NULL);

	feed("set interrupts on\r");
	assert(interrupts);

	feed("nope\r");
	assert(strstr(fresh(), "unknown command\r\n") != NULL);

	feed("\r\n");
	assert(strcmp(fresh(), "") == 0);

	puts("console OK");
	return 0;
}
