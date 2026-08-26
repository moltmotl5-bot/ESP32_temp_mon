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
<html lang="zh-Hant">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Temperature Dashboard</title>
<style>
*{box-sizing:border-box}body{font-family:system-ui,sans-serif;margin:0;background:#f4f6f8;color:#111}
.wrap{max-width:720px;margin:0 auto;padding:16px}
h1{font-size:1.4rem;margin:0 0 12px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px;margin-bottom:16px}
.card{background:#fff;border:1px solid #ccd;border-radius:8px;padding:14px}
.card .label,.section-label{font-size:.8rem;color:#555;margin-bottom:4px}
.card .value{font-size:1.6rem;font-weight:700}
.meta{font-size:.85rem;line-height:1.6;background:#fff;border:1px solid #ccd;border-radius:8px;padding:12px;margin-bottom:16px}
.chart-box,.table-box{background:#fff;border:1px solid #ccd;border-radius:8px;padding:12px;margin-bottom:12px}
canvas{width:100%;height:260px;display:block}
table{width:100%;border-collapse:collapse;font-size:.85rem}
th,td{padding:8px 10px;text-align:left;border-bottom:1px solid #e5e7eb}
th{background:#f9fafb;font-weight:600;color:#444}
tbody tr:last-child td{border-bottom:none}
.table-scroll{max-height:480px;overflow-y:auto}
.foot{font-size:.75rem;color:#666;text-align:center;margin-top:8px}
</style>
</head>
<body>
<div class="wrap">
<h1>Temperature Dashboard</h1>
<div class="grid">
<div class="card"><div class="label">Temperature</div><div class="value" id="temp">--</div></div>
<div class="card"><div class="label">Humidity</div><div class="value" id="hum">--</div></div>
<div class="card"><div class="label">Battery</div><div class="value" id="bat">--</div></div>
</div>
<div class="meta" id="meta">Loading...</div>
<div class="chart-box">
<div class="section-label">12-hour temperature (Y: 5 °C grid, X: hourly)</div>
<canvas id="chart"></canvas>
</div>
<div class="table-box">
<div class="section-label">All stored readings (NVS, <span id="readings-count">--</span>)</div>
<div class="table-scroll">
<table>
<thead><tr><th>Time</th><th>Temp (°C)</th><th>Humidity (%)</th></tr></thead>
<tbody id="readings"><tr><td colspan="3">Loading...</td></tr></tbody>
</table>
</div>
</div>
<div class="foot">Auto refresh 30s</div>
</div>
<script>
const CHART_HOURS=12;

function yBounds(points){
 let min=999,max=-999;
 points.forEach(p=>{if(p.temp<min)min=p.temp;if(p.temp>max)max=p.temp;});
 if(!isFinite(min)){min=20;max=30;}
 let yMin=Math.floor(min/5)*5;
 let yMax=Math.ceil(max/5)*5;
 if(yMax-yMin<10){yMin-=5;yMax+=5;}
 return {min:yMin,max:yMax};
}

function fmtTime(ts){
 const d=new Date(ts*1000);
 const p=n=>String(n).padStart(2,'0');
 return p(d.getMonth()+1)+'/'+p(d.getDate())+' '+p(d.getHours())+':'+p(d.getMinutes());
}

function fmtHour(ts){
 const d=new Date(ts*1000);
 return String(d.getHours()).padStart(2,'0')+':00';
}

function drawChart(allPoints){
 const c=document.getElementById('chart');
 const g=c.getContext('2d');
 const w=c.width=c.clientWidth,h=c.height=c.clientHeight;
 const pad={l:46,r:12,t:14,b:34};
 const pw=w-pad.l-pad.r,ph=h-pad.t-pad.b;
 const baseY=h-pad.b;
 g.fillStyle='#fff';g.fillRect(0,0,w,h);

 const t1=allPoints.length?allPoints[allPoints.length-1].ts:Math.floor(Date.now()/1000);
 const t0=t1-CHART_HOURS*3600;
 const xSpan=CHART_HOURS*3600;

 let points=allPoints.filter(p=>p.ts>=t0&&p.ts<=t1);
 if(!points.length&&allPoints.length) points=[allPoints[allPoints.length-1]];

 const {min:yMin,max:yMax}=yBounds(points.length?points:allPoints);
 const ySpan=yMax-yMin||5;
 g.font='11px system-ui,sans-serif';
 g.strokeStyle='#e5e7eb';g.lineWidth=1;
 g.fillStyle='#666';g.textAlign='right';
 for(let t=yMin;t<=yMax;t+=5){
  const y=pad.t+ph*(1-(t-yMin)/ySpan);
  g.beginPath();g.moveTo(pad.l,y);g.lineTo(w-pad.r,y);g.stroke();
  g.fillText(t+'°',pad.l-6,y+4);
 }
 g.textAlign='left';

 g.strokeStyle='#cbd5e1';g.lineWidth=1;
 g.beginPath();g.moveTo(pad.l,baseY);g.lineTo(w-pad.r,baseY);g.stroke();

 g.font='10px system-ui,sans-serif';
 for(let i=0;i<=CHART_HOURS;i++){
  const tickTs=t0+i*3600;
  const x=pad.l+pw*(i/CHART_HOURS);
  if(i>0&&i<CHART_HOURS){
   g.strokeStyle='#f3f4f6';g.lineWidth=1;
   g.beginPath();g.moveTo(x,pad.t);g.lineTo(x,baseY);g.stroke();
  }
  g.fillStyle='#dc2626';
  g.beginPath();g.arc(x,baseY,3.5,0,Math.PI*2);g.fill();
  g.fillStyle='#444';g.textAlign='center';
  g.fillText(fmtHour(tickTs),x,baseY+14);
 }
 g.textAlign='left';

 if(!points.length){
  g.fillStyle='#666';g.font='14px system-ui,sans-serif';
  g.fillText('No data in last 12 h',pad.l+8,h/2);
  return;
 }

 g.strokeStyle='#2563eb';g.lineWidth=2;g.beginPath();
 points.forEach((p,i)=>{
  const x=pad.l+pw*Math.min(1,Math.max(0,(p.ts-t0)/xSpan));
  const y=pad.t+ph*(1-(p.temp-yMin)/ySpan);
  if(i===0)g.moveTo(x,y);else g.lineTo(x,y);
 });
 g.stroke();
 points.forEach(p=>{
  const x=pad.l+pw*Math.min(1,Math.max(0,(p.ts-t0)/xSpan));
  const y=pad.t+ph*(1-(p.temp-yMin)/ySpan);
  g.fillStyle='#2563eb';g.beginPath();g.arc(x,y,2.5,0,Math.PI*2);g.fill();
 });
}

function buildReadingsTable(points){
 const tbody=document.getElementById('readings');
 document.getElementById('readings-count').textContent=points.length+' records';
 if(!points.length){
  tbody.innerHTML='<tr><td colspan="3">No data</td></tr>';
  return;
 }
 const rows=[...points].sort((a,b)=>b.ts-a.ts);
 tbody.innerHTML=rows.map(p=>
  '<tr><td>'+fmtTime(p.ts)+'</td><td>'+p.temp.toFixed(1)+'</td><td>'+p.hum.toFixed(1)+'</td></tr>'
 ).join('');
}

async function refresh(){
 try{
  const sr=await fetch('/api/status');
  if(!sr.ok) throw new Error('status '+sr.status);
  const s=await sr.json();
  document.getElementById('temp').textContent=s.sensor?s.temp.toFixed(1)+' °C':'ERR';
  document.getElementById('hum').textContent=s.sensor?s.hum.toFixed(1)+' %':'ERR';
  document.getElementById('bat').textContent=s.battery>=0?s.battery+' %':'--';
  document.getElementById('meta').innerHTML=
    'Clock: <b>'+s.clock+'</b><br>'+
    'WiFi: '+(s.wifi?'OK':'--')+' | NTP: '+(s.timeValid?(s.wifi?'OK':'loc'):'--')+
    ' | SD: '+(s.sd?'OK':'--')+' | Records: '+s.records+'/'+s.recordsMax;
  const hr=await fetch('/api/history');
  if(!hr.ok) throw new Error('history '+hr.status);
  const h=await hr.json();
  const pts=h.points||[];
  drawChart(pts);
  buildReadingsTable(pts);
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
  static TempRecord recs[RECORD_MAX];
  const uint16_t count = recordStoreCopyRecent(RECORD_MAX, recs);

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
