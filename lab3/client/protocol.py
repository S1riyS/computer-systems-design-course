"""Строки UART и проверка даты. Железо и страница сюда не входят."""

import re
from datetime import datetime

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


def iso_date(date_text):
    match = re.fullmatch(r"(\d{2})/(\d{2})/(\d{2})", date_text or "")
    if match is None:
        return ""
    day, month, year = match.groups()
    return "20%s-%s-%s" % (year, month, day)


def parse_clock_form(date_text, time_text, weekday):
    date_text = (date_text or "").strip()
    time_text = (time_text or "").strip()
    iso = re.fullmatch(r"(\d{4})-(\d{2})-(\d{2})", date_text)
    local = re.fullmatch(r"(\d{1,2})[./](\d{1,2})[./](\d{2,4})", date_text)
    if iso is not None:
        year = int(iso.group(1)) % 100
        month = int(iso.group(2))
        day = int(iso.group(3))
    elif local is not None:
        day = int(local.group(1))
        month = int(local.group(2))
        year = int(local.group(3)) % 100
    else:
        return None
    clock = re.fullmatch(r"(\d{1,2}):(\d{2})(?::(\d{2}))?", time_text)
    if clock is None:
        return None
    hour = int(clock.group(1))
    minute = int(clock.group(2))
    second = int(clock.group(3) or 0)
    if not fields_ok(hour, minute, second, day, month, year, int(weekday)):
        return None
    return hour, minute, second, day, month, year, int(weekday)


def current_fields():
    now = datetime.now()
    return now.hour, now.minute, now.second, now.day, now.month, now.year % 100, now.isoweekday()


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


def weekday_options():
    return "".join(
        '<option value="%d">%s</option>' % (code, name) for code, name in WEEKDAYS
    )
