#include "web_server.h"

#include "config.h"
#include "record_store.h"
#include "time_keeper.h"

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

namespace {
WebServer server(80);
WebDashboardState dashboardState;
bool running = false;

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Temp Monitor</title>
<style>
*{box-sizing:border-box}body{font-family:system-ui,sans-serif;margin:0;background:#f4f6f8;color:#111}
.wrap{max-width:720px;margin:0 auto;padding:16px}
h1{font-size:1.4rem;margin:0 0 12px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px;margin-bottom:16px}
.card{background:#fff;border:1px solid #ccd;border-radius:8px;padding:14px}
.card .label{font-size:.8rem;color:#555;margin-bottom:4px}
.card .value{font-size:1.6rem;font-weight:700}
.meta{font-size:.85rem;line-height:1.6;background:#fff;border:1px solid #ccd;border-radius:8px;padding:12px;margin-bottom:16px}
.chart-box{background:#fff;border:1px solid #ccd;border-radius:8px;padding:12px;margin-bottom:12px}
canvas{width:100%;height:220px;display:block}
.foot{font-size:.75rem;color:#666;text-align:center}
</style>
</head>
<body>
<div class="wrap">
<h1>Temp Monitor Dashboard</h1>
<div class="grid">
<div class="card"><div class="label">Temperature</div><div class="value" id="temp">--</div></div>
<div class="card"><div class="label">Humidity</div><div class="value" id="hum">--</div></div>
<div class="card"><div class="label">Battery</div><div class="value" id="bat">--</div></div>
</div>
<div class="meta" id="meta">Loading...</div>
<div class="chart-box"><div class="label">12-hour temperature</div><canvas id="chart"></canvas></div>
<div class="foot">Auto refresh 30s</div>
</div>
<script>
const YMIN=20,YMAX=45;
function drawChart(points){
 const c=document.getElementById('chart'),x=c.getContext('2d'),w=c.width=c.clientWidth,h=c.height=c.clientHeight;
 x.fillStyle='#fff';x.fillRect(0,0,w,h);
 x.strokeStyle='#ddd';x.lineWidth=1;
 for(let i=0;i<=4;i++){const gy=8+i*(h-16)/4;x.beginPath();x.moveTo(40,gy);x.lineTo(w-8,gy);x.stroke();}
 if(!points.length){x.fillStyle='#666';x.fillText('No data',48,h/2);return;}
 x.strokeStyle='#111';x.lineWidth=2;x.beginPath();
 points.forEach((p,i)=>{
  const px=40+(w-48)*i/(points.length-1||1);
  const py=8+(h-16)*(1-(p.temp-YMIN)/(YMAX-YMIN));
  if(i===0)x.moveTo(px,py);else x.lineTo(px,py);
 });
 x.stroke();
 x.fillStyle='#666';x.fillText('-12h',42,h-2);x.fillText('now',w-36,h-2);
}
async function refresh(){
 try{
  const sr=await fetch('/api/status');
  if(!sr.ok) throw new Error('status '+sr.status);
  const s=await sr.json();
  document.getElementById('temp').textContent=s.sensor?s.temp.toFixed(1)+' C':'ERR';
  document.getElementById('hum').textContent=s.sensor?s.hum.toFixed(1)+' %':'ERR';
  document.getElementById('bat').textContent=s.battery>=0?s.battery+' %':'--';
  document.getElementById('meta').innerHTML=
    'Clock: <b>'+s.clock+'</b><br>'+
    'WiFi: '+(s.wifi?'OK':'--')+' | NTP: '+(s.timeValid?(s.wifi?'OK':'loc'):'--')+
    ' | SD: '+(s.sd?'OK':'--')+' | Records: '+s.records+'/'+s.recordsMax;
  const hr=await fetch('/api/history');
  if(!hr.ok) throw new Error('history '+hr.status);
  const h=await hr.json();
  drawChart(h.points||[]);
 }catch(e){document.getElementById('meta').textContent='Fetch failed: '+e.message;}
}
refresh();setInterval(refresh,30000);
</script>
</body>
</html>)rawliteral";

void formatClock(char* out, size_t outLen) {
  if (!out || outLen == 0) return;
  const time_t now = time(nullptr);
  struct tm timeInfo {};
  if (timeKeeperIsValid() && localtime_r(&now, &timeInfo)) {
    snprintf(out, outLen, "%04d-%02d-%02d %02d:%02d:%02d", timeInfo.tm_year + 1900,
             timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min,
             timeInfo.tm_sec);
  } else {
    snprintf(out, outLen, "---- -- -- --:--:--");
  }
}

void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

void handleStatus() {
  char clockLine[32];
  formatClock(clockLine, sizeof(clockLine));

  char body[320];
  snprintf(body, sizeof(body),
           "{\"temp\":%.1f,\"hum\":%.1f,\"sensor\":%s,\"battery\":%d,\"wifi\":%s,"
           "\"timeValid\":%s,\"sd\":%s,\"records\":%u,\"recordsMax\":%u,\"clock\":\"%s\"}",
           dashboardState.tempC, dashboardState.humidity, dashboardState.hasSensor ? "true" : "false",
           static_cast<int>(dashboardState.batteryPct),
           dashboardState.wifiConnected ? "true" : "false",
           dashboardState.timeValid ? "true" : "false", dashboardState.sdReady ? "true" : "false",
           dashboardState.recordCount, dashboardState.recordMax, clockLine);
  server.send(200, "application/json; charset=utf-8", body);
}

void handleHistory() {
  TempRecord recs[CHART_POINTS];
  const uint16_t count = recordStoreCopyRecent(CHART_POINTS, recs);

  String body;
  body.reserve(static_cast<unsigned>(count) * 48U + 16U);
  body = "{\"points\":[";

  bool first = true;
  for (uint16_t i = 0; i < count; ++i) {
    const TempRecord& rec = recs[i];
    if (isnan(rec.temperature) || isnan(rec.humidity)) continue;

    if (!first) body += ',';
    first = false;

    char item[72];
    snprintf(item, sizeof(item), "{\"ts\":%lu,\"temp\":%.1f,\"hum\":%.1f}",
             static_cast<unsigned long>(rec.timestamp), rec.temperature, rec.humidity);
    body += item;
  }

  body += "]}";
  server.send(200, "application/json; charset=utf-8", body);
}

void handleNotFound() { server.send(404, "text/plain", "Not found"); }
}  // namespace

void webServerUpdateState(const WebDashboardState& state) { dashboardState = state; }

bool webServerActive() { return running; }

void webServerBegin() {
  if (running) return;

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/history", HTTP_GET, handleHistory);
  server.onNotFound(handleNotFound);
  server.begin();

  running = true;
  Serial.print("Web dashboard: http://");
  Serial.println(WiFi.localIP());
}

void webServerStop() {
  if (!running) return;
  server.stop();
  running = false;
  Serial.println("Web dashboard stopped");
}

void webServerLoop() {
  if (running) {
    server.handleClient();
  }
}
