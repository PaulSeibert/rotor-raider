#!/usr/bin/env python3
"""ROTOR RAIDER's terrain tiles (PF2, 16x16, 8 pens), drawn by rules - no
hand-placed pixels. Writes art/tiles.txt (text art, one frame per tile) and
art/tile_pal.txt (the theatre-1 palette; the copper loads each island band's
own colours at run time, so the pens mean the same thing everywhere):

    0  deep water (COLOR00, behind both playfields)
    1  wave crest (colour-cycled: the sea glitters)
    2  shallow water
    3  surf / foam
    4  beach
    5  land
    6  land, dark (trees, rocks, shadow)
    7  bright (concrete, roofs, lights)

Tile numbers are fixed (logic.h's TILE_* mirror them):
     0-15  coast by corner mask (bit 0 top-left, 1 top-right, 2 bottom-left,
           3 bottom-right = land): 0 open sea, 15 plain land
    16-19  open sea variants (wave crests)
    20-27  land variants (scrub, forest, rocks)
    28-36  factory, intact (3x3, row by row); 37-45 damaged; 46-54 destroyed
    55/56  AA turret / its crater;  57/58  radar station / its crater
    59-66  shipyard (4x2);  67-71  airstrip (5x1, friendly: a helipad first)
    72/73  road east-west / north-south;  74/75  pier east-west / north-south
    76     crater

    python3 art/tools/tiles.py
"""
import math
import os
import random

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.path.join(HERE, '..')
T = 16
PENS = '01234567'
PAL = ['0x026', '0x6CE', '0x29B', '0xDDD', '0xEC8', '0x6A3', '0x352', '0xFFE']
NAMES = ['deep water', 'wave crest', 'shallow', 'surf', 'beach', 'land', 'land dark', 'bright']


def blank(p=0):
    return [[p] * T for _ in range(T)]


def coast(mask, seed):
    """Land field from the four corners (bilinear), plus noise that's zero on
    the tile's edges, so neighbours always meet."""
    rnd = random.Random(seed)
    c = [(mask >> i) & 1 for i in range(4)]       # tl, tr, bl, br
    waves = [(rnd.uniform(0.5, 2.0), rnd.uniform(0.5, 2.0), rnd.uniform(0, 6.28)) for _ in range(3)]
    g = blank()
    for y in range(T):
        for x in range(T):
            u, v = (x + 0.5) / T, (y + 0.5) / T
            f = (c[0] * (1 - u) * (1 - v) + c[1] * u * (1 - v) + c[2] * (1 - u) * v + c[3] * u * v)
            env = math.sin(math.pi * u) * math.sin(math.pi * v)
            n = sum(math.sin(a * x + b * y + p) for a, b, p in waves) / 3
            f += 0.18 * env * n
            if f < 0.22:
                p = 0
            elif f < 0.40:
                p = 2
            elif f < 0.47:
                p = 3
            elif f < 0.58:
                p = 4
            else:
                p = 5
                if f > 0.75 and rnd.random() < 0.12:
                    p = 6
            g[y][x] = p
    return g


def sea(seed):
    """open sea: wave crests, some glittering (pen 1, colour-cycled by the
    copper), some steady foam (pen 3)"""
    rnd = random.Random(seed)
    g = blank()
    for k in range(3):
        x, y, n = rnd.randrange(T - 4), rnd.randrange(T), rnd.randrange(2, 5)
        for i in range(n):
            g[y][x + i] = 3 if k == 0 else 1
    return g


def land(kind, seed):
    rnd = random.Random(seed)
    g = blank(5)
    for y in range(T):
        for x in range(T):
            if rnd.random() < 0.10:
                g[y][x] = 6
    if kind == 'forest':
        for _ in range(5):
            cx, cy, r = rnd.uniform(2, 14), rnd.uniform(2, 14), rnd.uniform(2.0, 3.6)
            for y in range(T):
                for x in range(T):
                    d = math.hypot(x - cx, y - cy)
                    if d < r:
                        g[y][x] = 6
                    if d < r * 0.45 and x < cx and y < cy:
                        g[y][x] = 4          # sunlit crown
    elif kind == 'rocks':
        for _ in range(4):
            cx, cy = rnd.randrange(2, 14), rnd.randrange(2, 14)
            for dy in range(-1, 2):
                for dx in range(-1, 2):
                    g[cy + dy][cx + dx] = 6
            g[cy - 1][cx - 1] = 7
    return g


def canvas(w, h, seed, p=5):
    """land of w x h px with a little texture"""
    rnd = random.Random(seed)
    return [[6 if rnd.random() < 0.08 else p for _ in range(w)] for _ in range(h)]


def fill(g, x0, y0, x1, y1, p):
    for y in range(y0, y1):
        for x in range(x0, x1):
            g[y][x] = p


def box(g, x0, y0, x1, y1, face, edge=6, light=7):
    """a block seen from above, lit from the top-left: light edges top and
    left, shadow right and below"""
    fill(g, x0, y0, x1, y1, face)
    for x in range(x0, x1):
        g[y0][x] = light
    for y in range(y0, y1):
        g[y][x0] = light
        g[y][x1 - 1] = edge
    for x in range(x0, x1):
        g[y1 - 1][x] = edge
    # the cast shadow on the ground, down-right
    for y in range(y0 + 2, min(len(g), y1 + 2)):
        for x in range(x1, min(len(g[0]), x1 + 2)):
            g[y][x] = 6
    for x in range(x0 + 2, min(len(g[0]), x1 + 2)):
        for y in range(y1, min(len(g), y1 + 2)):
            g[y][x] = 6


def disc(g, cx, cy, r, p, rim=None):
    for y in range(len(g)):
        for x in range(len(g[0])):
            d = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
            if d <= r:
                g[y][x] = p
                if rim is not None and d > r - 1.2:
                    g[y][x] = rim


def crater(g, cx, cy, r, seed):
    rnd = random.Random(seed)
    for y in range(len(g)):
        for x in range(len(g[0])):
            d = math.hypot(x + 0.5 - cx, y + 0.5 - cy) + rnd.uniform(-1, 1)
            if d < r * 0.55:
                g[y][x] = 6
            elif d < r and rnd.random() < 0.6:
                g[y][x] = 4 if rnd.random() < 0.5 else 6


def factory(stage):
    g = canvas(48, 48, 400)
    fill(g, 2, 2, 46, 46, 4)                       # the yard
    for x in range(2, 46):
        g[2][x] = g[45][x] = 7
    for y in range(2, 46):
        g[y][2] = g[y][45] = 7
    box(g, 6, 6, 30, 24, 3)                       # the main hall
    for x in range(8, 28, 4):                      # its roof lights
        fill(g, x, 9, x + 2, 21, 7)
    box(g, 6, 28, 20, 40, 3)                       # stores
    disc(g, 36, 12, 5, 6, 7)                       # chimneys
    disc(g, 36, 12, 2.4, 6)
    disc(g, 38, 26, 4, 6, 7)
    disc(g, 38, 26, 1.8, 6)
    box(g, 26, 32, 42, 42, 7)                      # tanks
    for y in range(34, 40):
        g[y][34] = 6
    if stage >= 1:                                 # damaged: holes and scorch
        rnd = random.Random(401)
        for cx, cy, r in ((14, 14, 6), (24, 18, 5), (12, 34, 5), (36, 14, 5)):
            crater(g, cx, cy, r, rnd.randrange(1000))
    if stage >= 2:                                 # destroyed: rubble
        g2 = canvas(48, 48, 402)
        rnd = random.Random(403)
        for _ in range(26):
            cx, cy, r = rnd.uniform(4, 44), rnd.uniform(4, 44), rnd.uniform(3, 7)
            crater(g2, cx, cy, r, rnd.randrange(1000))
        for _ in range(40):
            x, y = rnd.randrange(4, 44), rnd.randrange(4, 44)
            g2[y][x] = 7
        g = g2
    return g


def turret(dead):
    g = canvas(16, 16, 500)
    if dead:
        crater(g, 8, 8, 7, 501)
        return g
    disc(g, 8, 8, 7, 4, 7)
    disc(g, 8, 8, 4, 6, 3)
    return g


def radar(dead):
    g = canvas(16, 16, 510)
    if dead:
        crater(g, 8, 8, 7, 511)
        return g
    box(g, 2, 2, 13, 13, 4)
    disc(g, 7.5, 7.5, 2.5, 6, 7)
    return g


def shipyard():
    g = canvas(64, 32, 520)
    box(g, 1, 1, 62, 30, 4)
    fill(g, 6, 6, 58, 22, 2)                       # the dry dock (water)
    for x in range(6, 58):
        g[6][x] = 6
        g[21][x] = 7
    for x in range(4, 60, 6):                      # crane rails and gantries
        g[3][x] = g[3][x + 1] = 6
        g[26][x] = g[26][x + 1] = 6
    box(g, 20, 2, 24, 28, 7)                       # a gantry crane over the dock
    box(g, 44, 2, 48, 28, 7)
    return g


def airstrip():
    g = canvas(80, 16, 530)
    fill(g, 0, 2, 80, 14, 6)                       # tarmac
    for x in range(0, 80):
        g[2][x] = g[13][x] = 7
    for x in range(22, 78, 8):                     # centreline
        fill(g, x, 7, x + 4, 9, 7)
    disc(g, 8, 8, 6.5, 6, 7)                       # the helipad: an H
    for y in range(4, 12):
        g[y][5] = g[y][10] = 7
    for x in range(5, 11):
        g[8][x] = 7
    return g


def road(vertical):
    g = canvas(16, 16, 540)
    for y in range(16):
        for x in range(5, 11):
            g[y][x] = 4
        g[y][4] = g[y][11] = 6
        if y % 6 < 3:
            g[y][7] = 7
    return [list(r) for r in zip(*g)] if not vertical else g


def pier(vertical):
    g = blank(2)
    for y in range(16):
        for x in range(4, 12):
            g[y][x] = 4 if y % 3 else 6
        g[y][3] = g[y][12] = 6
    return [list(r) for r in zip(*g)] if not vertical else g


def cut(img, tw, th):
    out = []
    for ty in range(th):
        for tx in range(tw):
            out.append([row[tx * T:(tx + 1) * T] for row in img[ty * T:(ty + 1) * T]])
    return out


def tiles():
    out = []
    for m in range(16):
        out.append(coast(m, 100 + m))
    for i in range(4):
        out.append(sea(200 + i))
    for i, k in enumerate(['scrub', 'scrub', 'scrub', 'forest', 'forest', 'forest', 'rocks', 'rocks']):
        out.append(land(k, 300 + i))
    for stage in range(3):
        out += cut(factory(stage), 3, 3)
    out += [turret(0), turret(1), radar(0), radar(1)]
    out += cut(shipyard(), 4, 2)
    out += cut(airstrip(), 5, 1)
    out += [road(0), road(1), pier(0), pier(1)]
    c = canvas(16, 16, 550)
    crater(c, 8, 8, 7, 551)
    out.append(c)
    assert len(out) == 77, len(out)
    return out


def main():
    ts = tiles()
    with open(os.path.join(ART, 'tile_pal.txt'), 'w') as f:
        f.write('# PF2 terrain pens (colours 8-15 on screen); theatre 1, band 1. The copper\n'
                '# loads each island band\'s own set (art/tools/theatres.py).\n')
        for i, (c, n) in enumerate(zip(PAL, NAMES)):
            f.write(f'{i}  {c}  {"transparent" if i == 0 else n}\n')
    with open(os.path.join(ART, 'tiles.txt'), 'w') as f:
        f.write('# Terrain tiles 16x16 (PF2), one frame per tile: see art/tools/tiles.py\n')
        f.write('# (drawn by that script - edit it, not this)\ncolors\n  .  transparent\n')
        for i in range(1, 8):
            f.write(f'  {PENS[i]}  {PAL[i]}\n')
        for g in ts:
            f.write('frame\n')
            for row in g:
                f.write(''.join('.' if p == 0 else PENS[p] for p in row) + '\n')
    # for Tiled (art/maps/*.tmj): the tiles as one image, 8 a row, and the tileset
    import struct
    import zlib
    cols, rgb = 8, [tuple(int(c[i], 16) * 17 for i in (2, 3, 4)) for c in PAL]
    rows_n = (len(ts) + cols - 1) // cols
    img = [[rgb[0]] * (cols * T) for _ in range(rows_n * T)]
    for i, g in enumerate(ts):
        for y in range(T):
            for x in range(T):
                img[(i // cols) * T + y][(i % cols) * T + x] = rgb[g[y][x]]

    def chunk(tag, d):
        return struct.pack('>I', len(d)) + tag + d + struct.pack('>I', zlib.crc32(tag + d))
    raw = b''.join(b'\0' + bytes(v for p in r for v in p) for r in img)
    with open(os.path.join(ART, 'tiles.png'), 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', cols * T, rows_n * T, 8, 2, 0, 0, 0)) +
                chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))
    with open(os.path.join(ART, 'tiles.tsx'), 'w') as f:
        f.write(f'<?xml version="1.0" encoding="UTF-8"?>\n<tileset version="1.10" name="tiles" tilewidth="16" '
                f'tileheight="16" tilecount="{len(ts)}" columns="{cols}">\n'
                f' <image source="tiles.png" width="{cols * T}" height="{rows_n * T}"/>\n</tileset>\n')
    print(f'{len(ts)} tiles')


if __name__ == '__main__':
    main()
