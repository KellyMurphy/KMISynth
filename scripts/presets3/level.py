"""Level-matches the third preset set: renders every factory preset with kmi_render and scales each new
preset's patch level toward the bank's median loudness (-21 dB RMS) with peaks kept under -2 dBFS.
Presets that would need more than the 2.0 patch-level ceiling get a carrier boost (boosts.json).
Writes levels.json; run gen.py afterwards (repeat once: drive and compression are not linear).

    python scripts/presets3/level.py <path-to-kmi_render.exe> <scratch-folder>
"""
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gen  # noqa: E402

TARGET_RMS = -21.0
MAX_PEAK = -2.0
FIRST_NEW = None  # found from the render list


def main():
    exe, scratch = sys.argv[1], sys.argv[2]
    os.makedirs(scratch, exist_ok=True)
    out = subprocess.run([exe, scratch], capture_output=True, text=True).stdout
    rows = {}
    for line in out.splitlines():
        m = re.match(r'^(\d+)\s+(.*\S)\s+(\S+)\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)', line)
        if m:
            rows[int(m.group(1))] = (float(m.group(4)), float(m.group(5)))
    presets = gen.load_presets()
    first = max(rows) + 1 - len(presets)
    path = os.path.join(HERE, 'levels.json')
    levels = json.load(open(path)) if os.path.exists(path) else {}
    cpath = os.path.join(HERE, 'caps.json')  # ceilings for presets that clip when chords stack up
    caps = json.load(open(cpath)) if os.path.exists(cpath) else {}
    bpath = os.path.join(HERE, 'boosts.json')
    boosts = json.load(open(bpath)) if os.path.exists(bpath) else {}
    for i, p in enumerate(presets):
        peak, rms = rows[first + i]
        cur = levels.get(p['name'], 1.0)
        gain_db = min(TARGET_RMS - rms, MAX_PEAK - peak)
        wanted = cur * 10 ** (gain_db / 20.0)
        new = max(0.05, min(caps.get(p['name'], 2.0), wanted))
        levels[p['name']] = round(new, 2)
        flag = ''
        if wanted > 2.0 and p['name'] not in caps:  # past the patch-level ceiling: boost the carriers instead
            boosts[p['name']] = round(min(4.0, boosts.get(p['name'], 1.0) * wanted / 2.0), 2)
            flag = '  (boost %.2f)' % boosts[p['name']]
        print('%-26s peak %6.1f rms %6.1f  level %.2f -> %.2f%s' % (p['name'], peak, rms, cur, new, flag))
    json.dump(levels, open(path, 'w'), indent=1, sort_keys=True)
    json.dump(boosts, open(bpath, 'w'), indent=1, sort_keys=True)


if __name__ == '__main__':
    main()
