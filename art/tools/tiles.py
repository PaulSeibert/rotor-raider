#!/usr/bin/env python3
"""ROTOR RAIDER's terrain tiles (PF2, 16x16, 8 pens), drawn by rules (art pilot 2026: lit
from the top-left, clustered crowns with cast shadows, banded and dithered coasts that sample
the land and sea tiles by coordinate, a hand-built factory). Same pens and the same 77 tile
numbers as before (logic.h's TILE_*):

    0  deep water (COLOR00, behind both playfields)   4  beach / sunlit / roads
    1  wave crest (colour-cycled: the sea glitters)    5  land
    2  shallow water                                   6  land, dark (shadow, canopy, eaves)
    3  surf / concrete / stone                         7  bright (lit edges, roofs)

     0-15  coast by corner mask (bit 0 top-left .. 3 bottom-right = land): 0 open sea, 15 plain land
    16-19  open sea;  20-27 land (3 grass, 3 forest, 2 rocks);  28-36 / 37-45 / 46-54 factory
    55/56 AA turret pit / crater;  57/58 radar / crater;  59-66 shipyard 4x2;  67-71 airstrip 5x1
    72/73 road east-west / north-south;  74/75 pier east-west / north-south;  76 crater

    python3 art/tools/tiles.py
"""
import math
import os
import random
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.path.join(HERE, '..')

T = 16


def grid(p=5):
    return [[p] * T for _ in range(T)]


def wrap_put(g, x, y, p):
    g[y % T][x % T] = p


def crown(g, cx, cy, r, wrap=True, rnd=None):
    """a tree crown / bush: dark rim, lit top-left, cast shadow down-right (pen 6)"""
    pts = []
    R = int(r) + 2
    for dy in range(-R, R + 2):
        for dx in range(-R, R + 2):
            x, y = cx + dx, cy + dy
            d = math.hypot(dx, dy)
            ds = math.hypot(dx - 1.2, dy - 1.2)
            if d <= r:
                l = -(dx * 0.6 + dy * 0.8) / max(r, 1)
                p = 4 if (l > 0.45 and d < r - 0.6) else 5 if l > -0.1 else 6
                if d > r - 0.7 and l <= 0.2:
                    p = 6
                pts.append((x, y, p))
            elif ds <= r:
                pts.append((x, y, 6))           # the shadow on the ground
    for x, y, p in sorted(pts, key=lambda t: 0 if t[2] == 6 and True else 1):
        if wrap:
            wrap_put(g, x, y, p)
        elif 0 <= x < T and 0 <= y < T:
            g[y][x] = p


def land_base(seed=1):
    rnd = random.Random(seed)
    g = grid(5)
    # grass grain: short dark strokes in clusters, never single noise pixels
    for _ in range(4):
        x, y = rnd.randrange(T), rnd.randrange(T)
        wrap_put(g, x, y, 6); wrap_put(g, x + 1, y, 6)
        if rnd.random() < 0.5:
            wrap_put(g, x + 1, y - 1, 4)
    crown(g, rnd.randrange(T), rnd.randrange(T), 1.2, rnd=rnd)
    return g


def land_variant(base, seed, n=2, r=(1.4, 2.2)):
    rnd = random.Random(seed)
    g = [row[:] for row in base]
    for _ in range(n):
        crown(g, rnd.randrange(4, 12), rnd.randrange(4, 11), rnd.uniform(*r), wrap=False, rnd=rnd)
    return g


def forest(seed):
    rnd = random.Random(seed)
    g = [row[:] for row in LAND]
    # crowns on a jittered 3x3 lattice (wrapping): dense in the middle, gaps of grass at
    # the edges, so a forest tile next to a land tile has no hard square edge
    cs = [(i * 8 + rnd.randrange(0, 4), j * 8 + rnd.randrange(0, 4), rnd.uniform(3.0, 3.8))
          for j in range(2) for i in range(2)] + [(rnd.randrange(T), rnd.randrange(T), 2.2)]
    for cx, cy, r in cs:
        crown(g, cx, cy, r)
    return g


def rocks(base, seed):
    """two or three boulders: dark rim, grey body (pen 3), bright cap top-left, shadow down-right"""
    rnd = random.Random(seed)
    g = [row[:] for row in base]
    spots = [(5, 5, 2.6), (10, 9, 2.0), (5, 11, 1.6)]
    for cx, cy, r in spots[:rnd.choice((2, 3))]:
        cx += rnd.uniform(-0.5, 0.5); cy += rnd.uniform(-0.5, 0.5)
        for y in range(T):
            for x in range(T):
                dx, dy = x + 0.5 - cx, y + 0.5 - cy
                d = math.hypot(dx, dy); ds = math.hypot(dx - 1.3, dy - 1.3)
                if d <= r:
                    l = -(dx * 0.6 + dy * 0.8) / r
                    g[y][x] = 6 if d > r - 0.8 and l < 0.3 else 7 if l > 0.75 else 4
                elif ds <= r and g[y][x] not in (4, 7):
                    g[y][x] = 6
    return g


def sea(seed, crests=3):
    rnd = random.Random(seed)
    g = grid(0)
    for k in range(crests):
        x, y = rnd.randrange(T), rnd.randrange(T)
        n = rnd.choice((3, 4))
        p = 1                      # open sea: pens 0 and 1 only (shared by every band: the copper switch lines lie in it)
        wrap_put(g, x, y + 1, p)
        for i in range(1, n - 1):
            wrap_put(g, x + i, y, p)
        wrap_put(g, x + n - 1, y + 1, p)
    return g


LAND = land_base(7)
SEA = sea(200, 2)


def coast(mask, seed):
    """corner-mask coast (bit 0 tl, 1 tr, 2 bl, 3 br = land), bands deep > shallow (dithered) >
    surf (broken foam) > beach > land; land and sea pixels sample the base tiles at the same
    coordinates so every coast tile meets every land / sea tile seamlessly."""
    rnd = random.Random(seed)
    c = [(mask >> i) & 1 for i in range(4)]
    waves = [(rnd.uniform(0.6, 1.6), rnd.uniform(0.6, 1.6), rnd.uniform(0, 6.28)) for _ in range(3)]
    g = grid(0)
    for y in range(T):
        for x in range(T):
            u, v = (x + 0.5) / T, (y + 0.5) / T
            f = c[0] * (1 - u) * (1 - v) + c[1] * u * (1 - v) + c[2] * (1 - u) * v + c[3] * u * v
            env = math.sin(math.pi * u) * math.sin(math.pi * v)
            f += 0.16 * env * sum(math.sin(a * x + b * y + p) for a, b, p in waves) / 3
            if f < 0.17:
                p = SEA[y][x]
            elif f < 0.22:
                p = 2 if (x + y) % 2 == 0 else 0          # dithered drop-off into the deep
            elif f < 0.37:
                p = 2
            elif f < 0.44:
                p = 3 if ((x * 3 + y * 5) % 7) else 2      # broken foam line
            elif f < 0.47:
                p = 3
            elif f < 0.57:
                p = 4
            elif f < 0.62:
                p = 4 if (x + y) % 2 else LAND[y][x]       # dithered beach edge
            else:
                p = LAND[y][x]
            g[y][x] = p
    return g


# ------------------------------------------------------------------ factory (from the AI raw)
def factory_from_raw():
    """hand-built (layout after the rd_pro factory4 thumbnail): a fenced concrete yard, a long
    hall with a corrugated roof, a sawtooth shed, two stacks, three tanks, crates; lit from the
    top-left, 2-px cast shadows down-right, the land ring outside sampled from the land tile"""
    g = [[LAND[y % T][x % T] for x in range(48)] for y in range(48)]
    def rect(x0, y0, x1, y1, p):
        for y in range(y0, y1):
            for x in range(x0, x1):
                g[y][x] = p
    def shadow(x0, y0, x1, y1, d=2):
        for y in range(y0 + d, y1 + d):
            for x in range(x0 + d, x1 + d):
                if 0 <= x < 48 and 0 <= y < 48 and not (x0 <= x < x1 and y0 <= y < y1):
                    g[y][x] = 6
    def disc(cx, cy, r, body=3, rim=6, hole=None):
        for y in range(48):
            for x in range(48):
                dx, dy = x + 0.5 - cx, y + 0.5 - cy
                d = math.hypot(dx, dy)
                if d <= r:
                    l = -(dx * 0.6 + dy * 0.8) / r
                    g[y][x] = rim if (d > r - 0.9 and l < 0.2) else 7 if l > 0.5 else body
                    if hole and d < hole:
                        g[y][x] = 6
                elif math.hypot(dx - 1.5, dy - 1.5) <= r and g[y][x] not in (3, 7):
                    g[y][x] = 6
    # the yard
    rect(2, 2, 46, 46, 4)
    for i in range(2, 46):
        g[2][i] = g[i][2] = 7
        g[45][i] = g[i][45] = 6
    for i in range(4, 44, 3):            # fence posts' shadows
        g[46][i] = g[i][46] = 6
    # the main hall: corrugated roof (rows of light/mid), dark eaves
    shadow(5, 5, 33, 22); rect(5, 5, 33, 22, 3)
    for y in range(6, 21):
        for x in range(6, 32):
            g[y][x] = 7 if (y % 2 == 0) else 3
    for x in range(5, 33):
        g[5][x] = 7; g[21][x] = 6
    for y in range(5, 22):
        g[y][5] = 7; g[y][32] = 6
    for x in range(6, 32):               # the ridge
        g[13][x] = 6 if x % 4 else 3
    # the sawtooth shed
    shadow(6, 27, 22, 41); rect(6, 27, 22, 41, 3)
    for y in range(27, 41):
        for x in range(6, 22):
            k = (x - 6) % 4
            g[y][x] = 7 if k == 0 else 3 if k < 3 else 6
    for x in range(6, 22):
        g[27][x] = 7; g[40][x] = 6
    # stacks and tanks
    disc(39, 9, 4.2, hole=1.8); disc(39, 19, 3.4, hole=1.4)
    for cx, cy in ((29, 30), (37, 30), (33, 38)):
        disc(cx, cy, 3.3)
    # crates by the gate (south)
    for x0 in (24, 27):
        rect(x0, 41, x0 + 2, 43, 7); g[42][x0 + 1] = 6
    for x in range(22, 26):              # the gate in the south wall
        g[45][x] = 4
    return g


def crater(g, cx, cy, r, seed):
    rnd = random.Random(seed)
    for y in range(len(g)):
        for x in range(len(g[0])):
            d = math.hypot(x + 0.5 - cx, y + 0.5 - cy) + rnd.uniform(-0.8, 0.8)
            if d < r * 0.45:
                g[y][x] = 6
            elif d < r * 0.7:
                g[y][x] = 4 if (x - cx) + (y - cy) > 0 else 6     # lit far rim (down-right)
            elif d < r and rnd.random() < 0.5:
                g[y][x] = 6 if rnd.random() < 0.6 else 3


def factory_stages():
    f0 = factory_from_raw()
    f1 = [r[:] for r in f0]
    for i, (cx, cy, r) in enumerate(((14, 16, 6), (30, 12, 5), (22, 34, 6), (38, 30, 4))):
        crater(f1, cx, cy, r, 40 + i)
    f2 = [r[:] for r in f0]
    rnd = random.Random(9)
    for i in range(22):
        crater(f2, rnd.uniform(4, 44), rnd.uniform(4, 44), rnd.uniform(3.5, 7), 60 + i)
    for _ in range(30):
        x, y = rnd.randrange(3, 45), rnd.randrange(3, 45)
        f2[y][x] = 7
    return f0, f1, f2


def cut(img, tw, th):
    return [[row[tx * T:(tx + 1) * T] for row in img[ty * T:(ty + 1) * T]] for ty in range(th) for tx in range(tw)]


def from_current(n):
    """a tile from the game's current kit (unchanged in the pilot), as pens"""
    _, frames = read_txt(GAME + '/art/tiles.txt')
    return [[0 if ch == '.' else int(ch) for ch in row] for row in frames[n]]




def _land(w, h):
    return [[LAND[y % T][x % T] for x in range(w)] for y in range(h)]


def _shadow_box(g, x0, y0, x1, y1, d=2):
    for y in range(y0 + d, min(len(g), y1 + d)):
        for x in range(x0 + d, min(len(g[0]), x1 + d)):
            if not (x0 <= x < x1 and y0 <= y < y1):
                g[y][x] = 6


def _block(g, x0, y0, x1, y1, face, shadow=True):
    if shadow:
        _shadow_box(g, x0, y0, x1, y1)
    for y in range(y0, y1):
        for x in range(x0, x1):
            g[y][x] = face
    for x in range(x0, x1):
        g[y0][x] = 7; g[y1 - 1][x] = 6
    for y in range(y0, y1):
        g[y][x0] = 7; g[y][x1 - 1] = 6


def turret_pit(dead):
    g = _land(16, 16)
    if dead:
        crater(g, 8, 8, 7, 501)
        for a in range(0, 360, 60):                      # scattered bags
            x = int(8 + math.cos(math.radians(a + 20)) * 6.5); y = int(8 + math.sin(math.radians(a + 20)) * 6.5)
            if 0 <= x < 15 and 0 <= y < 15:
                g[y][x] = 4; g[y][x + 1] = 6
        return g
    for y in range(16):
        for x in range(16):
            dx, dy = x + 0.5 - 8, y + 0.5 - 8
            d = math.hypot(dx, dy); a = math.atan2(dy, dx)
            ds = math.hypot(dx - 1, dy - 1)
            if d <= 5.4:
                g[y][x] = 3                              # the concrete floor (under the gun sprite)
            elif d <= 7.7:
                seam = (a * 10 / math.pi + (0.5 if d > 6.6 else 0)) % 2.0
                lit = -(dx * 0.6 + dy * 0.8) / d
                g[y][x] = 6 if seam < 0.35 else (7 if lit > 0.55 and seam > 1.3 else 4)
                if d > 7.0 and lit < -0.3:
                    g[y][x] = 6                          # the ring's shaded outer face
            elif ds <= 7.9:
                g[y][x] = 6                              # cast shadow, down-right
    return g


def radar_tile(dead):
    g = _land(16, 16)
    if dead:
        crater(g, 8, 8, 7, 511)
        return g
    _block(g, 1, 1, 14, 14, 3)
    for y in range(16):
        for x in range(16):
            dx, dy = x + 0.5 - 7.5, y + 0.5 - 7.5
            d = math.hypot(dx, dy)
            if d <= 4.6:
                far = (dx * 0.6 + dy * 0.8) / max(d, 0.1)    # concave dish: its far (down-right) inside is lit
                if d > 3.7:
                    g[y][x] = 7 if far < -0.2 else 6        # rim: lit top-left lip, shaded far lip
                else:
                    g[y][x] = 4 if far > 0.25 else 3 if d > 1.2 else 6
    g[7][7] = 6; g[6][6] = 7
    return g


def crater_tile():
    g = _land(16, 16)
    crater(g, 8, 8, 7, 551)
    return g


def shipyard_tiles():
    g = _land(64, 32)
    _block(g, 1, 1, 63, 31, 3)
    for y in range(7, 23):                               # the dry dock: water
        for x in range(6, 58):
            g[y][x] = 2
    for x in range(6, 58):
        g[7][x] = 6; g[22][x] = 7                        # shaded north wall, lit south wall
    for y in range(7, 23):
        g[y][6] = 6; g[y][57] = 7
    for y in range(10, 20):                              # a hull on the slip: keel, ribs, pointed bow
        for x in range(12, 52):
            half = 4.5 if x < 44 else 4.5 * (1 - (x - 44) / 8)
            d = abs(y + 0.5 - 15)
            if d <= half:
                g[y][x] = 3 if (d > half - 1.2 or d < 0.6) else (6 if x % 3 == 0 else 2)
    for x0 in (20, 42):                                  # two gantry cranes across it
        _shadow_box(g, x0, 2, x0 + 3, 29, 1)
        for y in range(2, 29):
            g[y][x0] = 7; g[y][x0 + 1] = 4; g[y][x0 + 2] = 6
    for x in range(3, 60, 4):                            # rails
        g[4][x] = g[4][x + 1] = 6
        g[26][x] = g[26][x + 1] = 6
    _block(g, 58, 3, 62, 9, 4, shadow=False)             # a shed
    return cut(g, 4, 2)


def airstrip_tiles():
    g = _land(80, 16)
    for y in range(2, 14):
        for x in range(80):
            g[y][x] = 3                                  # concrete runway
    for x in range(80):
        g[2][x] = 7; g[13][x] = 6; g[14][x] = 6          # lit north edge, shaded south edge + shadow
    for x in range(22, 78, 8):                           # centreline
        for xx in range(x, x + 4):
            g[8][xx] = 6
    for y in range(16):                                  # the helipad (same place: x 8): a ring and an H
        for x in range(16):
            d = math.hypot(x + 0.5 - 8, y + 0.5 - 8)
            if 5.0 <= d <= 6.4:
                g[y][x] = 4
    for y in range(5, 11):
        g[y][5] = g[y][10] = 6
    for x in range(5, 11):
        g[8][x] = 6
    return cut(g, 5, 1)


def road_tile(vertical):
    g = _land(16, 16)
    for y in range(16):
        for x in range(5, 11):
            g[y][x] = 4
        g[y][4] = 6; g[y][11] = 6
        if y % 6 < 3:
            g[y][7] = 3
    return g if vertical else [list(r) for r in zip(*g)]


def pier_tile(vertical):
    g = [[2] * 16 for _ in range(16)]
    for y in range(16):
        for x in range(5, 12):
            g[y][x] = 4 if y % 3 else 6                  # planks
        g[y][4] = 7; g[y][12] = 6                        # lit / shaded rails
        g[y][13] = 0                                     # its shadow on the water
    for y in (1, 8, 15):
        g[y][3] = 6; g[y][13] = 6                        # pilings
    return g if vertical else [list(r) for r in zip(*g)]


def full_kit():
    out = []
    for m in range(16):
        out.append([[0] * T for _ in range(T)] if m == 0 else (LAND if m == 15 else coast(m, 100 + m)))
    for i in range(4):
        out.append(sea(200 + i, 2 + (i % 2)))
    out += [LAND, land_variant(LAND, 301, 1), land_variant(LAND, 302, 2),
            forest(303), forest(304), forest(305), rocks(LAND, 306), rocks(LAND, 307)]
    for f in factory_stages():
        out += cut(f, 3, 3)
    out += [turret_pit(0), turret_pit(1), radar_tile(0), radar_tile(1)]
    out += shipyard_tiles()
    out += airstrip_tiles()
    out += [road_tile(0), road_tile(1), pier_tile(0), pier_tile(1)]
    out.append(crater_tile())
    assert len(out) == 77, len(out)
    return out


PAL1 = ['0x026', '0x6CE', '0x29B', '0xDDD', '0xEC8', '0x6A3', '0x352', '0xFFE']
NAMES = ['deep water', 'wave crest', 'shallow', 'surf', 'beach', 'land', 'land dark', 'bright']


def write_kit(art_dir):
    ts = full_kit()
    PENS = '01234567'
    with open(os.path.join(art_dir, 'tile_pal.txt'), 'w') as f:
        f.write('# PF2 terrain pens (colours 8-15 on screen); theatre 1, band 1. The copper\n'
                "# loads each island band's own set (art/tools/theatres.py).\n")
        for i, (c, n) in enumerate(zip(PAL1, NAMES)):
            f.write(f'{i}  {c}  {"transparent" if i == 0 else n}\n')
    with open(os.path.join(art_dir, 'tiles.txt'), 'w') as f:
        f.write('# Terrain tiles 16x16 (PF2), one frame per tile: see art/tools/tiles.py\n')
        f.write('# (drawn by that script - edit it, not this)\ncolors\n  .  transparent\n')
        for i in range(1, 8):
            f.write(f'  {PENS[i]}  {PAL1[i]}\n')
        for g in ts:
            f.write('frame\n')
            for row in g:
                f.write(''.join('.' if p == 0 else PENS[p] for p in row) + '\n')
    cols, rgb = 8, [tuple(int(c[i], 16) * 17 for i in (2, 3, 4)) for c in PAL1]
    rows_n = (len(ts) + cols - 1) // cols
    img = [[rgb[0]] * (cols * T) for _ in range(rows_n * T)]
    for i, g in enumerate(ts):
        for y in range(T):
            for x in range(T):
                img[(i // cols) * T + y][(i % cols) * T + x] = rgb[g[y][x]]

    def chunk(tag, d):
        return struct.pack('>I', len(d)) + tag + d + struct.pack('>I', zlib.crc32(tag + d))
    raw = b''.join(b'\0' + bytes(v for p in r for v in p) for r in img)
    with open(os.path.join(art_dir, 'tiles.png'), 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', cols * T, rows_n * T, 8, 2, 0, 0, 0)) +
                chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))
    with open(os.path.join(art_dir, 'tiles.tsx'), 'w') as f:
        f.write(f'<?xml version="1.0" encoding="UTF-8"?>\n<tileset version="1.10" name="tiles" tilewidth="16" '
                f'tileheight="16" tilecount="{len(ts)}" columns="{cols}">\n'
                f' <image source="tiles.png" width="{cols * T}" height="{rows_n * T}"/>\n</tileset>\n')
    return ts




def main():
    ts = write_kit(ART)
    print(f'{len(ts)} tiles')


if __name__ == '__main__':
    main()
