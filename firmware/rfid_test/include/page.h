#pragma once
#include <Arduino.h>

// Página captive: fundo escuro, UID grande centralizado, poll /last a cada 1s.
static const char PAGE_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="pt-br">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>RFID Test</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body {
    margin: 0; min-height: 100vh;
    display: flex; flex-direction: column;
    align-items: center; justify-content: center;
    background: #0d1117; color: #e6edf3;
    font-family: system-ui, -apple-system, Segoe UI, Roboto, sans-serif;
    text-align: center; padding: 24px;
  }
  h1 { font-size: 1rem; font-weight: 500; color: #8b949e; letter-spacing: .1em;
       text-transform: uppercase; margin: 0 0 24px; }
  #uid {
    font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
    font-size: clamp(2rem, 12vw, 5rem); font-weight: 700;
    letter-spacing: .08em; word-break: break-all; line-height: 1.1;
    color: #58a6ff; transition: color .15s;
  }
  #uid.empty { color: #30363d; font-size: clamp(1.2rem, 6vw, 2.2rem); }
  #age { margin-top: 18px; font-size: .9rem; color: #8b949e; min-height: 1.2em; }
  #dot { display:inline-block; width:8px; height:8px; border-radius:50%;
         background:#3fb950; margin-right:6px; vertical-align:middle; }
</style>
</head>
<body>
  <h1>Leitor RFID &mdash; ultimo codigo</h1>
  <div id="uid" class="empty">aproxime uma tag</div>
  <div id="age"></div>
<script>
  const uidEl = document.getElementById('uid');
  const ageEl = document.getElementById('age');
  async function tick() {
    try {
      const r = await fetch('/last', { cache: 'no-store' });
      const d = await r.json();
      if (d.uid && d.uid.length) {
        uidEl.textContent = d.uid;
        uidEl.classList.remove('empty');
        const s = (d.age_ms / 1000).toFixed(1);
        ageEl.innerHTML = '<span id="dot"></span>lido ha ' + s + ' s';
      } else {
        uidEl.textContent = 'aproxime uma tag';
        uidEl.classList.add('empty');
        ageEl.textContent = '';
      }
    } catch (e) { /* mantem ultimo estado */ }
  }
  tick();
  setInterval(tick, 1000);
</script>
</body>
</html>
)HTML";
