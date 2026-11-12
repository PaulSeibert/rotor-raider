#!/usr/bin/env python3
"""The interceptor's missile tell (1.4): art/jet.txt's frames 8-15, the jet's 8 headings with
a flash at the nose and at both wingtips. Run it after the pilot's integ.py wrote the 8 plain
frames: it keeps frames 0-7 and (re)makes 8-15, so it can be run again and again.

The flash is a small starburst in PF1's pens, the way STYLE.md draws a flare: a white (pen 4)
core with a yellow (pen 6) ring at the nose (a diamond, 2 px out) and a plus (1 px out) at each
wingtip, over whatever is there (it may take the outline: a flare glows). The light and the
palette are the jet's own; the plain frames are unchanged. Frame k points k * 45 degrees
clockwise from north (N NE E SE S SW W NW).
"""
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PATH = os.path.join(HERE, '..', 'jet.txt')
W = H = 32


def read(path):
    head, frames, cur = [], [], None
    for line in open(path):
        line = line.rstrip('\n')
        if line == 'frame':
            cur = []
            frames.append(cur)
        elif cur is None:
            head.append(line)
        elif line:
            cur.append(list(line))
    return head, frames


def extreme(frame, ux, uy):
    """the opaque pixel farthest along (ux, uy) from the middle"""
    best, at = -1e9, (16, 16)
    for y in range(H):
        for x in range(W):
            if frame[y][x] != '.':
                d = (x + 0.5 - 16) * ux + (y + 0.5 - 16) * uy
                if d > best:
                    best, at = d, (x, y)
    return at


def flare(frame, cx, cy, reach):
    for dy in range(-reach, reach + 1):
        for dx in range(-reach, reach + 1):
            d = abs(dx) + abs(dy)
            if d > reach:
                continue
            x, y = cx + dx, cy + dy
            if 0 <= x < W and 0 <= y < H:
                frame[y][x] = '4' if d == 0 else '6'


def tell_frame(frame, k):
    a = k * math.pi / 4
    ux, uy = math.sin(a), -math.cos(a)            # the nose
    out = [row[:] for row in frame]
    nx, ny = extreme(frame, ux, uy)
    # in from the tip by one pixel along the heading, so the flare sits on the nose
    flare(out, int(round(nx - ux)), int(round(ny - uy)), 2)
    for s in (1, -1):
        wx, wy = extreme(frame, -uy * s, ux * s)  # the wingtips: across the heading
        flare(out, int(round(wx + uy * s)), int(round(wy - ux * s)), 1)
    return out


def main():
    head, frames = read(PATH)
    plain = frames[:8]
    assert len(plain) == 8, 'jet.txt should start with the 8 plain frames'
    head = [h for h in head if 'missile tell' not in h]
    if head and head[0].startswith('# An interceptor'):
        head[0] = '# An interceptor, 32x32, 8 headings, then the same 8 flashing (the missile tell)'
        head.insert(2, '# (frames 8-15: art/tools/jet_tell.py draws the flash over frames 0-7)')
    with open(PATH, 'w', newline='\n') as f:
        for h in head:
            f.write(h + '\n')
        for fr in plain + [tell_frame(p, k) for k, p in enumerate(plain)]:
            f.write('frame\n')
            for row in fr:
                f.write(''.join(row) + '\n')
    print('art/jet.txt: 8 headings + 8 tell frames')


if __name__ == '__main__':
    main()
