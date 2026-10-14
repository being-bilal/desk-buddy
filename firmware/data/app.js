/* Desk Buddy control panel - talks to the device over WebSocket. */
(function () {
  'use strict';

  var FACES = [
    { id: 'neutral',   em: '\uD83D\uDE42', label: 'Neutral'   },
    { id: 'happy',     em: '\uD83D\uDE04', label: 'Happy'     },
    { id: 'tired',     em: '\uD83D\uDE34', label: 'Tired'     },
    { id: 'angry',     em: '\uD83D\uDE20', label: 'Angry'     },
    { id: 'surprised', em: '\uD83D\uDE2E', label: 'Surprised' }
  ];

  var $ = function (id) { return document.getElementById(id); };
  var ws = null, retry = null;

  /* ------------------------------ faces ------------------------------ */
  var facesEl = $('faces');
  FACES.forEach(function (f) {
    var b = document.createElement('button');
    b.className = 'face';
    b.dataset.face = f.id;
    b.innerHTML = '<span class="em">' + f.em + '</span><span>' + f.label + '</span>';
    b.addEventListener('click', function () { send({ t: 'face', v: f.id }); });
    facesEl.appendChild(b);
  });

  function send(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj));
  }

  /* ---------------------------- websocket ---------------------------- */
  function connect() {
    var url = (location.protocol === 'https:' ? 'wss://' : 'ws://') +
              location.host + '/ws';
    ws = new WebSocket(url);

    ws.onopen = function () { setStatus(true); send({ t: 'get' }); };
    ws.onclose = function () { setStatus(false); scheduleReconnect(); };
    ws.onerror = function () { try { ws.close(); } catch (e) {} };
    ws.onmessage = function (ev) {
      var m;
      try { m = JSON.parse(ev.data); } catch (e) { return; }
      if (m && m.t === 'state') apply(m);
    };
  }

  function scheduleReconnect() {
    if (retry) return;
    retry = setTimeout(function () { retry = null; connect(); }, 1500);
  }

  function setStatus(on) {
    $('dot').classList.toggle('on', on);
    if (!on) $('status').textContent = 'reconnecting\u2026';
  }

  function apply(s) {
    $('name').textContent = s.name || 'Desk Buddy';
    $('fw').textContent = s.fw ? 'v' + s.fw : '';
    $('ip').textContent = s.ip || '\u2014';
    $('status').textContent = s.ap ? 'setup AP' : 'connected';

    var list = document.querySelectorAll('.face');
    for (var i = 0; i < list.length; i++) {
      list[i].classList.toggle('active', list[i].dataset.face === s.face);
    }

    /* Never stomp on what the user is currently typing or dragging. */
    if (document.activeElement !== msg) {
      msg.value = s.text || '';
      updateCount();
    }
    if (document.activeElement !== vol) {
      vol.value = s.vol;
      $('volv').textContent = s.vol;
    }

    var bits = [
      s.mode === 'text' ? 'showing message' : 'face: ' + s.face,
      s.audio ? (s.playing ? 'playing track ' + s.track : 'audio idle')
              : 'no audio driver',
      Math.round(s.fps || 0) + ' fps',
      'up ' + (s.uptime || 0) + 's'
    ];
    if (!s.storage) bits.push('storage failed');
    $('stat-line').textContent = bits.join(' \u00b7 ');
  }

  /* ----------------------------- message ----------------------------- */
  var msg = $('msg');
  function updateCount() { $('count').textContent = msg.value.length; }
  msg.addEventListener('input', updateCount);

  $('save').addEventListener('click', function () {
    send({ t: 'text', v: msg.value });
    flash($('save'));
  });

  $('show').addEventListener('click', function () {
    send({ t: 'text', v: msg.value });   /* push the latest text first */
    send({ t: 'show' });
    flash($('show'));
  });

  $('hide').addEventListener('click', function () { send({ t: 'hide' }); });

  msg.addEventListener('keydown', function (e) {
    if ((e.metaKey || e.ctrlKey) && e.key === 'Enter') {
      e.preventDefault();
      $('save').click();
    }
  });

  /* ------------------------------ sound ------------------------------ */
  var playButtons = document.querySelectorAll('[data-play]');
  for (var p = 0; p < playButtons.length; p++) {
    (function (btn) {
      btn.addEventListener('click', function () {
        send({ t: 'play', n: parseInt(btn.dataset.play, 10) });
      });
    })(playButtons[p]);
  }
  $('stop').addEventListener('click', function () { send({ t: 'stop' }); });

  var vol = $('vol');
  vol.addEventListener('input', function () { $('volv').textContent = vol.value; });
  vol.addEventListener('change', function () {
    send({ t: 'vol', v: parseInt(vol.value, 10) });
  });

  /* ------------------------------ wifi ------------------------------- */
  $('wifi').addEventListener('submit', function (e) {
    e.preventDefault();
    var ssid = $('ssid').value.trim();
    if (!ssid) return;
    $('status').textContent = 'rebooting\u2026';
    fetch('/api/wifi', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ ssid: ssid, pass: $('pass').value })
    })['catch'](function () {});
  });

  $('reboot').addEventListener('click', function () {
    $('status').textContent = 'rebooting\u2026';
    fetch('/api/reboot', { method: 'POST' })['catch'](function () {});
  });

  function flash(el) {
    var old = el.textContent;
    el.textContent = '\u2713';
    setTimeout(function () { el.textContent = old; }, 900);
  }

  /* ------------------------------ boot ------------------------------- */
  updateCount();
  fetch('/api/wifi')
    .then(function (r) { return r.json(); })
    .then(function (w) { if (w && w.ssid) $('ssid').value = w.ssid; })
    ['catch'](function () {});
  connect();
})();
