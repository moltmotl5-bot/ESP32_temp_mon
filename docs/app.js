const MAX_HISTORY_HOURS = 72;

const chartView = {
  windowHours: 12,
  scrollHours: 0,
  followLive: true
};

let cachedPoints = [];

function yBounds(points) {
  let min = 999;
  let max = -999;
  points.forEach((p) => {
    if (p.temp < min) min = p.temp;
    if (p.temp > max) max = p.temp;
  });
  if (!Number.isFinite(min)) {
    min = 20;
    max = 30;
  }
  let yMin = Math.floor(min / 5) * 5;
  let yMax = Math.ceil(max / 5) * 5;
  if (yMax - yMin < 10) {
    yMin -= 5;
    yMax += 5;
  }
  return { min: yMin, max: yMax };
}

function fmtTime(ts) {
  const d = new Date(ts * 1000);
  const p = (n) => String(n).padStart(2, "0");
  return `${p(d.getMonth() + 1)}/${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}`;
}

function fmtHour(ts) {
  const d = new Date(ts * 1000);
  return `${String(d.getHours()).padStart(2, "0")}:00`;
}

function fmtAxisLabel(ts, windowHours) {
  if (windowHours > 24) {
    const d = new Date(ts * 1000);
    const p = (n) => String(n).padStart(2, "0");
    return `${p(d.getMonth() + 1)}/${p(d.getDate())} ${p(d.getHours())}:00`;
  }
  return fmtHour(ts);
}

function tickStepHours(windowHours) {
  if (windowHours <= 12) return 1;
  if (windowHours <= 24) return 2;
  return 6;
}

function getDataBounds(points) {
  const now = Math.floor(Date.now() / 1000);
  if (!points.length) {
    return { end: now, start: now - MAX_HISTORY_HOURS * 3600 };
  }
  let end = points[0].ts;
  let start = points[0].ts;
  points.forEach((p) => {
    if (p.ts > end) end = p.ts;
    if (p.ts < start) start = p.ts;
  });
  const earliestAllowed = end - MAX_HISTORY_HOURS * 3600;
  return { end, start: Math.max(start, earliestAllowed) };
}

function computeViewWindow(points) {
  const { end, start } = getDataBounds(points);
  const maxScrollHours = Math.max(0, Math.floor((end - start) / 3600 - chartView.windowHours));
  if (chartView.followLive) {
    chartView.scrollHours = 0;
  } else {
    chartView.scrollHours = Math.min(chartView.scrollHours, maxScrollHours);
  }
  const t1 = end - chartView.scrollHours * 3600;
  const t0 = t1 - chartView.windowHours * 3600;
  return { t0, t1, xSpan: chartView.windowHours * 3600, maxScrollHours };
}

function updateChartControls(points) {
  const { t0, t1, maxScrollHours } = computeViewWindow(points);
  const slider = document.getElementById("chart-scroll");
  const label = document.getElementById("chart-range-label");
  const scrollWrap = document.getElementById("chart-scroll-wrap");

  slider.max = String(maxScrollHours);
  slider.value = String(chartView.scrollHours);
  slider.disabled = maxScrollHours <= 0;
  scrollWrap.hidden = chartView.windowHours >= MAX_HISTORY_HOURS;

  label.textContent = `${fmtTime(t0)} — ${fmtTime(t1)}`;
  document.getElementById("chart-window-label").textContent =
    chartView.windowHours >= MAX_HISTORY_HOURS
      ? "3-day temperature (Y: 5 °C grid)"
      : `${chartView.windowHours}-hour window (scroll back up to 3 days)`;
}

function drawChart(allPoints) {
  const c = document.getElementById("chart");
  const g = c.getContext("2d");
  const w = (c.width = c.clientWidth);
  const h = (c.height = c.clientHeight);
  const pad = { l: 46, r: 12, t: 14, b: 34 };
  const pw = w - pad.l - pad.r;
  const ph = h - pad.t - pad.b;
  const baseY = h - pad.b;
  g.fillStyle = "#fff";
  g.fillRect(0, 0, w, h);

  const { t0, t1, xSpan } = computeViewWindow(allPoints);
  const windowHours = chartView.windowHours;

  let points = allPoints.filter((p) => p.ts >= t0 && p.ts <= t1);
  if (!points.length && allPoints.length) {
    points = allPoints.filter((p) => Math.abs(p.ts - t1) < xSpan || Math.abs(p.ts - t0) < xSpan);
    if (!points.length) points = [allPoints[allPoints.length - 1]];
  }

  const { min: yMin, max: yMax } = yBounds(points.length ? points : allPoints);
  const ySpan = yMax - yMin || 5;
  g.font = "11px system-ui,sans-serif";
  g.strokeStyle = "#e5e7eb";
  g.lineWidth = 1;
  g.fillStyle = "#666";
  g.textAlign = "right";
  for (let t = yMin; t <= yMax; t += 5) {
    const y = pad.t + ph * (1 - (t - yMin) / ySpan);
    g.beginPath();
    g.moveTo(pad.l, y);
    g.lineTo(w - pad.r, y);
    g.stroke();
    g.fillText(`${t}°`, pad.l - 6, y + 4);
  }
  g.textAlign = "left";

  g.strokeStyle = "#cbd5e1";
  g.lineWidth = 1;
  g.beginPath();
  g.moveTo(pad.l, baseY);
  g.lineTo(w - pad.r, baseY);
  g.stroke();

  const stepH = tickStepHours(windowHours);
  const tickCount = Math.floor(windowHours / stepH);
  g.font = "10px system-ui,sans-serif";
  for (let i = 0; i <= tickCount; i++) {
    const tickTs = t0 + i * stepH * 3600;
    if (tickTs > t1 + 60) break;
    const x = pad.l + pw * ((tickTs - t0) / xSpan);
    if (i > 0 && i < tickCount) {
      g.strokeStyle = "#f3f4f6";
      g.lineWidth = 1;
      g.beginPath();
      g.moveTo(x, pad.t);
      g.lineTo(x, baseY);
      g.stroke();
    }
    g.fillStyle = "#dc2626";
    g.beginPath();
    g.arc(x, baseY, 3.5, 0, Math.PI * 2);
    g.fill();
    g.fillStyle = "#444";
    g.textAlign = "center";
    const lbl = fmtAxisLabel(tickTs, windowHours);
    g.fillText(lbl, Math.max(pad.l + 20, Math.min(w - pad.r - 20, x)), baseY + 14);
  }
  g.textAlign = "left";

  if (!points.length) {
    g.fillStyle = "#666";
    g.font = "14px system-ui,sans-serif";
    g.fillText("No data in selected range", pad.l + 8, h / 2);
    return;
  }

  g.strokeStyle = "#2563eb";
  g.lineWidth = 2;
  g.beginPath();
  points.forEach((p, i) => {
    const x = pad.l + pw * Math.min(1, Math.max(0, (p.ts - t0) / xSpan));
    const y = pad.t + ph * (1 - (p.temp - yMin) / ySpan);
    if (i === 0) g.moveTo(x, y);
    else g.lineTo(x, y);
  });
  g.stroke();
  points.forEach((p) => {
    const x = pad.l + pw * Math.min(1, Math.max(0, (p.ts - t0) / xSpan));
    const y = pad.t + ph * (1 - (p.temp - yMin) / ySpan);
    g.fillStyle = "#2563eb";
    g.beginPath();
    g.arc(x, y, windowHours >= 72 ? 1.5 : 2.5, 0, Math.PI * 2);
    g.fill();
  });
}

function buildReadingsTable(points, sourceLabel) {
  const tbody = document.getElementById("readings");
  document.getElementById("readings-source").textContent = sourceLabel;
  document.getElementById("readings-count").textContent = `${points.length} records`;
  if (!points.length) {
    tbody.innerHTML = '<tr><td colspan="3">No data</td></tr>';
    return;
  }
  const rows = [...points].sort((a, b) => b.ts - a.ts);
  tbody.innerHTML = rows
    .map(
      (p) =>
        `<tr><td>${fmtTime(p.ts)}</td><td>${p.temp.toFixed(1)}</td><td>${p.hum.toFixed(1)}</td></tr>`
    )
    .join("");
}

function updateStatus(meta) {
  document.getElementById("temp").textContent =
    meta && meta.temp != null ? `${Number(meta.temp).toFixed(1)} °C` : "--";
  document.getElementById("hum").textContent =
    meta && meta.hum != null ? `${Number(meta.hum).toFixed(1)} %` : "--";
  document.getElementById("bat").textContent =
    meta && meta.battery != null ? `${meta.battery} %` : "--";
  if (!meta) {
    document.getElementById("meta").textContent = "Waiting for device data…";
    return;
  }
  document.getElementById("meta").innerHTML =
    `Clock: <b>${meta.clock || "--"}</b><br>` +
    `SD: ${meta.sd ? "OK" : "--"} | Records: ${meta.records ?? "--"}/${meta.recordsMax ?? "--"} | ` +
    `Updated: ${meta.updatedAt ? fmtTime(meta.updatedAt) : "--"}`;
}

function readingsToPoints(readingsVal) {
  if (!readingsVal) return [];
  return Object.entries(readingsVal).map(([ts, v]) => ({
    ts: Number(ts),
    temp: Number(v.temp),
    hum: Number(v.hum)
  }));
}

function redrawChart() {
  updateChartControls(cachedPoints);
  drawChart(cachedPoints);
}

function setupChartControls() {
  if (setupChartControls.ready) return;
  setupChartControls.ready = true;
  document.querySelectorAll("[data-chart-hours]").forEach((btn) => {
    btn.addEventListener("click", () => {
      chartView.windowHours = Number(btn.dataset.chartHours);
      chartView.followLive = chartView.scrollHours === 0;
      document.querySelectorAll("[data-chart-hours]").forEach((b) => b.classList.remove("active"));
      btn.classList.add("active");
      redrawChart();
    });
  });

  document.getElementById("chart-scroll").addEventListener("input", (e) => {
    chartView.scrollHours = Number(e.target.value);
    chartView.followLive = chartView.scrollHours === 0;
    redrawChart();
  });

  document.getElementById("chart-live-btn").addEventListener("click", () => {
    chartView.scrollHours = 0;
    chartView.followLive = true;
    redrawChart();
  });
}

let db = null;
let refreshTimer = null;

function deviceBaseRef() {
  return db.ref(`devices/${window.DEVICE_ID}`);
}

async function refreshDashboard() {
  const metaSnap = await deviceBaseRef().child("meta").get();
  const readingsSnap = await deviceBaseRef().child("readings").get();
  const meta = metaSnap.val();
  cachedPoints = readingsToPoints(readingsSnap.val());
  updateStatus(meta);
  updateChartControls(cachedPoints);
  drawChart(cachedPoints);
  buildReadingsTable(cachedPoints, "Firebase RTDB");
}

function showDashboard() {
  document.getElementById("login-panel").hidden = true;
  document.getElementById("dashboard").hidden = false;
  setupChartControls();
  refreshDashboard();
  if (refreshTimer) clearInterval(refreshTimer);
  refreshTimer = setInterval(refreshDashboard, 30000);
}

function showLogin(message) {
  document.getElementById("login-panel").hidden = false;
  document.getElementById("dashboard").hidden = true;
  if (message) {
    document.getElementById("login-error").textContent = message;
  }
  if (refreshTimer) {
    clearInterval(refreshTimer);
    refreshTimer = null;
  }
}

function initApp() {
  if (!window.FIREBASE_CONFIG || window.FIREBASE_CONFIG.apiKey === "YOUR_API_KEY") {
    showLogin("Configure docs/firebase-config.js before use.");
    return;
  }

  firebase.initializeApp(window.FIREBASE_CONFIG);
  db = firebase.database();
  const auth = firebase.auth();

  document.getElementById("login-form").addEventListener("submit", async (e) => {
    e.preventDefault();
    const email = document.getElementById("email").value.trim();
    const password = document.getElementById("password").value;
    document.getElementById("login-error").textContent = "";
    try {
      await auth.signInWithEmailAndPassword(email, password);
    } catch (err) {
      document.getElementById("login-error").textContent = err.message || "Login failed";
    }
  });

  document.getElementById("logout-btn").addEventListener("click", async () => {
    await auth.signOut();
  });

  auth.onAuthStateChanged((user) => {
    if (user) {
      showDashboard();
    } else {
      showLogin("");
    }
  });
}

document.addEventListener("DOMContentLoaded", initApp);
