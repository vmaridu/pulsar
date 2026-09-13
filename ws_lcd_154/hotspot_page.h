/* ===========================================================================
   The setup page's HTML/CSS/JS, as one PROGMEM string.

   Kept in its own header, not inline in hotspot.ino: the Arduino IDE's
   automatic-prototype scanner reads every .ino file as if it were all C++,
   and does not understand C++11 raw string literals — it was misreading the
   JavaScript in here as real function definitions ("function say(...)" etc.)
   and inventing bogus prototypes for them, breaking the whole sketch's build.
   A .h file included via #include is expanded by the compiler afterwards,
   never by that scanner, so its contents are invisible to it.
   =========================================================================== */

static const char PAGE[] PROGMEM = R"PULSARPAGE(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>pulsar setup</title>
<style>
:root{--bg:#05070c;--panel:#0c1422;--tx:#e6edf7;--dim:#7f8fa8;--dim2:#4d5c76;
--rule:#1c2740;--cy:#22d3ee;--gr:#5cf22e;--or:#ff7a00;--rd:#ff2626}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--tx);
font:15px/1.5 ui-sans-serif,system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
padding-bottom:110px}
header{padding:18px 16px 12px;border-bottom:1px solid var(--rule);position:sticky;top:0;
background:var(--bg);z-index:5}
h1{margin:0;font-size:19px;letter-spacing:.14em;text-transform:uppercase;color:var(--cy)}
header span{color:var(--dim);font-size:12px;letter-spacing:.1em}
main{padding:0 16px}
section{margin:22px 0 0;background:var(--panel);border:1px solid var(--rule);border-radius:10px;
padding:14px}
h2{margin:0 0 4px;font-size:12px;letter-spacing:.16em;text-transform:uppercase;color:var(--dim)}
label{display:block;margin:12px 0 0;font-size:12px;letter-spacing:.06em;
text-transform:uppercase;color:var(--dim)}
input,select,textarea{width:100%;margin-top:5px;padding:10px;background:#070b14;
color:var(--tx);border:1px solid var(--rule);border-radius:7px;font-size:16px;
font-family:inherit}
textarea{font-family:ui-monospace,SFMono-Regular,Menlo,monospace;font-size:12px}
input:focus,select:focus,textarea:focus{outline:none;border-color:var(--cy)}
.hint{margin:8px 0 0;font-size:12px;color:var(--dim2);line-height:1.45}
.row{display:flex;align-items:center;gap:9px;text-transform:none;letter-spacing:0;
font-size:13px;color:var(--tx)}
.row input{width:auto;margin:0}
button{font:inherit;font-size:14px;padding:10px 14px;border-radius:7px;cursor:pointer;
background:#101b2d;color:var(--tx);border:1px solid var(--rule)}
button:active{transform:translateY(1px)}
.primary{background:var(--cy);color:#04060a;border-color:var(--cy);font-weight:700}
.net{border:1px solid var(--rule);border-radius:9px;padding:11px;margin:11px 0 0;
background:#070b14}
.netHead{display:flex;align-items:center;gap:7px}
.netHead b{flex:1;font-size:12px;letter-spacing:.12em;color:var(--cy)}
.netHead button{padding:5px 10px;font-size:13px;line-height:1}
.tools{display:flex;gap:9px;margin-top:13px;flex-wrap:wrap}
.seen{margin-top:11px;font-size:13px}
.seen div{display:flex;justify-content:space-between;gap:10px;padding:7px 0;
border-top:1px solid var(--rule);cursor:pointer}
.seen small{color:var(--dim2)}
footer{position:fixed;bottom:0;left:0;right:0;background:var(--bg);
border-top:1px solid var(--rule);padding:12px 16px;display:flex;gap:10px;
align-items:center;flex-wrap:wrap}
footer button{flex:1;min-width:120px}
#msg{margin:0;flex-basis:100%;font-size:13px;color:var(--dim)}
#msg.bad{color:var(--or)}
#msg.good{color:var(--gr)}
.hide{display:none}
details{margin-top:14px}
summary{font-size:12px;letter-spacing:.1em;text-transform:uppercase;color:var(--dim);
cursor:pointer}
</style></head><body>

<header><h1>Pulsar setup</h1><span id="dev">&nbsp;</span></header>

<main>
<section>
  <h2>Endpoint</h2>
  <label>URL
    <input id="url" inputmode="url" autocapitalize="off" autocomplete="off"
           spellcheck="false" placeholder="https://api.example.com/v1/gateway_health"></label>
  <p class="hint">The exact URL this board polls — nothing is added to it, so paste
     the whole thing, path and all.</p>
  <label>API key
    <input id="key" type="password" autocomplete="off" spellcheck="false"></label>
  <label>API secret
    <input id="secret" type="password" autocomplete="off" spellcheck="false"></label>
  <label class="row"><input type="checkbox" id="secretClear"> forget the saved secret</label>
  <p class="hint">Leave the secret empty and the key is sent as
     <code>Authorization: Bearer</code>. Fill it in and the key becomes the public
     <code>X-Api-Key</code> and every request is signed with the secret instead.</p>
  <details>
    <summary>TLS root certificate</summary>
    <textarea id="ca" rows="4" spellcheck="false"
      placeholder="-----BEGIN CERTIFICATE-----"></textarea>
    <label class="row"><input type="checkbox" id="caClear"> forget the saved certificate</label>
    <p class="hint">Optional, and worth doing. Without a root to check against, an
       <code>https://</code> poll is encrypted but not authenticated — anything in the
       middle can read the key. Paste your CA's PEM here and it gets verified.</p>
  </details>
</section>

<section>
  <h2>Sound</h2>
  <label>Mute auto-clears after
    <select id="muteTimeout">
      <option value="5">5 minutes</option>
      <option value="10">10 minutes</option>
      <option value="30">30 minutes</option>
      <option value="60">1 hour</option>
      <option value="360">6 hours</option>
      <option value="720">12 hours</option>
      <option value="1440">24 hours</option>
      <option value="0">Never (only a double-tap or a restart)</option>
    </select></label>
  <p class="hint">Double-tap the right key to mute every sound. It always clears on a
     restart — this is the <em>other</em> way it clears, on its own, so a mute from
     last week can't silence a real critical today.</p>
</section>

<section>
  <h2>Wi-Fi networks</h2>
  <p class="hint">Top of the list wins. The device joins the highest one it can
     actually see, and if none of them are in range it simply keeps trying — that
     is not an error.</p>
  <div id="nets"></div>
  <div class="tools">
    <button id="add">+ add a network</button>
    <button id="scan">scan for networks</button>
  </div>
  <div class="seen" id="seenList"></div>
  <datalist id="seen"></datalist>
</section>
</main>

<footer>
  <button id="save">Save</button>
  <button id="saveboot" class="primary">Save &amp; restart</button>
  <p id="msg">Loading&hellip;</p>
</footer>

<script>
var MAXN = 6, st = {nets: []}, flags = {};
var $ = function(id){ return document.getElementById(id); };

function say(text, cls){
  var m = $('msg');
  m.textContent = text;
  m.className = cls || '';
}

/* ---- one network row. `f` carries which passwords are already stored, so the
   box can say "saved" and stay empty rather than showing what is in there. */
function row(n, i, total){
  var d = document.createElement('div');
  d.className = 'net';
  var entSaved = n.passSet ? 'saved - leave blank to keep' : '';
  var pskSaved = n.pskSet ? 'saved - leave blank to keep' : '';
  var ppSaved  = n.portalPassSet ? 'saved - leave blank to keep' : '';
  d.innerHTML =
    '<div class="netHead"><b>' + (i + 1) + (i === 0 ? ' &middot; tried first' : '') + '</b>' +
      '<button data-a="up" ' + (i === 0 ? 'disabled' : '') + ' title="higher priority">&uarr;</button>' +
      '<button data-a="down" ' + (i === total - 1 ? 'disabled' : '') + ' title="lower priority">&darr;</button>' +
      '<button data-a="del" title="remove">&#10005;</button></div>' +
    '<label>Network name (SSID)<input class="ssid" list="seen" autocapitalize="off" ' +
      'autocomplete="off" spellcheck="false"></label>' +
    '<label>Security<select class="sec">' +
      '<option value="psk">Password (WPA/WPA2)</option>' +
      '<option value="open">Open - no password</option>' +
      '<option value="enterprise">Enterprise (802.1X / PEAP)</option>' +
      '</select></label>' +
    '<label class="f-psk">Wi-Fi password<input class="psk" type="password" ' +
      'autocomplete="off" placeholder="' + pskSaved + '"></label>' +
    '<div class="f-ent">' +
      '<label>Username<input class="user" autocapitalize="off" autocomplete="off" ' +
        'spellcheck="false"></label>' +
      '<label>Password<input class="pass" type="password" autocomplete="off" ' +
        'placeholder="' + entSaved + '"></label>' +
      '<label>Outer identity<input class="identity" autocapitalize="off" ' +
        'autocomplete="off" spellcheck="false" placeholder="optional - defaults to the username">' +
        '</label>' +
      '<p class="hint">802.1X asks for these during the join itself. Without them the ' +
        'device never gets an address at all.</p>' +
    '</div>' +
    '<label class="row"><input type="checkbox" class="portal"> a sign-in page stands ' +
      'between this network and the internet</label>' +
    '<div class="f-portal">' +
      '<label>Sign-in name<input class="puser" autocapitalize="off" autocomplete="off" ' +
        'spellcheck="false"></label>' +
      '<label>Sign-in password<input class="ppass" type="password" autocomplete="off" ' +
        'placeholder="' + ppSaved + '"></label>' +
      '<p class="hint">For the ordinary hotel or guest portal: the device finds the form, ' +
        'fills these in and posts it. A portal that builds its form in JavaScript, or ' +
        'sends you through a separate identity provider, cannot be driven this way - ' +
        'the device will say PORTAL on screen instead of pretending.</p>' +
    '</div>';

  d.querySelector('.ssid').value     = n.ssid || '';
  d.querySelector('.sec').value      = n.security || 'psk';
  d.querySelector('.user').value     = n.user || '';
  d.querySelector('.identity').value = n.identity || '';
  d.querySelector('.portal').checked = !!n.portal;
  d.querySelector('.puser').value    = n.portalUser || '';
  /* Passwords typed this session are put back too. The device never sends one,
     so anything here is something just typed — and adding, removing or
     reordering a row redraws every row, which would otherwise throw away a
     password that was typed a moment ago without saying a word.            */
  d.querySelector('.psk').value      = n.psk || '';
  d.querySelector('.pass').value     = n.pass || '';
  d.querySelector('.ppass').value    = n.portalPass || '';

  function sync(){
    var sec = d.querySelector('.sec').value;
    d.querySelector('.f-psk').classList.toggle('hide', sec !== 'psk');
    d.querySelector('.f-ent').classList.toggle('hide', sec !== 'enterprise');
    d.querySelector('.f-portal').classList.toggle('hide', !d.querySelector('.portal').checked);
  }
  d.querySelector('.sec').addEventListener('change', sync);
  d.querySelector('.portal').addEventListener('change', sync);
  sync();

  d.addEventListener('click', function(e){
    var a = e.target.getAttribute && e.target.getAttribute('data-a');
    if (!a) return;
    e.preventDefault();
    collect();
    if (a === 'del') st.nets.splice(i, 1);
    if (a === 'up' && i > 0) st.nets.splice(i - 1, 0, st.nets.splice(i, 1)[0]);
    if (a === 'down' && i < st.nets.length - 1) st.nets.splice(i + 1, 0, st.nets.splice(i, 1)[0]);
    draw();
  });
  return d;
}

function draw(){
  var box = $('nets');
  box.textContent = '';
  if (!st.nets.length){
    var p = document.createElement('p');
    p.className = 'hint';
    p.textContent = 'No networks yet. Add one, or scan to see what is in range.';
    box.appendChild(p);
  }
  for (var i = 0; i < st.nets.length; i++) box.appendChild(row(st.nets[i], i, st.nets.length));
  $('add').disabled = st.nets.length >= MAXN;
}

/* Read every row back into st.nets, keeping the "already stored" flags so a
   redraw does not forget that a password exists.                           */
function collect(){
  var rows = $('nets').querySelectorAll('.net');
  var out = [];
  for (var i = 0; i < rows.length; i++){
    var d = rows[i], old = st.nets[i] || {};
    out.push({
      ssid:       d.querySelector('.ssid').value.trim(),
      security:   d.querySelector('.sec').value,
      psk:        d.querySelector('.psk').value,
      user:       d.querySelector('.user').value.trim(),
      pass:       d.querySelector('.pass').value,
      identity:   d.querySelector('.identity').value.trim(),
      portal:     d.querySelector('.portal').checked,
      portalUser: d.querySelector('.puser').value.trim(),
      portalPass: d.querySelector('.ppass').value,
      pskSet:        old.pskSet || !!d.querySelector('.psk').value,
      passSet:       old.passSet || !!d.querySelector('.pass').value,
      portalPassSet: old.portalPassSet || !!d.querySelector('.ppass').value
    });
  }
  st.nets = out;
  return out;
}

/* `keep` is set after a save: the fields are refreshed from the device (so the
   "saved" hints on the password boxes become true again) without throwing away
   the message that says the save worked.                                    */
function load(keep){
  fetch('/api/config').then(function(r){ return r.json(); }).then(function(c){
    MAXN = c.maxNets || 6;
    $('dev').textContent = c.device || '';
    $('url').value = c.url || '';
    $('key').placeholder    = c.keySet ? 'saved - leave blank to keep' : '';
    $('secret').placeholder = c.secretSet ? 'saved - leave blank to keep' : 'none - bearer token mode';
    $('ca').placeholder     = c.caSet ? 'saved - leave blank to keep'
                                      : '-----BEGIN CERTIFICATE-----';
    $('muteTimeout').value = String(c.muteTimeoutMin != null ? c.muteTimeoutMin : 30);
    st.nets = c.nets || [];
    draw();
    if (!keep) say(c.url ? 'Pointing at ' + c.url : 'No endpoint set yet.', c.url ? '' : 'bad');
  }).catch(function(e){ say('Could not read the current settings: ' + e, 'bad'); });
}

function save(reboot){
  var nets = collect();
  for (var i = 0; i < nets.length; i++){
    if (!nets[i].ssid){ say('Network #' + (i + 1) + ' has no name.', 'bad'); return; }
    if (nets[i].security === 'psk' && !nets[i].psk && !nets[i].pskSet){
      say('"' + nets[i].ssid + '" needs a password.', 'bad'); return;
    }
    if (nets[i].security === 'enterprise' && !nets[i].user){
      say('"' + nets[i].ssid + '" needs a username.', 'bad'); return;
    }
    if (nets[i].security === 'enterprise' && !nets[i].pass && !nets[i].passSet){
      say('"' + nets[i].ssid + '" needs a password.', 'bad'); return;
    }
  }
  var body = {
    url: $('url').value.trim(),
    key: $('key').value,
    secret: $('secret').value,
    secretClear: $('secretClear').checked,
    ca: $('ca').value.trim(),
    caClear: $('caClear').checked,
    muteTimeoutMin: parseInt($('muteTimeout').value, 10),
    nets: nets
  };
  say('Saving…');
  fetch('/api/config', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(body)
  }).then(function(r){
    return r.json().then(function(j){ return {ok: r.ok, j: j}; });
  }).then(function(res){
    if (!res.ok || !res.j.ok){ say('Not saved: ' + (res.j.error || 'unknown'), 'bad'); return; }
    if (!reboot){
      say('Saved. Restart to use it.', 'good');
      load(true);
      return;
    }
    say('Saved. Restarting… watch the device screen.', 'good');
    fetch('/api/reboot', {method: 'POST'}).catch(function(){});
  }).catch(function(e){ say('Save failed: ' + e, 'bad'); });
}

/* ---- scan. The device answers "still scanning" rather than holding the
   request open, so poll it until the list arrives.                        */
var scanTries = 0;
function scan(){
  fetch('/api/scan').then(function(r){ return r.json(); }).then(function(s){
    if (s.error){ say(s.error, 'bad'); return; }
    if (s.scanning){
      if (++scanTries > 14){ say('The scan is taking too long.', 'bad'); return; }
      say('Scanning…');
      setTimeout(scan, 1200);
      return;
    }
    scanTries = 0;
    var list = s.nets || [];
    list.sort(function(a, b){ return b.rssi - a.rssi; });
    var dl = $('seen'), out = $('seenList');
    dl.textContent = '';
    out.textContent = '';
    for (var i = 0; i < list.length; i++){
      var o = document.createElement('option');
      o.value = list[i].ssid;
      dl.appendChild(o);

      var d = document.createElement('div');
      var bars = list[i].rssi > -60 ? 'strong' : list[i].rssi > -75 ? 'ok' : 'weak';
      d.innerHTML = '<span></span><small>' + list[i].security + ' &middot; ' + bars +
                    ' ' + list[i].rssi + ' dBm</small>';
      d.firstChild.textContent = list[i].ssid || '(hidden)';
      d.addEventListener('click', (function(entry){
        return function(){
          collect();
          var at = -1;
          for (var k = 0; k < st.nets.length; k++) if (st.nets[k].ssid === entry.ssid) at = k;
          if (at < 0 && st.nets.length < MAXN){
            st.nets.push({ssid: entry.ssid, security: entry.security, portal: false});
            draw();
            say('Added "' + entry.ssid + '".', 'good');
          } else if (at < 0){
            say('That is already ' + MAXN + ' networks.', 'bad');
          } else {
            say('"' + entry.ssid + '" is already in the list.', '');
          }
        };
      })(list[i]));
      out.appendChild(d);
    }
    say(list.length + ' network' + (list.length === 1 ? '' : 's') +
        ' in range. Tap one to add it.', '');
  }).catch(function(e){ say('Scan failed: ' + e, 'bad'); });
}

$('add').addEventListener('click', function(){
  collect();
  if (st.nets.length < MAXN) st.nets.push({security: 'psk', portal: false});
  draw();
});
$('scan').addEventListener('click', function(){ scanTries = 0; scan(); });
$('save').addEventListener('click', function(){ save(false); });
$('saveboot').addEventListener('click', function(){ save(true); });
load();
</script>
</body></html>
)PULSARPAGE";
