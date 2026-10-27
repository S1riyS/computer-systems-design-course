#!/usr/bin/env python3
"""Клиент часов SDK-1.1M, лабораторная работа 3, вариант 1.

Запуск:
  python3 clock_client.py /dev/cu.usbserial-XXXX
  python3 clock_client.py COM5

На macOS, если python3 собран без tkinter, скрипт сам перезапустится
через /usr/bin/python3. Порт — виртуальный COM UART программатора
(у FTDI обычно тот, что с суффиксом B). Скорость 57600 8N1.
Журнал обмена пишется в uart.log в текущем каталоге.
"""

import argparse
import os
import queue
import re
import sys
import threading
from datetime import datetime

BAUD = 57600
WEEKDAYS = (
    (1, "понедельник"),
    (2, "вторник"),
    (3, "среда"),
    (4, "четверг"),
    (5, "пятница"),
    (6, "суббота"),
    (7, "воскресенье"),
)
STATE_TEXT = {
    "RUN": "идут",
    "STOP": "остановлены",
    "ERR": "ошибка",
}
TIME_RE = re.compile(
    r"TIME (\d{2}):(\d{2}):(\d{2}) (\d{2})/(\d{2})/(\d{2}) (\d) (RUN|STOP|ERR)$"
)
DASH_TIME = "TIME --:--:-- --/--/-- 0 ERR"


def days_in_month(month, year):
    days = (0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31)
    if not 1 <= month <= 12:
        return 0
    if month == 2 and year % 4 == 0:
        return 29
    return days[month]


def fields_ok(hour, minute, second, day, month, year, weekday):
    if not (0 <= hour <= 23 and 0 <= minute <= 59 and 0 <= second <= 59):
        return False
    if not (1 <= weekday <= 7 and 0 <= year <= 99):
        return False
    return 1 <= day <= days_in_month(month, year)


def build_set(hour, minute, second, day, month, year, weekday):
    return "SET %02d:%02d:%02d %02d/%02d/%02d %d" % (
        hour, minute, second, day, month, year, weekday,
    )


def parse_time(line):
    if line == DASH_TIME:
        return None
    match = TIME_RE.match(line)
    if match is None:
        return None
    hour, minute, second, day, month, year, weekday, state = match.groups()
    return {
        "time": "%s:%s:%s" % (hour, minute, second),
        "date": "%s/%s/%s" % (day, month, year),
        "weekday": int(weekday),
        "state": state,
    }


class SerialPort:
    """Минимальный COM-порт: termios на macOS/Linux и Win32 API на Windows."""

    def __init__(self, path, baud=BAUD):
        self._fd = None
        self._handle = None
        self._kernel32 = None
        self._lock = threading.Lock()
        if os.name == "nt":
            self._open_windows(path, baud)
        else:
            self._open_posix(path, baud)

    def _open_posix(self, path, baud):
        import fcntl
        import termios

        speed = getattr(termios, "B%d" % baud, None)
        if speed is None:
            raise OSError("скорость %d не поддерживается" % baud)
        fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        try:
            attrs = termios.tcgetattr(fd)
            cflag = attrs[2] & ~termios.CSIZE
            cflag |= termios.CS8 | termios.CLOCAL | termios.CREAD
            for name in ("PARENB", "PARODD", "CSTOPB", "CRTSCTS", "HUPCL"):
                cflag &= ~getattr(termios, name, 0)
            attrs[0] = 0
            attrs[1] = 0
            attrs[2] = cflag
            attrs[3] = 0
            attrs[4] = speed
            attrs[5] = speed
            attrs[6][termios.VMIN] = 0
            attrs[6][termios.VTIME] = 0
            termios.tcsetattr(fd, termios.TCSANOW, attrs)
            termios.tcflush(fd, termios.TCIOFLUSH)
            flags = fcntl.fcntl(fd, fcntl.F_GETFL)
            fcntl.fcntl(fd, fcntl.F_SETFL, flags & ~os.O_NONBLOCK)
        except Exception:
            os.close(fd)
            raise
        self._fd = fd

    def _open_windows(self, path, baud):
        import ctypes
        from ctypes import wintypes

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        name = path
        suffix = path[3:] if path.upper().startswith("COM") else ""
        if suffix.isdigit() and int(suffix) >= 10:
            name = "\\\\.\\" + path

        class DCB(ctypes.Structure):
            _fields_ = [
                ("DCBlength", wintypes.DWORD),
                ("BaudRate", wintypes.DWORD),
                ("flags", wintypes.DWORD),
                ("wReserved", wintypes.WORD),
                ("XonLim", wintypes.WORD),
                ("XoffLim", wintypes.WORD),
                ("ByteSize", wintypes.BYTE),
                ("Parity", wintypes.BYTE),
                ("StopBits", wintypes.BYTE),
                ("XonChar", ctypes.c_char),
                ("XoffChar", ctypes.c_char),
                ("ErrorChar", ctypes.c_char),
                ("EofChar", ctypes.c_char),
                ("EvtChar", ctypes.c_char),
                ("wReserved1", wintypes.WORD),
            ]

        class COMMTIMEOUTS(ctypes.Structure):
            _fields_ = [
                ("ReadIntervalTimeout", wintypes.DWORD),
                ("ReadTotalTimeoutMultiplier", wintypes.DWORD),
                ("ReadTotalTimeoutConstant", wintypes.DWORD),
                ("WriteTotalTimeoutMultiplier", wintypes.DWORD),
                ("WriteTotalTimeoutConstant", wintypes.DWORD),
            ]

        kernel32.CreateFileW.argtypes = [
            wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
            wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
        ]
        kernel32.CreateFileW.restype = ctypes.c_void_p
        kernel32.GetCommState.argtypes = [ctypes.c_void_p, ctypes.POINTER(DCB)]
        kernel32.GetCommState.restype = wintypes.BOOL
        kernel32.SetCommState.argtypes = [ctypes.c_void_p, ctypes.POINTER(DCB)]
        kernel32.SetCommState.restype = wintypes.BOOL
        kernel32.SetCommTimeouts.argtypes = [ctypes.c_void_p, ctypes.POINTER(COMMTIMEOUTS)]
        kernel32.SetCommTimeouts.restype = wintypes.BOOL
        kernel32.PurgeComm.argtypes = [ctypes.c_void_p, wintypes.DWORD]
        kernel32.PurgeComm.restype = wintypes.BOOL
        kernel32.ReadFile.argtypes = [
            ctypes.c_void_p, ctypes.c_void_p, wintypes.DWORD,
            ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p,
        ]
        kernel32.ReadFile.restype = wintypes.BOOL
        kernel32.WriteFile.argtypes = [
            ctypes.c_void_p, ctypes.c_void_p, wintypes.DWORD,
            ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p,
        ]
        kernel32.WriteFile.restype = wintypes.BOOL
        kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
        kernel32.CloseHandle.restype = wintypes.BOOL

        handle = kernel32.CreateFileW(
            name, 0x80000000 | 0x40000000, 0, None, 3, 0, None,
        )
        if not handle or handle == ctypes.c_void_p(-1).value:
            raise OSError(ctypes.get_last_error(), "не удалось открыть %s" % path)

        dcb = DCB()
        dcb.DCBlength = ctypes.sizeof(DCB)
        if not kernel32.GetCommState(handle, ctypes.byref(dcb)):
            kernel32.CloseHandle(handle)
            raise OSError(ctypes.get_last_error(), "GetCommState")
        dcb.BaudRate = baud
        dcb.ByteSize = 8
        dcb.Parity = 0
        dcb.StopBits = 0
        # fBinary, DTR enable, RTS enable, без аппаратного и программного потока.
        dcb.flags = 1 | (1 << 4) | (1 << 12)
        if not kernel32.SetCommState(handle, ctypes.byref(dcb)):
            kernel32.CloseHandle(handle)
            raise OSError(ctypes.get_last_error(), "SetCommState")
        timeouts = COMMTIMEOUTS(50, 0, 50, 0, 200)
        if not kernel32.SetCommTimeouts(handle, ctypes.byref(timeouts)):
            kernel32.CloseHandle(handle)
            raise OSError(ctypes.get_last_error(), "SetCommTimeouts")
        kernel32.PurgeComm(handle, 0x000F)
        self._kernel32 = kernel32
        self._handle = handle

    def read(self, size, timeout=0.05):
        with self._lock:
            if self._fd is not None:
                import select
                ready, _, _ = select.select([self._fd], [], [], timeout)
                if not ready:
                    return b""
                return os.read(self._fd, size)
            if self._handle is None:
                raise OSError("порт уже закрыт")

            import ctypes
            from ctypes import wintypes

            buffer = ctypes.create_string_buffer(size)
            got = wintypes.DWORD(0)
            ok = self._kernel32.ReadFile(
                self._handle, buffer, size, ctypes.byref(got), None,
            )
            if not ok:
                raise OSError(ctypes.get_last_error(), "ReadFile")
            return buffer.raw[:got.value]

    def write(self, data):
        with self._lock:
            if self._fd is not None:
                view = memoryview(data)
                while view:
                    written = os.write(self._fd, view)
                    view = view[written:]
                return
            if self._handle is None:
                raise OSError("порт уже закрыт")

            import ctypes
            from ctypes import wintypes

            buffer = ctypes.create_string_buffer(bytes(data))
            got = wintypes.DWORD(0)
            ok = self._kernel32.WriteFile(
                self._handle, buffer, len(data), ctypes.byref(got), None,
            )
            if not ok or got.value != len(data):
                raise OSError(ctypes.get_last_error(), "WriteFile")

    def close(self):
        with self._lock:
            fd = self._fd
            self._fd = None
            if fd is not None:
                os.close(fd)
            handle = self._handle
            self._handle = None
            if handle is not None and self._kernel32 is not None:
                self._kernel32.CloseHandle(handle)


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


def ensure_tk():
    try:
        import tkinter
    except ModuleNotFoundError:
        tkinter = None
    if tkinter is not None:
        return tkinter
    fallback = "/usr/bin/python3"
    if os.path.exists(fallback) and os.path.realpath(sys.executable) != os.path.realpath(fallback):
        os.execv(fallback, [fallback, *sys.argv])
    sys.stderr.write(
        "Нужен Python с модулем tkinter.\n"
        "На macOS: /usr/bin/python3 clock_client.py <порт>\n"
    )
    return None


def run_gui(port_name, log_path):
    tk = ensure_tk()
    if tk is None:
        return 1
    from tkinter import messagebox

    try:
        serial = SerialPort(port_name, BAUD)
    except OSError as exc:
        sys.stderr.write("Не удалось открыть %s: %s\n" % (port_name, exc))
        return 1

    log_file = open(log_path, "a", encoding="utf-8")
    log_lock = threading.Lock()

    def log_message(direction, text):
        stamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        with log_lock:
            log_file.write("%s %s %s\n" % (stamp, direction, text))
            log_file.flush()

    log_message("NOTE", "открыт %s, %d 8N1" % (port_name, BAUD))
    print("Журнал UART: %s" % os.path.abspath(log_path))

    root = tk.Tk()
    root.title("Часы SDK-1.1M")
    root.geometry("440x560")
    root.minsize(400, 520)

    messages = queue.Queue()
    alive = {"run": True}
    got_pong = {"ok": False}
    weekday_by_code = dict(WEEKDAYS)

    link = tk.StringVar(value="Открыт %s. Жду PONG…" % port_name)
    time_text = tk.StringVar(value="--:--:--")
    date_text = tk.StringVar(value="--/--/--")
    weekday_text = tk.StringVar(value="—")
    state_text = tk.StringVar(value="нет данных")
    result_text = tk.StringVar(value="")

    header = tk.Frame(root, padx=16, pady=12)
    header.pack(fill="x")
    tk.Label(header, textvariable=link, anchor="w").pack(fill="x")

    clock_box = tk.Frame(root, padx=16)
    clock_box.pack(fill="x")
    tk.Label(clock_box, textvariable=time_text, font=("Helvetica", 36, "bold")).pack(anchor="w")
    tk.Label(clock_box, textvariable=date_text, font=("Helvetica", 18)).pack(anchor="w", pady=(4, 0))
    tk.Label(clock_box, textvariable=weekday_text, font=("Helvetica", 16)).pack(anchor="w")
    tk.Label(clock_box, textvariable=state_text, font=("Helvetica", 16)).pack(anchor="w", pady=(0, 8))

    form = tk.LabelFrame(root, text="Установить время", padx=12, pady=8)
    form.pack(fill="x", padx=16, pady=8)
    labels = ("Часы", "Минуты", "Секунды", "День", "Месяц", "Год")
    defaults = ("12", "00", "00", "01", "01", "26")
    entries = []
    for row, (caption, default) in enumerate(zip(labels, defaults)):
        tk.Label(form, text=caption, width=10, anchor="w").grid(row=row, column=0, sticky="w", pady=2)
        entry = tk.Entry(form, width=8)
        entry.insert(0, default)
        entry.grid(row=row, column=1, sticky="w")
        entries.append(entry)
    tk.Label(form, text="День недели", anchor="w").grid(row=6, column=0, sticky="w", pady=2)
    weekday_var = tk.StringVar(value=WEEKDAYS[0][1])
    weekday_box = tk.OptionMenu(form, weekday_var, *[name for _, name in WEEKDAYS])
    weekday_box.config(width=16)
    weekday_box.grid(row=6, column=1, sticky="w")
    tk.Label(
        form,
        text="Год — две цифры (26 = 2026). День недели задаётся отдельно от даты.",
        wraplength=360,
        justify="left",
        fg="#444444",
    ).grid(row=7, column=0, columnspan=2, sticky="w", pady=(8, 4))

    def selected_weekday():
        for code, name in WEEKDAYS:
            if name == weekday_var.get():
                return code
        return 0

    def send_line(text):
        log_message("TX", text)
        serial.write((text + "\n").encode("ascii"))

    def on_set():
        try:
            numbers = [int(entry.get().strip()) for entry in entries]
        except ValueError:
            messagebox.showerror("Часы", "В полях нужны целые числа.")
            return
        weekday = selected_weekday()
        if not fields_ok(*numbers, weekday):
            messagebox.showerror(
                "Часы",
                "Таких даты или времени не бывает.\n"
                "Часы 0–23, минуты и секунды 0–59, год 0–99.",
            )
            return
        try:
            send_line(build_set(*numbers, weekday))
        except OSError as exc:
            result_text.set("Не удалось отправить: %s" % exc)
            return
        result_text.set("Команда отправлена, жду ответ стенда.")

    tk.Button(form, text="Записать в часы", command=on_set).grid(
        row=8, column=0, columnspan=2, sticky="w", pady=(4, 2),
    )
    tk.Label(root, textvariable=result_text, anchor="w", padx=16).pack(fill="x")
    tk.Label(
        root,
        text="Стенд сам присылает показания раз в секунду. Кнопка только записывает новое время.",
        wraplength=400,
        justify="left",
        fg="#444444",
        padx=16,
    ).pack(fill="x", pady=(8, 0))

    def apply_line(line):
        if line == "PONG":
            got_pong["ok"] = True
            link.set("Стенд ответил на PING. Порт %s." % port_name)
            return
        if line == "OK":
            result_text.set("Стенд принял новое время.")
            return
        if line == "ERR":
            result_text.set("Стенд отклонил команду.")
            return
        parsed = parse_time(line)
        if parsed is None and line.startswith("TIME "):
            time_text.set("--:--:--")
            date_text.set("--/--/--")
            weekday_text.set("—")
            state_text.set(STATE_TEXT["ERR"])
            return
        if parsed is None:
            return
        time_text.set(parsed["time"])
        date_text.set(parsed["date"])
        weekday_text.set(weekday_by_code.get(parsed["weekday"], "—"))
        state_text.set(STATE_TEXT.get(parsed["state"], parsed["state"]))

    def reader():
        pending = b""
        while alive["run"]:
            try:
                chunk = serial.read(128)
            except OSError:
                if alive["run"]:
                    messages.put(("dead", None))
                return
            if not chunk:
                continue
            pending += chunk
            while b"\n" in pending:
                raw, pending = pending.split(b"\n", 1)
                text = raw.decode("ascii", "replace").strip("\r").strip()
                if text:
                    log_message("RX", text)
                    messages.put(("line", text))

    def poll():
        while True:
            try:
                kind, payload = messages.get_nowait()
            except queue.Empty:
                break
            if kind == "line":
                apply_line(payload)
            elif kind == "dead":
                link.set("Связь с портом прервалась.")
        if alive["run"]:
            root.after(100, poll)

    def ping_timeout():
        if not got_pong["ok"] and alive["run"]:
            link.set("PING ушёл, PONG пока нет. Проверьте, тот ли это COM-порт.")

    def on_close():
        alive["run"] = False
        serial.close()
        thread.join(timeout=0.5)
        with log_lock:
            log_file.close()
        root.destroy()

    thread = threading.Thread(target=reader, daemon=True)
    thread.start()
    try:
        send_line("PING")
    except OSError as exc:
        on_close()
        sys.stderr.write("Не удалось отправить PING: %s\n" % exc)
        return 1
    root.after(100, poll)
    root.after(1500, ping_timeout)
    root.protocol("WM_DELETE_WINDOW", on_close)
    root.mainloop()
    return 0


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
    return run_gui(args.port, args.log)


if __name__ == "__main__":
    sys.exit(main())
