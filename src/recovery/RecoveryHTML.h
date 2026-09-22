#pragma once
// RecoveryHTML.h — embedded recovery SPA (PROGMEM)
// Edit this file directly; no quoting/escaping needed (C++11 raw literal).
#include <Arduino.h>

const char RECOVERY_HTML[] PROGMEM = R"raw(
<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport'
content='width=device-width,initial-scale=1'><title>Recovery</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font:14px monospace;background:#1a1a2e;color:#e0e0e0;max-width:680px;margin:0 auto;padding:10px}
h1{color:#ff6600;text-align:center;margin:10px 0}
h3{color:#ff8833;margin:8px 0 4px}
.tabs{display:flex;gap:2px;margin-bottom:10px;flex-wrap:wrap}
.tab{padding:8px 8px;background:#16213e;border:none;color:#e0e0e0;cursor:pointer;border-radius:4px 4px 0 0;flex:1;text-align:center;min-width:60px}
.tab.active{background:#ff6600;color:#000}
.panel{display:none;background:#16213e;padding:12px;border-radius:0 4px 4px 4px}
.panel.active{display:block}
input,button,select{width:100%;padding:8px;margin:6px 0;border:none;border-radius:3px;font:14px monospace}
input{background:#0f3460;color:#e0e0e0}
button{background:#ff6600;color:#000;cursor:pointer;font-weight:bold}
button:hover{background:#ff8833}
button.danger{background:#c33}
button.danger:hover{background:#e44}
.toggleRow{display:flex;gap:16px;margin-bottom:8px;align-items:center}
.toggleRow label{display:flex;align-items:center;gap:4px;cursor:pointer;color:#e0e0e0}
.toggleRow input[type=checkbox]{width:auto;margin:0}
#serialTerm{background:#000;color:#0f0;height:300px;overflow-y:auto;padding:8px;margin:6px 0;border-radius:3px;white-space:pre-wrap;word-break:break-all}
#serialInput{background:#0f3460;color:#0f0}
.status{color:#ff6600;margin:6px 0;min-height:20px}
.progress{width:100%;height:10px;background:#0f3460;border-radius:5px;margin:6px 0;overflow:hidden}
.progress div{height:100%;background:#ff6600;width:0%;transition:width .3s}
.fileItem{display:flex;justify-content:space-between;align-items:center;padding:4px 0;border-bottom:1px solid #0f3460}
.fileItem button{width:auto;padding:4px 10px;margin:0}
.info{color:#888;font-size:12px}
</style></head><body>
<h1>ESP8266 Recovery</h1>
<div class='tabs'>
<button class='tab active' onclick='showTab("firmware")'>Firmware</button>
<button class='tab' onclick='showTab("files")'>Files</button>
<button class='tab' onclick='showTab("serial")'>Serial</button>
<button class='tab' onclick='showTab("wifi")'>WiFi</button>
</div>
<div id='firmware' class='panel active'>
<p>Select a compiled .bin file and upload to update firmware.</p>
<input type='file' id='fwFile' accept='.bin' onchange='uploadFirmware()'>
<div class='progress'><div id='fwProgress'></div></div>
<div id='fwStatus' class='status'></div>
<button onclick='exitRecovery()'>Reboot to Normal</button>
 <div id='exitStatus' class='status'></div>
</div>
<div id='files' class='panel'>
<h3>LittleFS Files</h3>
<div id='fileList'></div>
<button onclick='loadFiles()'>Refresh</button>
</div>
<div id='serial' class='panel'>
<div class='toggleRow'>
<label><input type='checkbox' id='serWired' checked onchange='toggleSerial("wired")'> Wired</label>
<label><input type='checkbox' id='serWireless' onchange='toggleSerial("wireless")'> Wireless</label>
</div>
<div id='serialTerm'></div>
<input id='serialInput' placeholder='Command (help for list) + Enter...'
onkeydown='if(event.key=="Enter"){sendSerial(this.value);this.value=""}'>
<div class='toggleRow'>
<label><input type='checkbox' id='serialAuto' checked onchange='toggleAuto()'> Auto-scroll</label>
</div>
</div>
<div id='wifi' class='panel'>
<h3>Saved WiFi Network</h3>
<div id='wifiInfo' class='info'></div>
<label>SSID</label>
<input id='wifiSSID' placeholder='WiFi network name'>
<label>Password</label>
<input id='wifiPassword' type='password' placeholder='WiFi password'>
<button onclick='saveWifi()'>Save WiFi</button>
<button class='danger' onclick='deleteWifi()'>Delete Saved WiFi</button>
<div id='wifiStatus' class='status'></div>
</div>
<script>
var ws;var activeTab='firmware';var autoScroll=true;
function showTab(t){
activeTab=t;
document.querySelectorAll('.tab').forEach(function(e){e.classList.remove('active')});
event.target.classList.add('active');
document.querySelectorAll('.panel').forEach(function(e){e.classList.remove('active')});
document.getElementById(t).classList.add('active');
if(t==='serial'){connectWS();loadSerialConfig();}
else{if(ws)ws.close()}
if(t==='files')loadFiles();
if(t==='wifi')loadWifi();
}
function loadSerialConfig(){
fetch('/api/serial').then(function(r){return r.json()}).then(function(d){
document.getElementById('serWired').checked=d.wired;
document.getElementById('serWireless').checked=d.wireless;});}
function toggleSerial(t){
var el=document.getElementById(t==='wired'?'serWired':'serWireless');
var body=t+'='+el.checked;
fetch('/api/serial',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body});}
function loadWifi(){
fetch('/api/wifi').then(function(r){return r.json()}).then(function(d){
document.getElementById('wifiSSID').value=d.ssid||'';
document.getElementById('wifiPassword').value=d.password||'';
document.getElementById('wifiInfo').textContent=d.ssid?'Current: '+d.ssid:'No WiFi network saved';});}
function saveWifi(){
var s=document.getElementById('wifiSSID').value;
var p=document.getElementById('wifiPassword').value;
var st=document.getElementById('wifiStatus');
st.textContent='Saving...';
fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
body:'ssid='+encodeURIComponent(s)+'&password='+encodeURIComponent(p)})
.then(function(r){return r.text()}).then(function(t){st.textContent=t;loadWifi();});}
function deleteWifi(){
var st=document.getElementById('wifiStatus');
if(!confirm('Delete saved WiFi network?'))return;
st.textContent='Deleting...';
fetch('/api/wifi',{method:'DELETE'})
.then(function(r){return r.text()}).then(function(t){st.textContent=t;loadWifi();});}
function uploadFirmware(){
var f=document.getElementById('fwFile').files[0];if(!f)return;
var x=new XMLHttpRequest();
x.upload.onprogress=function(e){
document.getElementById('fwProgress').style.width=Math.round(e.loaded/e.total*100)+'%';};
x.onload=function(){document.getElementById('fwStatus').textContent=x.responseText;
if(x.responseText=='OK')document.getElementById('fwStatus').textContent='Success — rebooting...';};
x.onerror=function(){document.getElementById('fwStatus').textContent='Upload error';};
x.open('POST','/firmware');x.send(f);
document.getElementById('fwStatus').textContent='Uploading...';
}
function loadFiles(){
fetch('/api/files').then(function(r){return r.json()}).then(function(d){
var h='';d.files.forEach(function(f){
h+='<div class=fileItem><span>'+f.name+' <span class=info>('+f.size+'B)</span></span>'
+'<button onclick=delFile(\''+f.name+'\')>Delete</button></div>';});
document.getElementById('fileList').innerHTML=h||'<p>No files</p>';});
}
function delFile(n){
var f=new FormData();f.append('name',n);
fetch('/api/files/delete',{method:'POST',body:f}).then(function(){loadFiles()});}
function connectWS(){
if(ws){ws.close()}
ws=new WebSocket('ws://'+location.host+'/ws/serial');
ws.onmessage=function(e){
var t=document.getElementById('serialTerm');t.textContent+=e.data;
if(t.textContent.length>10000)t.textContent=t.textContent.slice(-8000);
if(autoScroll)t.scrollTop=t.scrollHeight;};}
function sendSerial(m){if(ws)ws.send(m+'\r\n');}
function toggleAuto(){autoScroll=document.getElementById('serialAuto').checked;}
function exitRecovery(){
 var s=document.getElementById('exitStatus');
 s.textContent='Rebooting to normal mode...';
 fetch('/api/exit',{method:'POST'})
 .then(function(r){return r.text()})
 .then(function(t){
  if(t==='OK'){s.textContent='Rebooting — please wait...';}
  else{s.textContent='Error: '+t;}
 })
 .catch(function(e){s.textContent='Request failed — device may still reboot';});
}
loadFiles();
</script></body></html>
)raw";
