#!/usr/bin/env python3
"""ROTOR RAIDER's sampled instruments (as in Rotor Rescue), synthesized (no recordings): a brassy
lead, a plucked bass, kick, snare and hi-hat. Writes sound/samples/*.wav for
the MML songs (`#inst NAME samples/FILE.wav root=NOTE`). Kept short: every
sample costs chip RAM.

    python3 sound/tools/make_samples.py
"""
import math
import os
import random
import struct
import wave

RATE = 33148
here = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(here, '..', 'samples')
rnd = random.Random(1982)


def hz(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


def lowpass(x, cutoff, poles=2):
    a = math.exp(-2 * math.pi * cutoff / RATE)
    for _ in range(poles):
        y, prev = [], 0.0
        for v in x:
            prev = (1 - a) * v + a * prev
            y.append(prev)
        x = y
    return x


def highpass(x, cutoff):
    a = math.exp(-2 * math.pi * cutoff / RATE)
    y, px, py = [], 0.0, 0.0
    for v in x:
        py = a * (py + v - px)
        px = v
        y.append(py)
    return y


def envelope(x, attack=0.002, release=0.02):
    n = len(x)
    a, r = int(attack * RATE), int(release * RATE)
    return [v * min(1.0, i / a if a else 1.0) * min(1.0, (n - i) / r if r else 1.0) for i, v in enumerate(x)]


def write(name, x, level=0.95):
    os.makedirs(out, exist_ok=True)
    p = max(abs(v) for v in x) or 1.0
    x = [v * level / p for v in x]
    with wave.open(os.path.join(out, name + '.wav'), 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b''.join(struct.pack('<h', int(v * 32767)) for v in x))
    print(f'{name}: {len(x) / RATE:.2f} s')


# brass lead (D5 = midi 74): two detuned saws, a filter that opens and closes
n, p1, p2, lead = int(0.36 * RATE), 0.0, 0.0, []
for i in range(n):
    t = i / RATE
    vib = 1 + 0.008 * math.sin(2 * math.pi * 5.2 * t) * min(1.0, t / 0.25)
    p1 += hz(74) * vib / RATE
    p2 += hz(74) * 1.006 * vib / RATE
    lead.append((2 * (p1 % 1) - 1) + 0.7 * (2 * (p2 % 1) - 1))
# filter sweep: blend a dark and a bright copy
dark = lowpass(lead, 900)
bright = lowpass(lead, 4200)
mixd = []
for i in range(n):
    t = i / RATE
    k = min(1.0, t / 0.04) * (0.55 + 0.45 * math.exp(-t * 4))
    mixd.append(dark[i] * (1 - k) + bright[i] * k)
write('lead', envelope(mixd, attack=0.012, release=0.12))

# bass pluck (D3 = midi 50): Karplus-Strong
period = RATE / hz(50)        # D3
size = int(period)
buf = [rnd.uniform(-1, 1) for _ in range(size)]
bass = []
for i in range(int(0.32 * RATE)):
    a = buf[i % size]
    b = buf[(i + 1) % size]
    v = (a + b) * 0.5 * 0.994
    buf[i % size] = v
    bass.append(a)
bass = [math.tanh(2.0 * v) for v in lowpass(bass, 1400)]
write('bass', envelope(bass, release=0.08))

# kick
n, ph, kick = int(0.16 * RATE), 0.0, []
for i in range(n):
    t = i / RATE
    ph += (48 + 120 * math.exp(-t * 32)) / RATE
    kick.append(math.tanh(2.0 * math.sin(2 * math.pi * ph) * math.exp(-t * 11)) +
                rnd.uniform(-1, 1) * math.exp(-t * 400) * 0.5)
write('kick', kick)

# snare
n, ph, snare = int(0.16 * RATE), 0.0, []
for i in range(n):
    t = i / RATE
    ph += (190 + 40 * math.exp(-t * 60)) / RATE
    snare.append(0.5 * math.sin(2 * math.pi * ph) * math.exp(-t * 30) +
                 rnd.uniform(-1, 1) * (0.8 * math.exp(-t * 18) + 0.1 * math.exp(-t * 6)))
write('snare', highpass(snare, 160))

# closed hi-hat
hat = [rnd.uniform(-1, 1) * math.exp(-i / RATE * 60) for i in range(int(0.06 * RATE))]
write('hat', highpass(highpass(hat, 5000), 4000), level=0.7)
