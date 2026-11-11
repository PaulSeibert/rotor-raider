#!/usr/bin/env python3
"""How a ProTracker module uses Paula's four voices: per arrangement position,
the notes on each voice (and the samples), the order jumps, and per voice how
loud it is next to the whole mix and how much of the song it sounds (rendered
with the kit's preview player). Which voice can the rotor borrow, and when?

    python3 sound/tools/mod_voices.py sound/songerson_-_dog70.mod [--wav OUTPREFIX]

--wav also writes 20 s of each voice alone and of the song without each voice
(OUTPREFIX_ch0.wav, OUTPREFIX_without0.wav, ...), to listen to what's lost.

Rotor Raider 1.4 (dog70): voice 0 (strings4 chords) is silent for positions
0-7 (512 rows, 61 s at speed 6 / 125 BPM), then B08 loops positions 8-13 with
all four voices. Voice 2, which the rotor took before 1.4, is the analog
string pad: the loudest voice (-3 dB to the mix), sounding 86% of the song.
"""
import math
import os
import struct
import sys

sys.path.insert(0, os.path.expanduser("~/amiga-game-kit/harness"))
from agk import sound  # noqa: E402

args = sys.argv[1:]
wav = None
if "--wav" in args:
    i = args.index("--wav")
    wav = args[i + 1]
    del args[i:i + 2]
mod = open(args[0], "rb").read()
order = list(mod[952:952 + mod[950]])
npat = max(mod[952:1080]) + 1
P0 = 1084
names = [mod[20 + i * 30:42 + i * 30].split(b"\0")[0].decode("latin1") for i in range(31)]


def cell(pat, row, ch):
    b0, b1, b2, b3 = mod[P0 + pat * 1024 + row * 16 + ch * 4:P0 + pat * 1024 + row * 16 + ch * 4 + 4]
    return (b0 & 0xF0) | (b2 >> 4), ((b0 & 0x0F) << 8) | b1, b2 & 15, b3


print("pos pat   notes per voice   samples")
for oi, pat in enumerate(order):
    n, sm, jumps = [0] * 4, [set() for _ in range(4)], []
    for row in range(64):
        for ch in range(4):
            sno, per, cmd, arg = cell(pat, row, ch)
            if per:
                n[ch] += 1
                sm[ch].add(names[sno - 1] if sno else "-")
            if cmd in (0xB, 0xD, 0xF):
                jumps.append(f"row {row} {cmd:X}{arg:02X}")
    print(f"{oi:3d} {pat:3d}   {n[0]:3d} {n[1]:3d} {n[2]:3d} {n[3]:3d}   "
          f"{[sorted(x) for x in sm]} {' '.join(jumps)}")


def solo(keep):
    m = bytearray(mod)
    for pat in range(npat):
        for row in range(64):
            for ch in set(range(4)) - keep:
                o = P0 + pat * 1024 + row * 16 + ch * 4
                cmd = m[o + 2] & 15
                m[o:o + 2] = b"\0\0"
                m[o + 2] = cmd if cmd in (0xB, 0xD, 0xF) else 0
                if cmd not in (0xB, 0xD, 0xF):
                    m[o + 3] = 0
    return bytes(m)


def rms(x):
    return math.sqrt(sum(v * v for v in x) / max(1, len(x)))


rate = sound.PREVIEW_RATE
full = sound.render_mod(mod, rate=rate)
rf = rms(full)
w = rate // 10
print(f"\nonce through: {len(full) / rate:.1f} s (the preview player ignores B jumps)")
for ch in range(4):
    x = sound.render_mod(solo({ch}), rate=rate)
    windows = range(0, len(x) - w, w)
    on = sum(1 for i in windows if rms(x[i:i + w]) > rf * 0.05)
    print(f"voice {ch} ({'left' if ch in (0, 3) else 'right'}): {20 * math.log10(max(1e-9, rms(x)) / rf):+.1f} dB "
          f"to the mix, sounding {100 * on / max(1, len(windows)):.0f}% of the song")
    if wav:
        sound.write_wav(f"{wav}_ch{ch}.wav", x[:rate * 20], rate)
        sound.write_wav(f"{wav}_without{ch}.wav", sound.render_mod(solo(set(range(4)) - {ch}), rate=rate)[:rate * 20], rate)
