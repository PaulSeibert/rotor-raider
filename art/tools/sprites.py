#!/usr/bin/env python3
"""ROTOR RAIDER's rotating craft, rendered from simple top-down models at
16 headings (0 = north, clockwise), 4x4 supersampled, lit from the world's
top-left whatever way the craft faces. Writes text art for agk's art
pipeline:

  art/chopper.txt   the Kestrel gunship: 32x32, 15 colours (an attached
                    sprite pair x 2), frame = heading * 2 + rotor phase
  art/shadow.txt    its shadow on the ground: 32x32 BOB, 16 headings
  art/carrier.txt   the carrier DPS Mirage: 96x32 BOB, east 0/1, west 2/3
                    (the wake's two frames)

BOBs use the game palette (art/palette.txt): 1 black, 2 shadow, 3 grey,
4 white, 5 orange, 6 yellow, 7 red.

    python3 art/tools/sprites.py
"""
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.path.join(HERE, '..')
SS = 4
LIGHT = (-0.6, -0.8)        # from the top-left of the world

# The gunship's colours (sprite colours 17-31, attached)
CHOPPER_COLORS = [
    ('K', '0x111', 'outline'),
    ('a', '0x232', 'body dark'),
    ('b', '0x464', 'body'),
    ('c', '0x686', 'body light'),
    ('d', '0x8A8', 'body highlight'),
    ('g', '0x25A', 'glass'),
    ('h', '0x7CF', 'glass shine'),
    ('r', '0x555', 'rotor'),
    ('s', '0x999', 'rotor light'),
    ('o', '0xF80', 'stripe'),
    ('y', '0xFF4', 'lamp'),
]


def ellipse(px, py, cx, cy, rx, ry):
    return ((px - cx) / rx) ** 2 + ((py - cy) / ry) ** 2 <= 1


def rect(px, py, x0, y0, x1, y1):
    return x0 <= px <= x1 and y0 <= py <= y1


def seg(px, py, ax, ay, bx, by, w):
    dx, dy = bx - ax, by - ay
    t = max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy)) <= w


def shade(nx, ny, base):
    """base: 0..3 (a b c d); brighter on the lit side"""
    lit = -(nx * LIGHT[0] + ny * LIGHT[1])
    k = base + (1 if lit > 0.35 else -1 if lit < -0.35 else 0)
    return 'abcd'[max(0, min(3, k))]


def chopper_pixel(lx, ly, wx, wy, phase):
    """lx, ly: model coordinates (nose to -y); wx, wy: the same point in the
    world's orientation (for lighting)."""
    # main rotor on top: two blades
    a0 = math.pi / 4 * phase
    for k in range(2):
        a = a0 + k * math.pi / 2
        bx, by = math.cos(a) * 14.5, math.sin(a) * 14.5
        if seg(lx, ly, -bx, -by, bx, by, 0.85):
            return 's' if (lx * bx + ly * by) > 0 else 'r'
    if lx * lx + ly * ly < 2.2:
        return 'K'
    # cockpit glass
    if ellipse(lx, ly, 0, -6.5, 2.7, 3.2):
        return 'h' if (ly < -7.5 and lx < 0) else 'g'
    # fuselage
    if ellipse(lx, ly, 0, -1.5, 4.4, 8.8):
        if abs(ly - 3.5) < 0.6:
            return 'o'
        r = math.hypot(lx / 4.4, (ly + 1.5) / 8.8) or 1
        nx, ny = wx / (r * 6), wy / (r * 9)
        return shade(nx, ny, 2)
    # tail boom, fin, tail rotor
    if rect(lx, ly, -1.1, 6, 1.1, 15):
        return shade(wx * 0.1, wy * 0.1, 1)
    if rect(lx, ly, -4.2, 12.5, 4.2, 14.0):
        return 'b'
    if seg(lx, ly, 2.2, 11.5, 2.2, 15.5, 0.6):
        return 's' if phase else 'r'
    # skids
    if rect(lx, ly, -6.2, -7, -5.2, 5) or rect(lx, ly, 5.2, -7, 6.2, 5):
        return 'a'
    if lx < -3.8 and abs(ly + 2) < 0.5 and lx > -5.5:
        return 'y'
    return '.'


def render(size, fn, heading, phase):
    ang = heading * math.pi / 8
    ca, sa = math.cos(ang), math.sin(ang)
    g = []
    for y in range(size):
        row = []
        for x in range(size):
            votes = {}
            for sy in range(SS):
                for sx in range(SS):
                    wx = x + (sx + 0.5) / SS - size / 2
                    wy = y + (sy + 0.5) / SS - size / 2
                    lx = wx * ca + wy * sa      # world -> model: rotate back
                    ly = -wx * sa + wy * ca
                    c = fn(lx, ly, wx, wy, phase)
                    votes[c] = votes.get(c, 0) + 1
            solid = {k: v for k, v in votes.items() if k != '.'}
            if sum(solid.values()) >= SS * SS // 2:
                row.append(max(solid, key=solid.get))
            else:
                row.append('.')
        g.append(row)
    # a dark outline where a solid pixel meets the outside
    out = [r[:] for r in g]
    for y in range(size):
        for x in range(size):
            if g[y][x] != '.' and g[y][x] not in 'rs':
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    xx, yy = x + dx, y + dy
                    if not (0 <= xx < size and 0 <= yy < size) or g[yy][xx] == '.':
                        out[y][x] = 'K'
                        break
    return out


def save(name, comment, colors, frames):
    with open(os.path.join(ART, name + '.txt'), 'w') as f:
        f.write(f'# {comment}\n# (drawn by art/tools/sprites.py - edit that, not this)\ncolors\n  .  transparent\n')
        for ch, c, n in colors:
            f.write(f'  {ch}  {c}\n')
        for g in frames:
            f.write('frame\n')
            for row in g:
                f.write(''.join(row) + '\n')
    print(f'{name}: {len(frames)} frames')


def shadow_pixel(lx, ly, wx, wy, phase):
    """The gunship's silhouette without its rotor blades."""
    if ellipse(lx, ly, 0, -1.5, 4.4, 8.8) or rect(lx, ly, -1.1, 6, 1.1, 15) or \
            rect(lx, ly, -4.2, 12.5, 4.2, 14.0):
        return '2'
    if rect(lx, ly, -6.2, -7, -5.2, 5) or rect(lx, ly, 5.2, -7, 6.2, 5):
        return '2'
    return '.'


def render_plain(size, fn, heading):
    """Like render, without the outline."""
    ang = heading * math.pi / 8
    ca, sa = math.cos(ang), math.sin(ang)
    g = []
    for y in range(size):
        row = []
        for x in range(size):
            n = 0
            for sy in range(SS):
                for sx in range(SS):
                    wx = x + (sx + 0.5) / SS - size / 2
                    wy = y + (sy + 0.5) / SS - size / 2
                    if fn(wx * ca + wy * sa, -wx * sa + wy * ca, wx, wy, 0) != '.':
                        n += 1
            row.append('2' if n >= SS * SS // 2 else '.')
        g.append(row)
    return g


def carrier(phase):
    """96x32, bow to the right (east). Hull, flight deck, runway, helipad,
    the bridge on the starboard (south) side, a wake astern."""
    W, H = 96, 32
    g = [['.'] * W for _ in range(H)]
    cy = 15.5
    for y in range(H):
        for x in range(W):
            # hull: straight sides, pointed bow from x = 76, rounded stern
            half = 10.5
            if x > 76:
                half = 10.5 * (1 - ((x - 76) / 19.5) ** 1.6)
            if x < 10:
                half = 10.5 * math.sqrt(max(0, 1 - ((10 - x) / 9.0) ** 2))
            d = abs(y + 0.5 - cy)
            if x >= 1 and d <= half:
                edge = half - d < 1.2
                g[y][x] = '1' if edge else '3'
                if not edge and (y < cy - 8 or y > cy + 8):
                    g[y][x] = '2'           # the deck edge, darker
    # runway centreline (dashes) and the deck's edge lines
    for x in range(14, 80):
        if (x // 4) % 2 == 0:
            g[16][x] = '4'
    # helipad: a yellow ring with an H, aft
    for y in range(H):
        for x in range(W):
            r = math.hypot(x + 0.5 - 30, y + 0.5 - 15.5)
            if 5.2 <= r <= 6.4 and g[y][x] != '.':
                g[y][x] = '6'
    for y in range(12, 20):
        g[y][28] = '4'
        g[y][32] = '4'
    for x in range(28, 33):
        g[16][x] = '4'
    # the bridge (island) on the starboard side, with lights
    for y in range(21, 26):
        for x in range(52, 64):
            g[y][x] = '1' if (y in (21, 25) or x in (52, 63)) else '3'
    for x in range(54, 62, 2):
        g[22][x] = '4'
    g[24][56] = '7'
    g[23][60] = '6'
    # wake astern: foam, two frames
    for y in range(9, 23):
        for x in range(0, 6):
            if g[y][x] == '.' and ((x + y + phase) % 3 == 0) and abs(y - cy) < 6 - x * 0.5:
                g[y][x] = '4'
    return g


def flip(g):
    return [list(reversed(r)) for r in g]


def save_bob(name, comment, frames):
    pal = {'1': '0x111', '2': '0x444', '3': '0x888', '4': '0xFEC', '5': '0xF60', '6': '0xFC2', '7': '0xC22'}
    used = sorted({c for g in frames for r in g for c in r if c != '.'})
    with open(os.path.join(ART, name + '.txt'), 'w') as f:
        f.write(f'# {comment}\n# (drawn by art/tools/sprites.py - edit that, not this)\ncolors\n  .  transparent\n')
        for c in used:
            f.write(f'  {c}  {pal[c]}\n')
        for g in frames:
            f.write('frame\n')
            for row in g:
                f.write(''.join(row) + '\n')
    print(f'{name}: {len(frames)} frames')


def small_bobs():
    """shot (a tracer), bomb, fireball (6 frames), puff (4), reticle (2)"""
    shot = [list('.46.' + '.' * 12), list('4664' + '.' * 12), list('4664' + '.' * 12), list('.46.' + '.' * 12)]
    bomb = [list(r.ljust(16, '.')) for r in ['..11....', '.1331...', '133331..', '133331..', '.1331...',
                                              '..11....', '.1..1...', '........']]
    booms = []
    for f, (r, core) in enumerate([(4, 3), (8, 5), (11, 6), (13, 5), (14, 2), (12, 0)]):
        g = [['.'] * 32 for _ in range(32)]
        rnd = __import__('random').Random(600 + f)
        for y in range(32):
            for x in range(32):
                d = math.hypot(x + 0.5 - 16, y + 0.5 - 16) + rnd.uniform(-1.5, 1.5)
                if d < core:
                    g[y][x] = '4' if d < core * 0.5 else '6'
                elif d < r:
                    if f >= 4:
                        g[y][x] = '2' if rnd.random() < 0.7 else ('3' if rnd.random() < 0.5 else '.')
                    else:
                        g[y][x] = '5' if d < r * 0.75 else '7'
        booms.append(g)
    puffs = []
    for f, r in enumerate([3, 5, 6, 6]):
        g = [['.'] * 16 for _ in range(16)]
        rnd = __import__('random').Random(700 + f)
        for y in range(16):
            for x in range(16):
                d = math.hypot(x + 0.5 - 8, y + 0.5 - 8) + rnd.uniform(-1, 1)
                if d < r:
                    g[y][x] = ('6' if d < 2 else '5') if f == 0 else ('3' if f < 3 else '2') if rnd.random() < 0.8 - f * 0.15 else '.'
        puffs.append(g)
    rets = []
    for c in ('4', '6'):
        g = [['.'] * 16 for _ in range(16)]
        for i in range(4):
            for (x, y) in ((i, 0), (0, i), (15 - i, 0), (15, i), (i, 15), (0, 15 - i), (15 - i, 15), (15, 15 - i)):
                g[y][x] = c
        g[7][7] = g[7][8] = g[8][7] = g[8][8] = c
        rets.append(g)
    return [shot], [bomb], booms, puffs, rets


def render_pens(size, fn, heading):
    """A BOB at a heading: fn(lx, ly, wx, wy) -> pen char or '.', majority of
    4x4 samples; a black outline where it meets the outside."""
    ang = heading * math.pi / 8
    ca, sa = math.cos(ang), math.sin(ang)
    g = []
    for y in range(size):
        row = []
        for x in range(size):
            votes = {}
            for sy in range(SS):
                for sx in range(SS):
                    wx = x + (sx + 0.5) / SS - size / 2
                    wy = y + (sy + 0.5) / SS - size / 2
                    c = fn(wx * ca + wy * sa, -wx * sa + wy * ca, wx, wy)
                    votes[c] = votes.get(c, 0) + 1
            solid = {k: v for k, v in votes.items() if k != '.'}
            row.append(max(solid, key=solid.get) if sum(solid.values()) >= SS * SS // 2 else '.')
        g.append(row)
    out = [r[:] for r in g]
    for y in range(size):
        for x in range(size):
            if g[y][x] not in '.1':
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    xx, yy = x + dx, y + dy
                    if not (0 <= xx < size and 0 <= yy < size) or g[yy][xx] == '.':
                        out[y][x] = '1'
                        break
    return out


def lit(wx, wy, nx, ny, base):
    """grey shading: '2' dark, '3' mid, '4' light"""
    l = -(nx * LIGHT[0] + ny * LIGHT[1])
    k = base + (1 if l > 0.3 else -1 if l < -0.3 else 0)
    return '234'[max(0, min(2, k))]


def jet_pixel(lx, ly, wx, wy):
    # a delta-wing interceptor, nose to -y
    if ellipse(lx, ly, 0, -1, 2.2, 11):
        if ellipse(lx, ly, 0, -6, 1.2, 2.4):
            return '6'                                  # canopy
        return lit(wx, wy, lx / 2.2, (ly + 1) / 11, 1)
    # the delta: from (0,-4) back to (+-9, 8)
    if -4 <= ly <= 8 and abs(lx) <= (ly + 4) * 0.75:
        if abs(lx) > (ly + 4) * 0.75 - 1.6 and ly > 3:
            return '7'                                  # red wingtips
        return lit(wx, wy, lx / 9, 0.3, 0)
    if 7 <= ly <= 10 and abs(lx) <= 4:
        return '2'                                      # tailplane
    if 10 < ly <= 11.5 and abs(lx) <= 1:
        return '5'                                      # exhaust
    return '.'


def bomber_pixel(lx, ly, wx, wy):
    # a straight-wing bomber, four engines
    if ellipse(lx, ly, 0, 0, 2.6, 13):
        if ellipse(lx, ly, 0, -9.5, 1.5, 2.2):
            return '6'
        return lit(wx, wy, lx / 2.6, ly / 13, 1)
    if -3 <= ly <= 1 and abs(lx) <= 14:
        for ex in (-9, -5, 5, 9):
            if abs(lx - ex) < 1.2 and -5 <= ly <= 1:
                return '1'
        if abs(lx) > 12.5:
            return '7'
        return lit(wx, wy, lx / 14, -0.4, 1)
    if 9 <= ly <= 11 and abs(lx) <= 5:
        return '2'
    return '.'


def boat_pixel(lx, ly, wx, wy):
    # a small supply boat, bow to -y
    half = 3.2 if ly > -2 else 3.2 * max(0, (ly + 7) / 5)
    if -7 <= ly <= 6 and abs(lx) <= half:
        if -1 <= ly <= 4 and abs(lx) <= 1.8:
            return '5'                                  # cargo
        if -4 <= ly < -1 and abs(lx) <= 1.6:
            return '4'                                  # wheelhouse
        return '3'
    if 6 < ly <= 8 and abs(lx) <= 2.5:
        return '4'                                      # wake
    return '.'


def missile_pixel(lx, ly, wx, wy):
    if -4 <= ly <= 3 and abs(lx) <= 0.9:
        return '4' if ly < -2 else '3'
    if 3 < ly <= 6 and abs(lx) <= 1.2:
        return '6' if ly < 5 else '5'
    return '.'


def cruiser():
    """96x32, bow to the east: grey warship with gun turrets, red stripes"""
    W, H = 96, 32
    g = [['.'] * W for _ in range(H)]
    cy = 15.5
    for y in range(H):
        for x in range(W):
            half = 9.5
            if x > 70:
                half = 9.5 * (1 - ((x - 70) / 25.0) ** 1.5)
            if x < 8:
                half = 9.5 * math.sqrt(max(0, 1 - ((8 - x) / 7.0) ** 2))
            d = abs(y + 0.5 - cy)
            if x >= 1 and d <= half:
                g[y][x] = '1' if half - d < 1.2 else ('2' if y > cy else '3')
    for y in range(11, 21):                              # superstructure
        for x in range(36, 58):
            g[y][x] = '1' if y in (11, 20) or x in (36, 57) else '4' if y < 15 else '3'
    for tx in (20, 68, 78):                              # gun turrets
        for y in range(H):
            for x in range(W):
                if math.hypot(x + 0.5 - tx, y + 0.5 - cy) < 3.6:
                    g[y][x] = '2' if math.hypot(x + 0.5 - tx, y + 0.5 - cy) > 2.4 else '3'
        for x in range(tx, tx + 7):
            g[15][x] = g[16][x] = '1'
    for x in range(10, 30):
        g[8][x] = g[23][x] = '7'
    return g


FONT5 = {   # 5x7 capitals for the logo
    'R': ['1111.', '1...1', '1...1', '1111.', '1.1..', '1..1.', '1...1'],
    'O': ['.111.', '1...1', '1...1', '1...1', '1...1', '1...1', '.111.'],
    'T': ['11111', '..1..', '..1..', '..1..', '..1..', '..1..', '..1..'],
    'A': ['.111.', '1...1', '1...1', '11111', '1...1', '1...1', '1...1'],
    'I': ['11111', '..1..', '..1..', '..1..', '..1..', '..1..', '11111'],
    'D': ['1111.', '1...1', '1...1', '1...1', '1...1', '1...1', '1111.'],
    'E': ['11111', '1....', '1....', '1111.', '1....', '1....', '11111'],
    ' ': ['.....'] * 7,
}


def logo(text='ROTOR RAIDER', sx=4, sy=4):
    """Chunky capitals, sx x sy per font pixel: yellow at the top through
    orange to red, a black outline and drop shadow"""
    w = len(text) * 6 * sx - sx + 4
    w = (w + 15) // 16 * 16
    h = 7 * sy + 4
    g = [['.'] * w for _ in range(h)]
    x0 = (w - (len(text) * 6 * sx - sx)) // 2
    for i, ch in enumerate(text):
        for fy, row in enumerate(FONT5[ch]):
            for fx, b in enumerate(row):
                if b != '1':
                    continue
                for yy in range(sy):
                    for xx in range(sx):
                        y = 1 + fy * sy + yy
                        x = x0 + i * 6 * sx + fx * sx + xx
                        band = y * 3 // (7 * sy)
                        g[y][x] = '64' [0] if band == 0 else ('5' if band == 1 else '7')
                        if yy == 0 and (fy == 0 or FONT5[ch][fy - 1][fx] != '1'):
                            g[y][x] = '4'      # a highlight on top edges
    # shadow down-right, then the outline
    out = [r[:] for r in g]
    for y in range(h):
        for x in range(w):
            if g[y][x] == '.' and y >= 2 and x >= 2 and g[y - 2][x - 2] != '.':
                out[y][x] = '2'
    g2 = [r[:] for r in out]
    for y in range(h):
        for x in range(w):
            if out[y][x] == '.':
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < w and 0 <= yy < h and out[yy][xx] not in '.2':
                        g2[y][x] = '1'
                        break
    return g2


# The BOBs go on the floppy, not in the program (an A500's 512 KB of slow
# RAM holds the program; its images would be there twice: once in the
# program, once in chip RAM). In the order of logic.h's BOB_*:
BOB_ORDER = ['shadow', 'carrier', 'shot', 'bomb', 'boom', 'puff', 'reticle', 'jet', 'bomber', 'boat',
             'cruiser', 'flak', 'missile', 'logo', 'pointer']


def read_txt(name):
    """frames of a BOB text art file (pen digits, '.' transparent)"""
    frames, cur = [], None
    for line in open(os.path.join(ART, name + '.txt')):
        line = line.rstrip('\n')
        if line == 'frame':
            cur = []
            frames.append(cur)
        elif cur is not None and line and not line.startswith('#'):
            cur.append(line)
    return frames


def export_bobs():
    """disk/bobs.bin: for each BOB, its frames stacked, a row = the 3 planes'
    words then the mask's (the layout agk's art.c uses). src/bobs.h: sizes."""
    import struct
    blob = bytearray()
    lines = ['// Generated by art/tools/sprites.py - don\'t edit. The BOBs on the floppy',
             '// (disk/bobs.bin -> bobs.bin.pk): per BOB its frames stacked, each row the',
             '// 3 planes\' words then the mask\'s.', '#ifndef _BOBS_H_', '#define _BOBS_H_',
             'typedef struct { unsigned long ulOffs; unsigned short uwW, uwH, uwFrames; } tBobFile;',
             'static const tBobFile g_pBobFile[] = {']
    for name in BOB_ORDER:
        frames = read_txt(name)
        w = max(len(r) for f in frames for r in f)
        w16 = (w + 15) // 16 * 16
        h = len(frames[0])
        off = len(blob)
        for f in frames:
            for row in f:
                row = row.ljust(w16, '.')
                pens = [0 if c == '.' else int(c) for c in row]
                for p in range(3):
                    for x0 in range(0, w16, 16):
                        v = 0
                        for b in range(16):
                            if (pens[x0 + b] >> p) & 1:
                                v |= 0x8000 >> b
                        blob += struct.pack('>H', v)
                for x0 in range(0, w16, 16):
                    v = 0
                    for b in range(16):
                        if pens[x0 + b]:
                            v |= 0x8000 >> b
                    blob += struct.pack('>H', v)
        lines.append(f'\t{{{off}, {w16}, {h}, {len(frames)}}},   // {name}')
    lines += ['};', f'#define BOBS_BYTES {len(blob)}', '#endif', '']
    os.makedirs(os.path.join(ART, '..', 'disk'), exist_ok=True)
    open(os.path.join(ART, '..', 'disk', 'bobs.bin'), 'wb').write(blob)
    open(os.path.join(ART, '..', 'src', 'bobs.h'), 'w').write('\n'.join(lines))
    print(f'disk/bobs.bin: {len(blob)} bytes')


def pointer_pixel(lx, ly, wx, wy):
    """home's pointer: a chevron, tip to -y"""
    if ly < -6 or ly > 5:
        return '.'
    half = (ly + 6) * 0.55
    if abs(lx) > half:
        return '.'
    if ly > 1 and abs(lx) < (ly - 1) * 0.9:
        return '.'                                   # the notch
    return '5' if ly > 2 else '6'


def chrome_logo():
    """ROTOR over a bigger RAIDER: smooth, slanted capitals with a bevel.
    Pens: 1 outline, 2 drop shadow, 3 bevel dark, 4 rotor blur, 5 face,
    6 face shine, 7 bevel light. The copper colours pens 3-7 line by line
    (a chrome gradient), so the face is one pen here."""
    W, H = 304, 92

    def glyph_field(ch):
        rows = FONT5[ch]
        def f(u, v):            # u, v in glyph cells; bilinear over cell centres
            u -= 0.5
            v -= 0.5
            x0, y0 = math.floor(u), math.floor(v)
            tx, ty = u - x0, v - y0
            def cell(x, y):
                return 1.0 if 0 <= y < 7 and 0 <= x < 5 and rows[y][x] == '1' else 0.0
            a = cell(x0, y0) * (1 - tx) + cell(x0 + 1, y0) * tx
            b = cell(x0, y0 + 1) * (1 - tx) + cell(x0 + 1, y0 + 1) * tx
            return a * (1 - ty) + b * ty
        return f

    mask = [[0] * W for _ in range(H)]
    def word(text, sx, sy, top, gap):
        width = len(text) * (5 * sx + gap) - gap
        left = (W - width) // 2 - 8
        for i, ch in enumerate(text):
            f = glyph_field(ch)
            for y in range(7 * sy):
                for x in range(5 * sx + 10):
                    shear = (7 * sy - y) * 0.24
                    u, v = (x - shear) / sx, y / sy
                    if 0 <= u < 5 and f(u, v) > 0.30:
                        X, Y = left + i * (5 * sx + gap) + x, top + y
                        if 0 <= X < W and 0 <= Y < H:
                            mask[Y][X] = 1
        return top, top + 7 * sy
    word('ROTOR', 6, 5, 2, 5)
    word('RAIDER', 8, 6, 42, 6)
    g = [['.'] * W for _ in range(H)]
    # the rotor's blur: an arc behind ROTOR
    for y in range(H):
        for x in range(W):
            d = math.hypot((x - W / 2 + 8) / 150.0, (y - 30) / 26.0)
            if 0.93 < d < 1.0 and y < 34:
                g[y][x] = '4'
    # drop shadow, then the letters with a bevel
    for y in range(H):
        for x in range(W):
            if mask[y][x] == 0 and y >= 3 and x >= 3 and mask[y - 3][x - 3]:
                g[y][x] = '2'
    for y in range(H):
        for x in range(W):
            if not mask[y][x]:
                continue
            up = y > 0 and mask[y - 1][x] and x > 0 and mask[y][x - 1]
            dn = y < H - 1 and mask[y + 1][x] and x < W - 1 and mask[y][x + 1]
            up2 = y > 1 and mask[y - 2][x]
            dn2 = y < H - 2 and mask[y + 2][x]
            if not up or not up2:
                g[y][x] = '7'
            elif not dn or not dn2:
                g[y][x] = '3'
            else:
                g[y][x] = '5'
    # a diagonal glint on the faces
    for y in range(H):
        for x in range(W):
            if g[y][x] == '5' and (x + y) % 23 in (0, 1):
                g[y][x] = '6'
    # the black outline
    out = [r[:] for r in g]
    for y in range(H):
        for x in range(W):
            if g[y][x] in '.24':
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < W and 0 <= yy < H and mask[yy][xx]:
                        out[y][x] = '1'
                        break
    return out


def main():
    frames = [render(32, chopper_pixel, h, p) for h in range(16) for p in range(2)]
    save('chopper', 'The Kestrel gunship, 32x32, heading * 2 + rotor phase', CHOPPER_COLORS, frames)
    save_bob('shadow', "The gunship's shadow, 32x32, one frame per heading",
             [render_plain(32, shadow_pixel, h) for h in range(16)])
    e = [carrier(0), carrier(1)]
    save_bob('carrier', 'The carrier DPS Mirage, 96x32: east 0/1, west 2/3', e + [flip(f) for f in e])
    # enemies: 8 headings drawn (the rules steer in 16; the art shows the nearest 8)
    save_bob('jet', 'An interceptor, 32x32, 8 headings', [render_pens(32, jet_pixel, h) for h in range(0, 16, 2)])
    save_bob('bomber', 'A bomber, 32x32, 8 headings', [render_pens(32, bomber_pixel, h) for h in range(0, 16, 2)])
    save_bob('boat', 'A supply boat, 16x16, 8 headings', [render_pens(16, boat_pixel, h) for h in range(0, 16, 2)])
    save_bob('missile', 'A homing missile, 16x16, 8 headings', [render_pens(16, missile_pixel, h) for h in range(0, 16, 2)])
    c = cruiser()
    save_bob('cruiser', 'The Combine cruiser, 96x32: east, west', [c, flip(c)])
    flak = [list(r.ljust(16, '.')) for r in ['.66.....', '6446....', '6446....', '.66.....']]   # a bright tracer
    save_bob('flak', 'A flak or naval shell, 16x4', [flak])
    save_bob('logo', 'ROTOR RAIDER, the title: chrome (the copper colours pens 3-7 line by line)', [chrome_logo()])
    save_bob('pointer', "Home's pointer at the screen's edge, 16x16, 8 headings",
             [render_pens(16, pointer_pixel, h) for h in range(0, 16, 2)])
    shot, bomb, booms, puffs, rets = small_bobs()
    save_bob('shot', 'A cannon round (tracer), 16x4', shot)
    save_bob('bomb', 'A falling bomb, 16x8', bomb)
    save_bob('boom', 'An explosion, 32x32, 6 frames', booms)
    save_bob('puff', 'A shell hitting the ground, 16x16, 4 frames', puffs)
    save_bob('reticle', 'Where a bomb would land, 16x16, 2 frames (blinks)', rets)


if __name__ == '__main__':
    main()
    export_bobs()
