#!/usr/bin/env python3
"""The Desert Palms Software logo for the Amiga: the 16-bit logo (the right
half of art/src/desert_palms_logo_source.png) at 288 px wide, in 16 colours
where 4 of them change on every line (the copper reloads them), so the
shield's sunset stripes and the purple horizon keep their gradients.

Writes:
  disk/dpslogo.bin       the picture for the floppy (agk packs it): 16 colours,
                         ROW_REGS colours per line, then 4 interleaved bitplanes
  src/dps_logo.h         its sizes, for main.c
  art/dps_logo_amiga.png how it looks on the Amiga (2x), for the docs

Plain Python 3, no libraries:
    python3 art/tools/make_dps_logo.py
"""
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..')
SRC = os.path.join(ROOT, 'art', 'src', 'desert_palms_logo_source.png')

W = 288            # multiple of 16
PLANES = 4
ROW_REGS = 4       # colours 12-15 change per line
FIXED = 16 - ROW_REGS


# ------------------------------------------------------------------ PNG in/out
def load_png(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', 'not a PNG'
    pos, idat = 8, b''
    while pos < len(data):
        n, tag = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if tag == b'IHDR':
            w, h, depth, ctype, _, _, inter = struct.unpack('>IIBBBBB', body)
            assert depth == 8 and ctype in (2, 6) and not inter, 'need 8-bit RGB/RGBA'
        elif tag == b'IDAT':
            idat += body
        pos += 12 + n
    ch = 4 if ctype == 6 else 3
    stride = w * ch
    raw = zlib.decompress(idat)
    prev = bytearray(stride)
    px = []
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - ch] if i >= ch else 0
            b = prev[i]
            c = prev[i - ch] if i >= ch else 0
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
        px.append([tuple(line[x * ch:x * ch + 3]) for x in range(w)])
        prev = line
    return w, h, px


def save_png(path, w, h, rows):
    raw = b''.join(b'\x00' + bytes(v for p in r for v in p) for r in rows)

    def chunk(tag, d):
        return struct.pack('>I', len(d)) + tag + d + struct.pack('>I', zlib.crc32(tag + d))
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


# ------------------------------------------------------------------- colours
def to12(c):
    return tuple(min(15, int(v + 8) // 17) for v in c)


def r12(c):
    """a colour average (in 0-15 units) to the nearest Amiga colour"""
    return tuple(min(15, max(0, int(v + 0.5))) for v in c)


def rgb(c12):
    return tuple(v * 17 for v in c12)


def dist(a, b):
    dr, dg, db = a[0] - b[0], a[1] - b[1], a[2] - b[2]
    return 3 * dr * dr + 4 * dg * dg + 2 * db * db


def nearest(c, pal):
    best, bi = 1 << 30, 0
    for i, p in enumerate(pal):
        d = dist(c, p)
        if d < best:
            best, bi = d, i
    return bi, best


def kmeans(points, k, iters=12, init=None):
    """points: [(colour, weight)]"""
    if not points:
        return []
    pts = sorted(points, key=lambda p: -p[1])
    cent = list(init) if init else []
    if not cent:
        cent = [pts[0][0]]
    while len(cent) < k:
        # farthest-point start, weighted
        far, fd = None, -1
        for c, wgt in pts:
            d = nearest(c, cent)[1] * wgt
            if d > fd:
                far, fd = c, d
        if fd <= 0:
            break
        cent.append(far)
    for _ in range(iters):
        acc = [[0, 0, 0, 0] for _ in cent]
        for c, wgt in pts:
            i = nearest(c, cent)[0]
            a = acc[i]
            a[0] += c[0] * wgt; a[1] += c[1] * wgt; a[2] += c[2] * wgt; a[3] += wgt
        cent = [(a[0] / a[3], a[1] / a[3], a[2] / a[3]) if a[3] else cent[i] for i, a in enumerate(acc)]
    return cent


# ------------------------------------------------------------------- picture
def main():
    sw, sh, src = load_png(SRC)
    # the 16-bit logo: the right half; its box is where it's brighter than the night
    x0 = sw // 2
    xs, ys = [], []
    for y in range(sh):
        for x in range(x0, sw, 2):
            r, g, b = src[y][x]
            if max(r, g, b) > 110:
                xs.append(x)
                ys.append(y)
    bx0, bx1, by0, by1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
    scale = (bx1 - bx0) / W
    H = int((by1 - by0) / scale) & ~1
    print(f'source box x {bx0}-{bx1} y {by0}-{by1}, scale 1/{scale:.3f} -> {W}x{H}')

    # area average down to W x H
    img = []
    for y in range(H):
        sy0, sy1 = by0 + int(y * scale), by0 + max(int(y * scale) + 1, int((y + 1) * scale))
        row = []
        for x in range(W):
            sx0, sx1 = bx0 + int(x * scale), bx0 + max(int(x * scale) + 1, int((x + 1) * scale))
            r = g = b = n = 0
            for yy in range(sy0, sy1):
                line = src[yy]
                for xx in range(sx0, sx1):
                    p = line[xx]
                    r += p[0]; g += p[1]; b += p[2]; n += 1
            row.append(to12((r / n, g / n, b / n)))
        img.append(row)

    # the fixed colours: the whole picture's main colours (colour 0 the night)
    hist = {}
    for row in img:
        for c in row:
            hist[c] = hist.get(c, 0) + 1
    bg = max(hist, key=hist.get)
    fixed = kmeans(list(hist.items()), FIXED, init=[bg])
    fixed = [bg] + [r12(c) for c in fixed[1:]]
    # keep pure black for the palm if anything near it is used
    print('fixed:', ' '.join('%X%X%X' % c for c in fixed))

    # per line: ROW_REGS more colours for what the fixed ones fit badly
    rowpal, prev = [], None
    for y, row in enumerate(img):
        h = {}
        for c in row:
            d = nearest(c, fixed)[1]
            if d > 6:
                h[c] = h.get(c, 0) + d
        pts = list(h.items())
        cent = [r12(c) for c in kmeans(pts, ROW_REGS)] if pts else []
        while len(cent) < ROW_REGS:
            cent.append(prev[len(cent)] if prev else bg)
        if prev:
            # keep each register's colour where it can: fewer copper MOVEs
            order, left = [None] * ROW_REGS, list(cent)
            for i in range(ROW_REGS):
                if prev[i] in left:
                    order[i] = prev[i]
                    left.remove(prev[i])
            for i in range(ROW_REGS):
                if order[i] is None:
                    j = min(range(len(left)), key=lambda j: dist(left[j], prev[i]))
                    order[i] = left.pop(j)
            cent = order
        rowpal.append(cent)
        prev = cent

    # pixels -> pens
    pens, out = [], []
    for y, row in enumerate(img):
        pal = fixed + rowpal[y]
        pr, orow = [], []
        for c in row:
            i = nearest(c, pal)[0]
            pr.append(i)
            orow.append(rgb(pal[i]))
        pens.append(pr)
        out.append(orow)

    moves = sum(1 for y in range(1, H) for i in range(ROW_REGS) if rowpal[y][i] != rowpal[y - 1][i])
    print(f'{moves} colour changes over {H} lines')

    # disk/dpslogo.bin: the unique colours (words, big-endian), then colour
    # indices (words): the first line's 16 registers, ROW_REGS per line; then
    # the bitplanes. Indices let the fade work on the few unique colours.
    def w12(c):
        return (c[0] << 8) | (c[1] << 4) | c[2]
    uniq = []
    for c in fixed + [c for r in rowpal for c in r]:
        if c not in uniq:
            uniq.append(c)
    blob = bytearray()
    for c in uniq:
        blob += struct.pack('>H', w12(c))
    first_offs = len(blob)
    for c in fixed + rowpal[0]:
        blob += struct.pack('>H', uniq.index(c))
    rows_offs = len(blob)
    for y in range(H):
        for c in rowpal[y]:
            blob += struct.pack('>H', uniq.index(c))
    while len(blob) & 3:
        blob.append(0)
    planes_offs = len(blob)
    for y in range(H):
        for p in range(PLANES):
            for xb in range(W // 8):
                v = 0
                for bit in range(8):
                    if (pens[y][xb * 8 + bit] >> p) & 1:
                        v |= 0x80 >> bit
                blob.append(v)
    os.makedirs(os.path.join(ROOT, 'disk'), exist_ok=True)
    open(os.path.join(ROOT, 'disk', 'dpslogo.bin'), 'wb').write(blob)

    hdr = f"""// Generated by art/tools/make_dps_logo.py - don't edit.
// The Desert Palms Software logo on the floppy (disk/dpslogo.bin -> dpslogo.bin.pk):
// DPS_LOGO_COLORS unique colours, then colour indices (all words): the
// first line's 16 registers, DPS_LOGO_ROW_REGS per line (registers
// DPS_LOGO_FIRST_ROW_REG...); then DPS_LOGO_PLANES interleaved bitplanes.
#ifndef _DPS_LOGO_H_
#define _DPS_LOGO_H_
#define DPS_LOGO_W {W}
#define DPS_LOGO_H {H}
#define DPS_LOGO_PLANES {PLANES}
#define DPS_LOGO_ROW_REGS {ROW_REGS}
#define DPS_LOGO_FIRST_ROW_REG {FIXED}
#define DPS_LOGO_COLORS {len(uniq)}
#define DPS_LOGO_FIRST_OFFS {first_offs}
#define DPS_LOGO_ROWS_OFFS {rows_offs}
#define DPS_LOGO_PLANES_OFFS {planes_offs}
#define DPS_LOGO_BYTES {len(blob)}
#endif
"""
    open(os.path.join(ROOT, 'src', 'dps_logo.h'), 'w').write(hdr)
    print(f'{len(uniq)} unique colours')
    big = [[p for p in r for _ in range(2)] for r in out for _ in range(2)]
    save_png(os.path.join(ROOT, 'art', 'dps_logo_amiga.png'), W * 2, H * 2, big)
    print(f'disk/dpslogo.bin {len(blob)} bytes')


if __name__ == '__main__':
    main()
