#!/usr/bin/env python3
"""Build docs/sounds.html: a GitHub Pages player for the factory preset demos in docs/audio.

Usage: python scripts/make-sound-page.py            (run from the repository root)
The preset list is read from the README's preset table, so re-run it after adding presets.
"""
import html
import json
import re
import sys
from pathlib import Path

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")
readme = (root / "README.md").read_text(encoding="utf-8")
row = re.compile(r"^\s*\| (.+?) \| (\w+) \| (.+?) \| \[▶ mp3\]\(docs/(audio/[^)]+)\) \|$", re.M)
presets = []
for i, (name, cat, shows, path) in enumerate(row.findall(readme), 1):
    if not (root / "docs" / path).is_file():
        sys.exit(f"missing {path}")
    presets.append({"n": i, "name": name, "cat": cat, "shows": shows, "src": path, "id": Path(path).stem})
if not presets:
    sys.exit("no presets found in README.md")

cats = []
for p in presets:
    if p["cat"] not in cats:
        cats.append(p["cat"])

data = json.dumps(presets, ensure_ascii=False)
page = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>KMISynth: Sound Library</title>
<link rel="icon" href="data:,">
<style>
  :root {
    --bg: #14161b; --panel: #1d2027; --edge: #2d313b; --control: #262a33;
    --text: #dde1e8; --dim: #8b919d; --accent: #4fc3f7; --warm: #ffb74d; --track: #383d48;
  }
  * { box-sizing: border-box; }
  body { margin: 0; background: var(--bg); color: var(--text);
         font: 15px/1.45 "Segoe UI", system-ui, -apple-system, sans-serif; }
  header { padding: 28px 24px 12px; max-width: 1100px; margin: 0 auto; }
  h1 { margin: 0 0 4px; font-size: 26px; font-weight: 600; letter-spacing: .3px; }
  h1 span { color: var(--accent); }
  .sub { color: var(--dim); margin: 0 0 18px; }
  .sub a { color: var(--accent); }
  .bar { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; }
  input[type=search] { flex: 1 1 240px; background: var(--control); border: 1px solid var(--edge);
         color: var(--text); padding: 9px 12px; border-radius: 8px; font: inherit; }
  input[type=search]:focus { outline: 2px solid var(--accent); outline-offset: -1px; }
  .chips { display: flex; flex-wrap: wrap; gap: 6px; margin-top: 10px; }
  .chip { background: var(--control); border: 1px solid var(--edge); color: var(--dim);
          padding: 4px 11px; border-radius: 999px; cursor: pointer; font: inherit; font-size: 13px; }
  .chip[aria-pressed=true] { background: var(--accent); border-color: var(--accent); color: #0b1a22; }
  main { max-width: 1100px; margin: 0 auto; padding: 8px 24px 48px; }
  h2 { font-size: 15px; text-transform: uppercase; letter-spacing: 1.5px; color: var(--warm);
       margin: 26px 0 10px; font-weight: 600; }
  .grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(330px, 1fr)); gap: 10px; }
  .card { background: var(--panel); border: 1px solid var(--edge); border-radius: 10px;
          padding: 12px 14px; display: flex; gap: 12px; align-items: flex-start; scroll-margin-top: 16px; }
  .card.playing { border-color: var(--accent); box-shadow: 0 0 0 1px var(--accent) inset; }
  .card.target { border-color: var(--warm); }
  .play { flex: none; width: 40px; height: 40px; border-radius: 50%; border: none; cursor: pointer;
          background: var(--control); color: var(--accent); display: grid; place-items: center; }
  .play:hover { background: var(--track); }
  .play:focus-visible { outline: 2px solid var(--accent); }
  .play svg { width: 16px; height: 16px; fill: currentColor; }
  .body { flex: 1; min-width: 0; }
  .name { font-weight: 600; }
  .name small { color: var(--dim); font-weight: 400; margin-left: 6px; }
  .shows { color: var(--dim); font-size: 13.5px; margin-top: 2px; }
  .prog { height: 4px; background: var(--track); border-radius: 2px; margin-top: 9px; cursor: pointer; }
  .prog i { display: block; height: 100%; width: 0; background: var(--accent); border-radius: 2px; }
  .time { color: var(--dim); font-size: 12px; margin-top: 4px; font-variant-numeric: tabular-nums; }
  .card a.dl { color: var(--dim); font-size: 12px; margin-left: 8px; }
  .empty { color: var(--dim); padding: 20px 0; }
  footer { color: var(--dim); text-align: center; font-size: 13px; padding: 0 24px 32px; }
  footer a { color: var(--accent); }
</style>
</head>
<body>
<header>
  <h1><span>KMISynth</span> Sound Library</h1>
  <p class="sub">__COUNT__ factory presets, each rendered straight from the engine. Press play; one plays at a time.
    <a href="https://github.com/KellyMurphy/KMISynth/wiki">Wiki</a> ·
    <a href="https://github.com/KellyMurphy/KMISynth">GitHub</a></p>
  <div class="bar"><input type="search" id="q" placeholder="Search presets, e.g. bell, sync, MPE, wave scan" aria-label="Search presets"></div>
  <div class="chips" id="chips"></div>
</header>
<main id="list"></main>
<footer>Demos are made by <code>kmi_render</code> and <code>scripts/make-audio-demos.ps1</code>. KMISynth is licensed under the AGPL-3.0.</footer>
<script>
const PRESETS = __DATA__;
const CATS = __CATS__;
const PLAY = '<svg viewBox="0 0 16 16"><path d="M4 2.5v11l9.5-5.5z"/></svg>';
const PAUSE = '<svg viewBox="0 0 16 16"><path d="M3.5 2.5h3v11h-3zM9.5 2.5h3v11h-3z"/></svg>';
const audio = new Audio();
audio.preload = "none";
let current = null;      // the preset id that owns the audio element
let activeCat = null;

const esc = s => s.replace(/[&<>"]/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]));
const fmt = t => isFinite(t) ? Math.floor(t / 60) + ":" + String(Math.floor(t % 60)).padStart(2, "0") : "0:00";

function render() {
  const q = document.getElementById("q").value.trim().toLowerCase().split(/\\s+/).filter(Boolean);
  const list = document.getElementById("list");
  let out = "";
  for (const cat of CATS) {
    if (activeCat && cat !== activeCat) continue;
    const items = PRESETS.filter(p => p.cat === cat &&
      q.every(w => (p.name + " " + p.cat + " " + p.shows).toLowerCase().includes(w)));
    if (!items.length) continue;
    out += `<h2 id="cat-${cat.toLowerCase()}">${esc(cat)}</h2><div class="grid">`;
    for (const p of items) {
      const on = current === p.id && !audio.paused;
      out += `<div class="card${on ? " playing" : ""}" id="${p.id}">
        <button class="play" data-id="${p.id}" aria-label="Play ${esc(p.name)}">${on ? PAUSE : PLAY}</button>
        <div class="body">
          <div class="name">${esc(p.name)}<small>#${p.n}</small></div>
          <div class="shows">${esc(p.shows)}</div>
          <div class="prog" data-id="${p.id}"><i></i></div>
          <div class="time"><span class="t">0:00</span><a class="dl" href="${p.src}" download>mp3</a></div>
        </div></div>`;
    }
    out += "</div>";
  }
  list.innerHTML = out || '<p class="empty">No presets match.</p>';
  markTarget();
}

function card(id) { return document.getElementById(id); }

function setUi(id, playing) {
  const c = card(id);
  if (!c) return;
  c.classList.toggle("playing", playing);
  c.querySelector(".play").innerHTML = playing ? PAUSE : PLAY;
}

function toggle(id) {
  const p = PRESETS.find(x => x.id === id);
  if (current === id) {
    audio.paused ? audio.play() : audio.pause();
    return;
  }
  if (current) { setUi(current, false); const c = card(current); if (c) c.querySelector(".prog i").style.width = "0"; }
  current = id;
  audio.src = p.src;
  audio.play();
  history.replaceState(null, "", "#" + id);
}

audio.addEventListener("play", () => setUi(current, true));
audio.addEventListener("pause", () => setUi(current, false));
audio.addEventListener("ended", () => setUi(current, false));
audio.addEventListener("timeupdate", () => {
  const c = card(current);
  if (!c) return;
  c.querySelector(".prog i").style.width = (audio.duration ? 100 * audio.currentTime / audio.duration : 0) + "%";
  c.querySelector(".t").textContent = fmt(audio.currentTime) + " / " + fmt(audio.duration);
});

document.addEventListener("click", e => {
  const b = e.target.closest(".play");
  if (b) return toggle(b.dataset.id);
  const bar = e.target.closest(".prog");
  if (bar && bar.dataset.id === current && audio.duration) {
    const r = bar.getBoundingClientRect();
    audio.currentTime = audio.duration * (e.clientX - r.left) / r.width;
  }
});

function chips() {
  const el = document.getElementById("chips");
  el.innerHTML = ['<button class="chip" data-cat="" aria-pressed="' + (!activeCat) + '">All</button>']
    .concat(CATS.map(c => `<button class="chip" data-cat="${c}" aria-pressed="${activeCat === c}">${c}</button>`)).join("");
  el.onclick = e => {
    const b = e.target.closest(".chip");
    if (!b) return;
    activeCat = b.dataset.cat || null;
    chips(); render();
  };
}

function markTarget() {
  const id = decodeURIComponent(location.hash.slice(1));
  document.querySelectorAll(".card.target").forEach(c => c.classList.remove("target"));
  const c = id && card(id);
  if (c) { c.classList.add("target"); }
}

document.getElementById("q").addEventListener("input", render);
window.addEventListener("hashchange", () => { markTarget(); const c = card(location.hash.slice(1)); if (c) c.scrollIntoView({block: "center"}); });
chips();
render();
const first = card(decodeURIComponent(location.hash.slice(1)));
if (first) first.scrollIntoView({block: "center"});
</script>
</body>
</html>
"""
page = (page.replace("__DATA__", data)
            .replace("__CATS__", json.dumps(cats))
            .replace("__COUNT__", str(len(presets))))
(root / "docs" / "sounds.html").write_text(page, encoding="utf-8")
(root / "docs" / ".nojekyll").write_text("", encoding="utf-8")
print(f"wrote docs/sounds.html with {len(presets)} presets")
