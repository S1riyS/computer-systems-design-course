"""Поток UART, журнал и страница часов на 127.0.0.1."""

import json
import os
import sys
import threading
import webbrowser
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from protocol import (
    STATE_TEXT,
    WEEKDAYS,
    build_set,
    current_fields,
    iso_date,
    parse_clock_form,
    parse_time,
    weekday_options,
)
from serial_port import BAUD, SerialPort

STATIC_DIR = Path(__file__).resolve().parent / "static"
STATIC_FILES = {
    "/": ("index.html", "text/html; charset=utf-8"),
    "/index.html": ("index.html", "text/html; charset=utf-8"),
    "/style.css": ("style.css", "text/css; charset=utf-8"),
    "/app.js": ("app.js", "text/javascript; charset=utf-8"),
}


class BoardView:
    def __init__(self, port_name):
        self._lock = threading.Lock()
        self.link = "Открыт %s. Жду ответ стенда…" % port_name
        self.notice = ""
        self.time = "--:--:--"
        self.date = "--/--/--"
        self.weekday = 0
        self.state = ""
        self.valid = False
        self.pong = False

    def update(self, **fields):
        with self._lock:
            self.__dict__.update(fields)

    def snapshot(self):
        with self._lock:
            names = dict(WEEKDAYS)
            return {
                "link": self.link,
                "notice": self.notice,
                "time": self.time,
                "date": self.date,
                "weekday": self.weekday,
                "weekday_name": names.get(self.weekday, "—"),
                "state": self.state,
                "state_text": STATE_TEXT.get(self.state, "нет данных"),
                "valid": self.valid,
                "iso_date": iso_date(self.date) if self.valid else "",
            }


def run_web(port_name, log_path):
    try:
        serial = SerialPort(port_name, BAUD)
    except OSError as exc:
        sys.stderr.write("Не удалось открыть %s: %s\n" % (port_name, exc))
        return 1

    log_file = open(log_path, "a", encoding="utf-8")
    log_lock = threading.Lock()
    alive = {"run": True}
    view = BoardView(port_name)

    def log_message(direction, text):
        stamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        with log_lock:
            log_file.write("%s %s %s\n" % (stamp, direction, text))
            log_file.flush()

    def send_line(text):
        log_message("TX", text)
        serial.write((text + "\n").encode("ascii"))

    def apply_line(line):
        if line == "PONG":
            view.update(pong=True, link="Стенд на связи. Порт %s." % port_name)
            return
        if line == "OK":
            view.update(notice="Стенд принял новое время.")
            return
        if line == "ERR":
            view.update(notice="Стенд отклонил команду.")
            return
        parsed = parse_time(line)
        if parsed is None and line.startswith("TIME "):
            view.update(
                valid=False, time="--:--:--", date="--/--/--", weekday=0, state="ERR",
            )
            return
        if parsed is None:
            return
        view.update(valid=True, **parsed)

    def reader():
        pending = b""
        while alive["run"]:
            try:
                chunk = serial.read(128)
            except OSError:
                if alive["run"]:
                    view.update(link="Связь с портом прервалась.")
                return
            if not chunk:
                continue
            pending += chunk
            while b"\n" in pending:
                raw, pending = pending.split(b"\n", 1)
                text = raw.decode("ascii", "replace").strip("\r").strip()
                if text:
                    log_message("RX", text)
                    apply_line(text)

    def ping_timeout():
        with view._lock:
            got_pong = view.pong
        if got_pong or not alive["run"]:
            return
        view.update(link="PING ушёл, ответа нет. Проверьте, тот ли это COM-порт.")

    def send_fields(fields):
        hour, minute, second, day, month, year, weekday = fields
        send_line(build_set(hour, minute, second, day, month, year, weekday))
        view.update(notice="Команда отправлена, жду ответ стенда.")
        return {
            "notice": "Команда отправлена, жду ответ стенда.",
            "iso_date": "20%02d-%02d-%02d" % (year, month, day),
            "time": "%02d:%02d:%02d" % (hour, minute, second),
            "weekday": weekday,
        }

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):
            return

        def _send(self, code, content_type, body):
            data = body if isinstance(body, bytes) else body.encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):
            path = self.path.split("?", 1)[0]
            if path == "/api/state":
                self._send(200, "application/json; charset=utf-8", json.dumps(view.snapshot()))
                return
            static = STATIC_FILES.get(path)
            if static is None:
                self._send(404, "text/plain; charset=utf-8", "not found")
                return
            name, content_type = static
            body = (STATIC_DIR / name).read_text(encoding="utf-8")
            if name == "index.html":
                body = body.replace("__OPTIONS__", weekday_options())
            self._send(200, content_type, body)

        def do_POST(self):
            if self.path.split("?", 1)[0] != "/api/set":
                self._send(404, "text/plain; charset=utf-8", "not found")
                return
            length = int(self.headers.get("Content-Length", "0") or 0)
            try:
                payload = json.loads(self.rfile.read(length).decode("utf-8") or "{}")
            except json.JSONDecodeError:
                self._send(400, "application/json; charset=utf-8", json.dumps({"error": "Не понял запрос."}))
                return
            if payload.get("now"):
                fields = current_fields()
            else:
                fields = parse_clock_form(
                    payload.get("date", ""), payload.get("time", ""), payload.get("weekday", 0),
                )
            if fields is None:
                message = "Такой даты или времени не бывает."
                view.update(notice=message)
                self._send(400, "application/json; charset=utf-8", json.dumps({"error": message}))
                return
            try:
                result = send_fields(fields)
            except OSError as exc:
                message = "Не удалось отправить: %s" % exc
                view.update(notice=message)
                self._send(500, "application/json; charset=utf-8", json.dumps({"error": message}))
                return
            self._send(200, "application/json; charset=utf-8", json.dumps(result))

    class Server(ThreadingHTTPServer):
        daemon_threads = True
        allow_reuse_address = True

    log_message("NOTE", "открыт %s, %d 8N1" % (port_name, BAUD))
    print("Журнал UART: %s" % os.path.abspath(log_path), flush=True)
    thread = threading.Thread(target=reader, daemon=True)
    thread.start()
    try:
        send_line("PING")
    except OSError as exc:
        alive["run"] = False
        serial.close()
        log_file.close()
        sys.stderr.write("Не удалось отправить PING: %s\n" % exc)
        return 1

    timer = threading.Timer(1.5, ping_timeout)
    timer.daemon = True
    timer.start()
    try:
        httpd = Server(("127.0.0.1", 0), Handler)
    except OSError as exc:
        alive["run"] = False
        serial.close()
        log_file.close()
        sys.stderr.write("Не удалось открыть страницу: %s\n" % exc)
        return 1

    url = "http://127.0.0.1:%d/" % httpd.server_address[1]
    print("Окно часов: %s" % url, flush=True)
    print("Закрыть клиент: Ctrl+C", flush=True)
    webbrowser.open(url)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print()
    finally:
        alive["run"] = False
        timer.cancel()
        httpd.server_close()
        serial.close()
        thread.join(timeout=0.5)
        log_file.close()
    return 0
