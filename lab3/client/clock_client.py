#!/usr/bin/env python3
"""Клиент часов SDK-1.1M, лабораторная работа 3, вариант 1.

Запуск:
  python3 clock_client.py /dev/cu.usbserial-XXXX
  python3 clock_client.py COM5

Открывает страницу в браузере: время с платы, поля даты и времени,
список дня недели, кнопки «Установить» и «Установить текущее».
Порт — виртуальный COM UART программатора (у FTDI обычно суффикс B).
Скорость 57600 8N1. Журнал обмена — uart.log в текущем каталоге.
"""

import argparse
import os
import sys

from protocol import (
    DASH_TIME,
    build_set,
    fields_ok,
    iso_date,
    parse_clock_form,
    parse_time,
)
from serial_port import BAUD, SerialPort
from server import run_web


def self_test():
    assert fields_ok(9, 5, 3, 27, 10, 26, 2)
    assert fields_ok(0, 0, 0, 29, 2, 24, 1)
    assert fields_ok(0, 0, 0, 29, 2, 0, 1)
    assert not fields_ok(9, 5, 3, 29, 2, 26, 1)
    assert not fields_ok(9, 5, 3, 31, 4, 26, 1)
    assert not fields_ok(24, 0, 0, 1, 1, 26, 1)
    assert build_set(9, 5, 3, 27, 10, 26, 2) == "SET 09:05:03 27/10/26 2"
    parsed = parse_time("TIME 09:05:03 27/10/26 2 RUN")
    assert parsed == {
        "time": "09:05:03",
        "date": "27/10/26",
        "weekday": 2,
        "state": "RUN",
    }
    assert parse_time(DASH_TIME) is None
    assert parse_time("PONG") is None
    assert parse_clock_form("2026-10-27", "09:05:03", 2) == (9, 5, 3, 27, 10, 26, 2)
    assert parse_clock_form("27/10/26", "9:05", 2) == (9, 5, 0, 27, 10, 26, 2)
    assert parse_clock_form("31/04/26", "00:00:00", 1) is None
    assert iso_date("27/10/26") == "2026-10-27"

    import pty
    master, slave = pty.openpty()
    port = SerialPort(os.ttyname(slave), BAUD)
    try:
        os.write(master, b"PONG\n")
        data = b""
        for _ in range(20):
            data += port.read(32)
            if b"\n" in data:
                break
        assert data == b"PONG\n", data
        port.write(b"PING\n")
        echo = os.read(master, 16)
        assert echo == b"PING\n", echo
    finally:
        port.close()
        os.close(master)
        os.close(slave)
    print("ok")


def main():
    parser = argparse.ArgumentParser(
        description="Окно часов стенда SDK-1.1M. Порт задаётся аргументом.",
    )
    parser.add_argument(
        "port",
        nargs="?",
        help="COM-порт UART, например COM5 или /dev/cu.usbserial-XXXXB",
    )
    parser.add_argument(
        "--log",
        default="uart.log",
        help="файл журнала обмена (по умолчанию uart.log)",
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="проверить календарь, разбор строк и петлю UART без стенда",
    )
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if not args.port:
        parser.error("нужен COM-порт")
    return run_web(args.port, args.log)


if __name__ == "__main__":
    sys.exit(main())
