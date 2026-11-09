#!/usr/bin/env python3
"""The ROTOR RAIDER title logo for the Amiga: the three-blade rotor in its
orange swirl over ROTOR (orange-gold) and RAIDER (blue), from
art/src/rotor_raider_title_source.png (512x512 RGBA), trimmed to its opaque
area and scaled to W x H.

It's a BOB in playfield 1 of the title's dual playfield, so the theatre keeps
panning behind it and shows through wherever the picture is see-through (pen
0). Playfield 1 has 3 planes: pens 1-7. The copper reloads all 7 on every one
of the logo's lines, so each line gets the 7 colours it needs most (k-means
over the line and a little of its neighbours, which keeps the gradients
smooth from line to line), with a light ordered (Bayer) dither between them.

Writes:
  art/logo.txt              the picture as BOB text art (pens 1-7, '.'), which
                            art/tools/sprites.py packs into disk/bobs.bin
  src/title_logo.h          its size and its 7 colours per line, for main.c
  art/title_logo_amiga.png  how it looks on the Amiga (2x, over the sea), for the docs

Plain Python 3, no libraries; run it before art/tools/sprites.py:
    python3 art/tools/make_title_logo.py [--out DIR]
"""
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from make_dps_logo import save_png, r12, rgb, dist, kmeans  # noqa: E402

ROOT = os.path.join(HERE, '..', '..')
SRC = os.path.join(ROOT, 'art', 'src', 'rotor_raider_title_source.png')

W = 160            # multiple of 16: 10 words; centred on the screen it's word aligned
H = 146            # (the source is 283x263: a little squashed, to leave room for the text)
PENS = 7           # playfield 1's pens 1-7, all reloaded on every line
ALPHA_MIN = 128    # source alpha (0-255, area averaged) from which a pixel is solid
NEIGHBOUR = 0.35   # the weight of the lines above and below in a line's palette
DITHER = 0.5       # 0: none; 1: a full ordered mix between the two nearest colours
BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def load_rgba(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', 'not a PNG'
    pos, idat = 8, b''
    while pos < len(data):
        n, tag = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if tag == b'IHDR':
            w, h, depth, ctype, _, _, inter = struct.unpack('>IIBBBBB', body)
            assert depth == 8 and ctype == 6 and not inter, 'need 8-bit RGBA'
        elif tag == b'IDAT':
            idat += body
        pos += 12 + n
    stride = w * 4
    raw = zlib.decompress(idat)
    prev = bytearray(stride)
    px = []
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - 4] if i >= 4 else 0
            b = prev[i]
            c = prev[i - 4] if i >= 4 else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        px.append([tuple(line[x * 4:x * 4 + 4]) for x in range(w)])
        prev = line
    return w, h, px


def pick(c, pal, x, y):
    """the pen for colour c (0-15 units, floats) at (x, y): the nearest, or
    with the ordered dither its neighbour, as often as c lies towards it"""
    best = sorted(range(len(pal)), key=lambda i: dist(c, pal[i]))[:2]
    i0, i1 = best[0], best[1]
    if DITHER <= 0 or i0 == i1:
        return i0
    a, b = pal[i0], pal[i1]
    ab = [b[k] - a[k] for k in range(3)]
    ll = sum(v * v for v in ab)
    if not ll:
        return i0
    t = sum((c[k] - a[k]) * ab[k] for k in range(3)) / ll      # 0 at a, 1 at b
    t = max(0.0, min(1.0, t)) * DITHER
    return i1 if t > (BAYER[y & 3][x & 3] + 0.5) / 16 else i0


def main():
    out = ROOT
    if '--out' in sys.argv:
        out = sys.argv[sys.argv.index('--out') + 1]
    sw, sh, src = load_rgba(SRC)
    xs = [x for y in range(sh) for x in range(sw) if src[y][x][3] >= ALPHA_MIN]
    ys = [y for y in range(sh) for x in range(sw) if src[y][x][3] >= ALPHA_MIN]
    bx0, bx1, by0, by1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
    sx, sy = (bx1 - bx0) / W, (by1 - by0) / H
    print(f'source box x {bx0}-{bx1} y {by0}-{by1} -> {W}x{H} (1/{sx:.3f} across, 1/{sy:.3f} down)')

    # area average down to W x H, premultiplied by alpha; solid where the
    # covered alpha reaches ALPHA_MIN (colours in 0-15 units, unrounded)
    img = []
    for y in range(H):
        fy0, fy1 = by0 + y * sy, by0 + (y + 1) * sy
        row = []
        for x in range(W):
            fx0, fx1 = bx0 + x * sx, bx0 + (x + 1) * sx
            r = g = b = a = n = 0.0
            for yy in range(int(fy0), min(sh, int(fy1 + 0.999))):
                wy = min(fy1, yy + 1) - max(fy0, yy)
                if wy <= 0:
                    continue
                line = src[yy]
                for xx in range(int(fx0), min(sw, int(fx1 + 0.999))):
                    wx = min(fx1, xx + 1) - max(fx0, xx)
                    if wx <= 0:
                        continue
                    p = line[xx]
                    wa = wx * wy * p[3]
                    r += p[0] * wa; g += p[1] * wa; b += p[2] * wa; a += wa; n += wx * wy
            if a / n >= ALPHA_MIN:
                row.append((r / a / 17, g / a / 17, b / a / 17))
            else:
                row.append(None)
        img.append(row)

    # 7 colours per line: the line's colours, and its neighbours' a little
    def hist(y, wgt, h):
        if 0 <= y < H:
            for c in img[y]:
                if c is not None:
                    k = r12(c)
                    h[k] = h.get(k, 0) + wgt
    rowpal, prev = [], None
    for y in range(H):
        h = {}
        hist(y, 1.0, h)
        hist(y - 1, NEIGHBOUR, h)
        hist(y + 1, NEIGHBOUR, h)
        pts = list(h.items())
        cent = [r12(c) for c in kmeans(pts, PENS)] if pts else []
        cent = list(dict.fromkeys(cent))                 # (no colour twice)
        while len(cent) < PENS:
            cent.append(prev[len(cent)] if prev else (0, 0, 0))
        if prev:
            # each register keeps its colour where it can (it reads better in
            # a copper dump; the copper writes all 7 a line anyway)
            order, left = [None] * PENS, list(cent)
            for i in range(PENS):
                if prev[i] in left:
                    order[i] = prev[i]
                    left.remove(prev[i])
            for i in range(PENS):
                if order[i] is None:
                    j = min(range(len(left)), key=lambda j: dist(left[j], prev[i]))
                    order[i] = left.pop(j)
            cent = order
        rowpal.append(cent)
        prev = cent

    # pixels -> pens (0: see-through)
    pens = []
    for y, row in enumerate(img):
        pens.append([0 if c is None else 1 + pick(c, rowpal[y], x, y) for x, c in enumerate(row)])
    used = sum(1 for r in pens for p in r if p)
    print(f'{used} solid pixels of {W * H}')

    def w12(c):
        return (c[0] << 8) | (c[1] << 4) | c[2]

    # art/logo.txt: BOB text art
    txt = ['# Generated by art/tools/make_title_logo.py - don\'t edit. The title logo, %dx%d:' % (W, H),
           '# pens 1-7, which the copper reloads on every line (src/title_logo.h).', 'frame']
    txt += [''.join('.' if p == 0 else str(p) for p in r) for r in pens]
    os.makedirs(os.path.join(out, 'art'), exist_ok=True)
    open(os.path.join(out, 'art', 'logo.txt'), 'w').write('\n'.join(txt) + '\n')

    hdr = ['// Generated by art/tools/make_title_logo.py - don\'t edit.',
           '// The title logo (art/logo.txt, BOB_LOGO in disk/bobs.bin): its size, and the',
           '// colours of playfield 1\'s pens 1-7 on each of its lines (the copper loads them).',
           '#ifndef _TITLE_LOGO_H_', '#define _TITLE_LOGO_H_',
           f'#define TITLE_LOGO_W {W}', f'#define TITLE_LOGO_H {H}', f'#define TITLE_LOGO_PENS {PENS}',
           f'static const unsigned short g_pTitleLogoPal[{H}][{PENS}] = {{']
    for r in rowpal:
        hdr.append('\t{' + ', '.join('0x%03X' % w12(c) for c in r) + '},')
    hdr += ['};', '#endif', '']
    os.makedirs(os.path.join(out, 'src'), exist_ok=True)
    open(os.path.join(out, 'src', 'title_logo.h'), 'w').write('\n'.join(hdr))

    # the preview: 2x, over the deep sea
    sea = rgb((0x1, 0x3, 0x7))
    rows = []
    for y in range(H + 8):
        row = []
        for x in range(W + 16):
            ly, lx = y - 4, x - 8
            if 0 <= ly < H and 0 <= lx < W and pens[ly][lx]:
                row.append(rgb(rowpal[ly][pens[ly][lx] - 1]))
            else:
                row.append(sea)
        rows.append(row)
    big = [[p for p in r for _ in range(2)] for r in rows for _ in range(2)]
    save_png(os.path.join(out, 'art', 'title_logo_amiga.png'), (W + 16) * 2, (H + 8) * 2, big)
    print('art/logo.txt, src/title_logo.h, art/title_logo_amiga.png')


if __name__ == '__main__':
    main()
