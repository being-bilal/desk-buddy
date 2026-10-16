/* Desk Buddy control panel.
   The face grid is built from the list the device reports, so the browser can
   never drift out of sync with the firmware. */
(function () {
  'use strict';

  var $ = function (id) { return document.getElementById(id); };

  /* Presentation only - the authoritative face list comes from the device. */
  /* Pastel fills with soft plum ink - every face now uses dark text, since
     all of these are light tints. */
  var INK = '#4A4458';
  var META = {
    neutral:   { em: '\uD83D\uDE42', label: 'Neutral',   bg: '#E9E7F2', fg: INK },
    happy:     { em: '\uD83D\uDE04', label: 'Happy',     bg: '#FFE9A8', fg: INK },
    excited:   { em: '\uD83E\uDD29', label: 'Excited',   bg: '#FFD8BC', fg: INK },
    love:      { em: '\uD83D\uDE0D', label: 'Love',      bg: '#FFC2D6', fg: INK },
    cool:      { em: '\uD83D\uDE0E', label: 'Cool',      bg: '#BFE7F0', fg: INK },
    wink:      { em: '\uD83D\uDE09', label: 'Wink',      bg: '#DCD0F7', fg: INK },
    tired:     { em: '\uD83D\uDE34', label: 'Tired',     bg: '#D3D6F7', fg: INK },
    sad:       { em: '\uD83D\uDE22', label: 'Sad',       bg: '#C3DCF7', fg: INK },
    angry:     { em: '\uD83D\uDE20', label: 'Angry',     bg: '#FFC7C7', fg: INK },
    surprised: { em: '\uD83D\uDE2E', label: 'Surprised', bg: '#DBF3BC', fg: INK },
    confused:  { em: '\uD83E\uDD14', label: 'Confused',  bg: '#BCEBDD', fg: INK },
    dead:      { em: '\uD83D\uDC80', label: 'Dead',      bg: '#E4E4EC', fg: INK }
  };
  var facesEl = $('faces');
  var msg = $('msg');
  var vol = $('vol');

  var ws = null;
  var retryTimer = null;
  var renderedList = null;

  /* ------------------------------ helpers ------------------------------- */
  function metaFor(name) {
    if (META[name]) return META[name];
    return {
      em: '\u2753',
      label: name.charAt(0).toUpperCase() + name.slice(1),
      bg: '#E9E7F2',
      fg: '#4A4458'
    };
  }

  function send(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj));
  }

  /* ------------------------------- faces -------------------------------- */
  function renderFaces(list) {
    if (renderedList && renderedList.join('|') === list.join('|')) return;
    renderedList = list.slice();

    facesEl.innerHTML = '';
    list.forEach(function (name) {
      var m = metaFor(name);
      var b = document.createElement('button');
      b.className = 'face';
      b.dataset.face = name;
      b.style.background = m.bg;
      b.style.color = m.fg;

      var em = document.createElement('span');
      em.className = 'em';
      em.textContent = m.em;

      var label = document.createElement('span');
      label.textContent = m.label;

      b.appendChild(em);
      b.appendChild(label);
      b.addEventListener('click', function () { send({ t: 'face', v: name }); });
      facesEl.appendChild(b);
    });

    $('faceTotal').textContent = list.length;
    buildTicker(list.length);
  }

  function buildTicker(count) {
    var items = [
      count + ' expressions',
      'tap to switch face',
      'hold for your message',
      'piezo powered',
      'no cloud, all on your LAN',
      'buddy.local'
    ];
    /* Each item carries its own trailing separator, so the track is exactly
       two identical halves and the -50% translation loops seamlessly. */
    var half = items.map(function (t) {
      return '<b>\u2726</b> ' + t + ' &nbsp;&nbsp; ';
    }).join('');
    $('ticker').innerHTML = half + half;
  }

  /* ----------------------------- websocket ------------------------------ */
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
    if (retryTimer) return;
    retryTimer = setTimeout(function () { retryTimer = null; connect(); }, 1500);
  }

  function setStatus(on) {
    $('dot').classList.toggle('on', on);
    $('status').querySelector('b').textContent = on ? 'online' : 'reconnecting';
  }

  function apply(s) {
    $('ip').textContent = s.ip || '\u2014';

    if (s.faceList && s.faceList.length) renderFaces(s.faceList);

    var cards = facesEl.children;
    for (var i = 0; i < cards.length; i++) {
      cards[i].classList.toggle('active', cards[i].dataset.face === s.face);
    }

    /* Never stomp on what the user is typing or dragging. */
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
      s.audio ? (s.playing ? 'melody ' + s.track : 'audio idle') : 'no audio driver',
      Math.round(s.fps || 0) + ' fps',
      'fw ' + (s.fw || '?'),
      'up ' + (s.uptime || 0) + 's'
    ];
    if (!s.storage) bits.push('storage failed');
    $('stat-line').textContent = bits.join(' \u00b7 ');
  }

  /* ------------------------------ message ------------------------------- */
  function updateCount() { $('count').textContent = msg.value.length; }
  msg.addEventListener('input', updateCount);

  $('save').addEventListener('click', function () {
    send({ t: 'text', v: msg.value });
    flash($('save'));
  });

  $('show').addEventListener('click', function () {
    send({ t: 'text', v: msg.value });     /* push the latest text first */
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

  /* ------------------------------- sound -------------------------------- */
  var playButtons = document.querySelectorAll('[data-play]');
  for (var p = 0; p < playButtons.length; p++) {
    (function (btn) {
      btn.addEventListener('click', function () {
        send({ t: 'play', n: parseInt(btn.dataset.play, 10) });
      });
    })(playButtons[p]);
  }
  $('stop').addEventListener('click', function () { send({ t: 'stop' }); });

  vol.addEventListener('input', function () { $('volv').textContent = vol.value; });
  vol.addEventListener('change', function () {
    send({ t: 'vol', v: parseInt(vol.value, 10) });
  });

  /* ------------------------------- wifi --------------------------------- */
  $('wifi').addEventListener('submit', function (e) {
    e.preventDefault();
    var ssid = $('ssid').value.trim();
    if (!ssid) return;
    $('status').querySelector('b').textContent = 'rebooting';
    fetch('/api/wifi', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ ssid: ssid, pass: $('pass').value })
    })['catch'](function () {});
  });

  $('reboot').addEventListener('click', function () {
    $('status').querySelector('b').textContent = 'rebooting';
    fetch('/api/reboot', { method: 'POST' })['catch'](function () {});
  });

  function flash(el) {
    var old = el.textContent;
    el.textContent = '\u2713 saved';
    setTimeout(function () { el.textContent = old; }, 900);
  }

  /* ------------------------------- boot --------------------------------- */
  updateCount();
  buildTicker(12);
  fetch('/api/wifi')
    .then(function (r) { return r.json(); })
    .then(function (w) { if (w && w.ssid) $('ssid').value = w.ssid; })
    ['catch'](function () {});
  connect();
})();
