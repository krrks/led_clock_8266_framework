// LED Clock Web UI — vanilla JS SPA
var dashWs, serialWs, configSchema, autoScroll = true;

// ── Tab navigation ────────────────────────────────────────────────────
function showTab(name) {
  document.querySelectorAll('.tab').forEach(function(e) { e.classList.remove('active'); });
  document.querySelectorAll('.nav-btn').forEach(function(e) { e.classList.remove('active'); });
  var tab = document.getElementById(name);
  if (tab) tab.classList.add('active');
  var btn = document.querySelector('[onclick="showTab(\'' + name + '\')"]');
  if (btn) btn.classList.add('active');
  if (name === 'serial') connectSerialWS();
  if (name === 'settings') loadSettings();
  if (name === 'pins') loadPins();
  if (name === 'dashboard') connectDashWS();
}

// ── System info bar ────────────────────────────────────────────────────
function updateSysInfo() {
  fetch('/api/status').then(function(r) { return r.json(); }).then(function(d) {
    document.getElementById('sysInfo').textContent =
      'Heap: ' + d.heap + ' | Uptime: ' + d.uptime + 's | Flash: ' + (d.flashSize / 1048576).toFixed(1) + 'MB';
  });
}
setInterval(updateSysInfo, 10000);
updateSysInfo();

// ── Dashboard WebSocket ────────────────────────────────────────────────
function connectDashWS() {
  if (dashWs) dashWs.close();
  dashWs = new WebSocket('ws://' + location.host + '/ws/dashboard');
  dashWs.onmessage = function(e) {
    var d = JSON.parse(e.data);
    var el = document.getElementById('dashTime'); if (el) el.textContent = d.currentTime || '--:--:--';
    el = document.getElementById('dashDate'); if (el) el.textContent = d.currentDate || '----';
    el = document.getElementById('dashWeekday'); if (el) el.textContent = d.weekday || '---';
    el = document.getElementById('dashNtp'); if (el) el.textContent = d.ntpSynced ? 'Synced' : 'Manual';
    el = document.getElementById('dashTemp'); if (el) el.textContent = (d.temperature || 0).toFixed(1) + 'C';
    el = document.getElementById('dashWeather'); if (el) el.textContent = d.weatherDesc || 'N/A';
    el = document.getElementById('dashHeap'); if (el) el.textContent = d.freeHeap + ' B';
    el = document.getElementById('dashUptime'); if (el) el.textContent = d.uptime + ' s';
  };
}

// ── Settings ───────────────────────────────────────────────────────────
function loadSettings() {
  fetch('/api/config').then(function(r) { return r.json(); }).then(function(d) {
    configSchema = d;
    var h = '';
    for (var k in d) {
      var v = d[k];
      var label = k.replace(/([A-Z])/g, ' $1').replace(/^./, function(s) { return s.toUpperCase(); });
      var input;
      if (typeof v === 'boolean') {
        input = '<input type="checkbox" id="cfg_' + k + '" ' + (v ? 'checked' : '') + '>';
      } else if (typeof v === 'number') {
        input = '<input type="number" id="cfg_' + k + '" value="' + v + '">';
      } else {
        input = '<input type="text" id="cfg_' + k + '" value="' + (typeof v === 'string' ? v.replace(/"/g, '&quot;').replace(/</g, '&lt;') : v) + '">';
      }
      h += '<div class="setting-row"><label>' + label + '</label>' + input + '</div>';
    }
    document.getElementById('settingsForm').innerHTML = h;
  });
}

function saveConfig() {
  var cfg = {};
  var els = document.getElementById('settingsForm').querySelectorAll('input');
  els.forEach(function(el) {
    var key = el.id.replace('cfg_', '');
    if (el.type === 'checkbox') cfg[key] = el.checked;
    else if (el.type === 'number') cfg[key] = Number(el.value);
    else cfg[key] = el.value;
  });
  fetch('/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(cfg)
  }).then(function(r) { return r.text(); }).then(function(t) {
    document.getElementById('settingsStatus').textContent = t;
  });
}

// ── Pin settings ───────────────────────────────────────────────────────
function loadPins() {
  fetch('/api/pins').then(function(r) { return r.json(); }).then(function(d) {
    var h = '';
    d.pins.forEach(function(p) {
      h += '<tr><td>GPIO' + p.gpio + '</td><td>' + p.name + '</td><td>' +
           p.description + '</td><td>' + p.mode + '</td></tr>';
    });
    document.getElementById('pinTableBody').innerHTML = h;
  });
}

// ── Serial Monitor WebSocket ──────────────────────────────────────────
function connectSerialWS() {
  if (serialWs) serialWs.close();
  serialWs = new WebSocket('ws://' + location.host + '/ws/serial');
  serialWs.onmessage = function(e) {
    var t = document.getElementById('serialTerm');
    t.textContent += e.data;
    if (t.textContent.length > 10000) t.textContent = t.textContent.slice(-8000);
    if (autoScroll) t.scrollTop = t.scrollHeight;
  };
  serialWs.onclose = function() { setTimeout(connectSerialWS, 3000); };
}

document.getElementById('serialInput').addEventListener('keydown', function(e) {
  if (e.key === 'Enter') {
    if (serialWs && serialWs.readyState === WebSocket.OPEN) {
      serialWs.send(this.value + '\r\n');
    }
    this.value = '';
  }
});

// ── Init ───────────────────────────────────────────────────────────────
connectDashWS();
