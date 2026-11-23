var board = document.getElementById("board");
var meta = document.getElementById("meta");
var dateInput = document.getElementById("date");
var timeInput = document.getElementById("time");
var weekday = document.getElementById("weekday");
var link = document.getElementById("link");
var notice = document.getElementById("notice");
var seeded = false;

function fill(data) {
  if (!data) return;
  if (data.iso_date) dateInput.value = data.iso_date;
  if (data.time && data.time.indexOf("-") < 0) timeInput.value = data.time;
  if (data.weekday) weekday.value = String(data.weekday);
}

async function refresh() {
  var response = await fetch("/api/state");
  var data = await response.json();
  board.value = data.time;
  meta.textContent = data.date + " · " + data.weekday_name + " · " + data.state_text;
  link.textContent = data.link;
  if (data.notice) notice.textContent = data.notice;
  if (!seeded && data.valid) {
    fill(data);
    seeded = true;
  }
}

async function submit(useNow) {
  var body = useNow ? {now: true} : {
    date: dateInput.value,
    time: timeInput.value,
    weekday: Number(weekday.value)
  };
  var response = await fetch("/api/set", {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify(body)
  });
  var data = await response.json();
  notice.textContent = data.notice || data.error || "";
  if (response.ok) {
    fill(data);
    seeded = true;
  }
}

dateInput.addEventListener("input", function () { seeded = true; });
timeInput.addEventListener("input", function () { seeded = true; });
weekday.addEventListener("change", function () { seeded = true; });
document.getElementById("apply").onclick = function () { submit(false); };
document.getElementById("now").onclick = function () { submit(true); };
refresh();
setInterval(refresh, 200);
