#!/usr/bin/env python3
"""Rotor Raider's pacing from a run's serial log, per theatre, against the
design playbook's targets (section 1.1). The game prints its state every 16
frames: tair / tshots = planes and enemy shots (flak, missiles) on screen,
fired = shots fired at the gunship so far.

    agk run -f tools/pacing.agk -o build/pacing
    python3 tools/pace.py build/pacing/serial.txt

Threat = planes + shots on screen (P1). A rest beat is 3 s or more with no
threat (P6). Only frames in play count (state=1).
"""
import re
import sys

STEP = 16 / 50           # seconds between samples
names = ["Coral Reach", "Ember Flats", "Frostline"]
runs = {}
th = 0
for line in open(sys.argv[1], errors="replace"):
    m = re.search(r"theatre loaded=(\d)", line)
    if m:
        th = int(m.group(1))
        continue
    if "tair=" not in line or "state=1" not in line:
        continue
    d = dict(re.findall(r"(\w+)=(-?\d+)", line))
    runs.setdefault(th, []).append((int(d["frame"]), int(d["tair"]), int(d["tshots"]), int(d["fired"])))


def pct(v, p):
    v = sorted(v)
    return v[min(len(v) - 1, int(p * len(v)))]


print(f"{'theatre':12} {'min':>4} {'threat mean':>11} {'p90':>4} {'max':>4} {'quiet':>6} "
      f"{'shots mean':>10} {'p90':>4} {'fired/min':>9} {'rest beats':>10}")
for t in sorted(runs):
    r = runs[t]
    threat = [a + s for _f, a, s, _x in r]
    shots = [s for _f, _a, s, _x in r]
    minutes = len(r) * STEP / 60
    fired = sum(max(0, r[i + 1][3] - r[i][3]) for i in range(len(r) - 1))
    rests, run = [], 0
    for v in threat + [1]:
        if v == 0:
            run += 1
        else:
            if run * STEP >= 3:
                rests.append(run * STEP)
            run = 0
    print(f"{names[t] if t < 3 else t:12} {minutes:4.1f} {sum(threat) / len(threat):11.2f} {pct(threat, 0.9):4d} "
          f"{max(threat):4d} {100 * threat.count(0) / len(threat):5.0f}% {sum(shots) / len(shots):10.2f} "
          f"{pct(shots, 0.9):4d} {fired / minutes:9.0f} {len(rests):4d} ({', '.join(f'{x:.0f}' for x in rests[:6])}{'...' if len(rests) > 6 else ''} s)")
print("targets (playbook 1.1): threat mean 2.5-3 (first level, +50% a level), p90 6, quiet 8-12%,\n"
      "shots on screen 0.7-1.5 / p90 2.5-5, 30-100 shots a minute, a 4-8 s rest every 60-80 s")
