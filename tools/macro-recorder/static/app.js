const $ = (id) => document.getElementById(id);

let lastSeq = 0;
let feedGeneration = 0;
let eventsPending = false;
let statusPending = false;
let actionError = '';
let eventsError = '';

let TOKEN = localStorage.getItem("macro_token") || "";
if (!TOKEN) {
  TOKEN = prompt("Access token (leave empty if none):") || "";
  if (TOKEN) localStorage.setItem("macro_token", TOKEN);
}
function authOptions(options = {}) {
  const headers = new Headers(options.headers || {});
  if (TOKEN) headers.set("Authorization", "Bearer " + TOKEN);
  return { ...options, headers };
}

async function api(path, options) {
  const response = await fetch(path, authOptions(options));
  if (!response.ok) throw new Error('HTTP ' + response.status);
  return response.json();
}

function setConnectionStatus(text) {
  const status = $("connectionStatus");
  if (status) status.textContent = text;
}

function fmtTime(iso) {
  const d = new Date(iso);
  const ms = String(d.getMilliseconds()).padStart(3, "0");
  return d.toLocaleTimeString([], { hour12: false }) + "." + ms;
}

function keyEl(ev) {
  const row = document.createElement("div");

  if (ev.type === "meta") {
    row.className = "ev ev-meta";
    row.textContent =
      (ev.action === "start" ? "\u25CF Recording started" : "\u25A0 Recording stopped") +
      " \u2014 " +
      (ev.time || "");
    return row;
  }

  row.className = "ev ev-key";

  const t = document.createElement("span");
  t.className = "ev-time";
  t.textContent = fmtTime(ev.time);

  const cap = document.createElement("span");
  cap.className = "cap " + (ev.kind === "char" ? "cap-char" : "cap-special");
  if (ev.kind === "char") {
    cap.textContent = ev.key === " " ? "\u2423" : ev.key;
  } else {
    cap.textContent = (ev.type === "up" ? "\u2191 " : "") + ev.key;
  }

  const dt = document.createElement("span");
  dt.className = "ev-dt";
  dt.textContent = ev.dt != null ? "+" + ev.dt + "s" : "";

  row.append(t, cap, dt);
  return row;
}

function render(events) {
  const feed = $("feed");
  for (const ev of events) {
    feed.prepend(keyEl(ev));
  }
  while (feed.children.length > 2000) {
    feed.removeChild(feed.lastChild);
  }
}

async function pollEvents() {
  if (eventsPending) return;
  eventsPending = true;
  const generation = feedGeneration;
  try {
    const d = await api("/api/events?since=" + lastSeq);
    if (generation !== feedGeneration) return;
    if (!Array.isArray(d.events) || !Number.isSafeInteger(d.next) || d.next < 0) {
      throw new Error('Invalid event response');
    }
    eventsError = '';
    if (d.next < lastSeq) {
      // A restarted server has a new sequence; fetch its retained events from zero.
      $('feed').replaceChildren();
      lastSeq = 0;
      return;
    }
    render(d.events);
    lastSeq = d.next;
  } catch (_) {
    if (generation !== feedGeneration) return;
    eventsError = 'Recorder events unavailable';
    setConnectionStatus("Recorder unavailable");
  } finally {
    eventsPending = false;
  }
}

async function pollStatus() {
  if (statusPending) return;
  statusPending = true;
  try {
    const s = await api("/api/status");
    if (typeof s.recording !== 'boolean' || !Number.isSafeInteger(s.total_recorded) || s.total_recorded < 0 || typeof s.log_path !== 'string') {
      throw new Error('Invalid status response');
    }
    const badge = $("badge");
    badge.textContent = s.recording ? "RECORDING" : "IDLE";
    badge.className = "badge " + (s.recording ? "on" : "off");
    $("count").textContent = s.total_recorded;
    $("logPath").textContent = s.log_path;
    $("btnStart").disabled = s.recording;
    $("btnStop").disabled = !s.recording;
    setConnectionStatus(actionError || eventsError || "Connected");
  } catch (_) {
    $("btnStart").disabled = true;
    $("btnStop").disabled = true;
    $("badge").textContent = 'UNKNOWN';
    $("badge").className = 'badge off';
    setConnectionStatus("Recorder unavailable");
  } finally {
    statusPending = false;
  }
}

async function post(path) {
  try {
    const result = await api(path, { method: "POST" });
    if (result.ok !== true) throw new Error(result.reason || result.error || 'Request failed');
    actionError = '';
    if (path === '/api/clear') {
      feedGeneration++;
      $('feed').replaceChildren();
    }
    await pollStatus();
    return true;
  } catch (error) {
    actionError = 'Request failed: ' + error.message;
    setConnectionStatus(actionError);
    return false;
  }
}

$("btnStart").onclick = () => post("/api/start");
$("btnStop").onclick = () => post("/api/stop");
$("btnClear").onclick = () => post("/api/clear");
$("btnExportJson").onclick = () => exportLog("json");
$("btnExportCsv").onclick = () => exportLog("csv");

async function exportLog(format) {
  try {
    const response = await fetch("/api/export?format=" + format, authOptions());
    if (!response.ok) {
      setConnectionStatus("Export failed");
      return;
    }
    const blob = await response.blob();
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob);
    link.download = "macro-log." + format;
    link.click();
    URL.revokeObjectURL(link.href);
  } catch (_) {
    setConnectionStatus("Export unavailable");
  }
}

pollEvents();
pollStatus();
setInterval(pollEvents, 800);
setInterval(pollStatus, 800);
