#!/usr/bin/env python3
"""ROTOR RAIDER's theatres: island maps made from a seed, with their
structures placed by rules, written to the floppy (disk/theatreN.bin, loaded
with agkLoadFile) and as Tiled maps (art/maps/theatreN.tmj, to look at).

Each theatre has three bands of islands (north, middle, south) with open sea
between them: the copper loads each band's own 8 terrain colours where its
first row comes on screen, so the change is never seen on land.

theatreN.bin, big-endian:
    "RRTH", W, H, object count, band count (words)
    W * H tile numbers (bytes, row by row)
    objects: 6 bytes each - type, tx, ty, a, b, c   (OBJ_* below; logic.h)
    bands: 18 bytes each - first tile row, 0, then colours for pens 0-7

    python3 art/tools/theatres.py
"""
import json
import math
import os
import random
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..', '..')
W, H = 160, 128
BANDS = (0, 42, 85)                      # first tile row of each band

# tiles (tiles.py)
SEA0, LAND0, PLAIN = 16, 20, 15
FACTORY, TURRET, RADAR, SHIPYARD, AIRSTRIP = 28, 55, 57, 59, 67
ROAD_H, ROAD_V, PIER_H, PIER_V = 72, 73, 74, 75

# objects (logic.h's OBJ_*)
OBJ_CARRIER, OBJ_FACTORY, OBJ_TURRET, OBJ_RADAR, OBJ_SHIPYARD, OBJ_AIRSTRIP, OBJ_PIER, OBJ_SPAWN = range(1, 9)


def field(rnd, islands):
    """Land height at the tile corners: (W+1) x (H+1)."""
    blobs = []
    for cx, cy, r in islands:
        k = rnd.randint(7, 12)
        for _ in range(k):
            a, d = rnd.uniform(0, 6.28), rnd.uniform(0, r * 0.8)
            rx, ry = rnd.uniform(r * 0.3, r * 0.65), rnd.uniform(r * 0.3, r * 0.65)
            blobs.append((cx + math.cos(a) * d, cy + math.sin(a) * d * 0.8, rx, ry))
    # value noise (two octaves) for ragged coasts, bays and headlands
    grid = {}

    def corner(ix, iy, k):
        key = (ix, iy, k)
        if key not in grid:
            grid[key] = rnd.uniform(-1, 1)
        return grid[key]

    def noise(x, y, scale, k):
        fx, fy = x / scale, y / scale
        ix, iy = int(math.floor(fx)), int(math.floor(fy))
        u, v = fx - ix, fy - iy
        u, v = u * u * (3 - 2 * u), v * v * (3 - 2 * v)
        a, b = corner(ix, iy, k), corner(ix + 1, iy, k)
        c, d = corner(ix, iy + 1, k), corner(ix + 1, iy + 1, k)
        return (a * (1 - u) + b * u) * (1 - v) + (c * (1 - u) + d * u) * v

    h = [[0.0] * (W + 1) for _ in range(H + 1)]
    for y in range(H + 1):
        for x in range(W + 1):
            v = 0.0
            for bx, by, rx, ry in blobs:
                d2 = ((x - bx) / rx) ** 2 + ((y - by) / ry) ** 2
                if d2 < 4:
                    v += math.exp(-d2)
            if v > 0.12:
                v += 0.32 * noise(x, y, 5.0, 0) + 0.14 * noise(x, y, 2.3, 1)
            h[y][x] = v
    return h


class Theatre:
    def __init__(self, n, seed, islands, palettes):
        self.n = n
        self.rnd = random.Random(seed)
        self.islands = islands
        self.palettes = palettes
        h = field(self.rnd, islands)
        land = [[1 if h[y][x] > 0.55 else 0 for x in range(W + 1)] for y in range(H + 1)]
        for y in range(H + 1):
            for x in range(W + 1):
                if x < 3 or y < 3 or x > W - 3 or y > H - 3:
                    land[y][x] = 0
        self.h = h
        self.t = []
        for y in range(H):
            row = []
            for x in range(W):
                m = land[y][x] | land[y][x + 1] << 1 | land[y + 1][x] << 2 | land[y + 1][x + 1] << 3
                if m == 0:
                    row.append(SEA0 + self.rnd.randrange(4) if self.rnd.random() < 0.35 else 0)
                elif m == 15:
                    hh = h[y][x]
                    if hh > 1.5:
                        row.append(LAND0 + 3 + self.rnd.randrange(3))
                    elif self.rnd.random() < 0.06:
                        row.append(LAND0 + 6 + self.rnd.randrange(2))
                    else:
                        row.append(LAND0 + self.rnd.randrange(3))
                else:
                    row.append(m)
            self.t.append(row)
        self.used = [[False] * W for _ in range(H)]
        self.objects = []

    # ------------------------------------------------------------- helpers
    def is_land(self, x, y):
        return 0 <= x < W and 0 <= y < H and (self.t[y][x] == PLAIN or LAND0 <= self.t[y][x] < LAND0 + 8)

    def is_sea(self, x, y):
        return 0 <= x < W and 0 <= y < H and (self.t[y][x] == 0 or SEA0 <= self.t[y][x] < SEA0 + 4)

    def free_area(self, x, y, w, h, border=1):
        for yy in range(y - border, y + h + border):
            for xx in range(x - border, x + w + border):
                if not self.is_land(xx, yy) or self.used[yy][xx]:
                    return False
        return True

    def place(self, first, x, y, w, h):
        for i in range(w * h):
            self.t[y + i // w][x + i % w] = first + i
            self.used[y + i // w][x + i % w] = True

    def site(self, cx, cy, w, h, rmax=8):
        """the free area nearest (cx, cy)"""
        best = None
        for y in range(int(cy) - rmax, int(cy) + rmax):
            for x in range(int(cx) - rmax, int(cx) + rmax):
                if self.free_area(x, y, w, h):
                    d = (x + w / 2 - cx) ** 2 + (y + h / 2 - cy) ** 2
                    if best is None or d < best[0]:
                        best = (d, x, y)
        return best and (best[1], best[2])

    # ---------------------------------------------------------- structures
    def factory(self, island, index):
        cx, cy, r = island
        s = self.site(cx, cy, 3, 3)
        assert s, f'theatre {self.n}: no room for a factory at {island}'
        x, y = s
        self.place(FACTORY, x, y, 3, 3)
        self.objects.append([OBJ_FACTORY, x, y, 0, 0, 0])
        self.road_and_pier(x + 1, y + 1, index)
        # AA turrets around it
        k = 0
        for a in self.rnd.sample(range(8), 8):
            if k == 3:
                break
            ang = a * math.pi / 4
            tx, ty = int(x + 1 + math.cos(ang) * 4.5), int(y + 1 + math.sin(ang) * 4.5)
            if self.free_area(tx, ty, 1, 1, 0):
                self.place(TURRET, tx, ty, 1, 1)
                self.objects.append([OBJ_TURRET, tx, ty, 0, 0, 0])
                k += 1

    def road_and_pier(self, fx, fy, index):
        """From the factory's middle the shortest straight way to the sea that
        looks out to the open sea: a road over the land, a pier on the water."""
        best = None
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            x, y, n = fx + 2 * dx, fy + 2 * dy, 0
            while self.is_land(x, y) or (0 < self.t[y][x] < 16 if 0 <= x < W and 0 <= y < H else False):
                x, y, n = x + dx, y + dy, n + 1
                if n > 20:
                    break
            if self.is_sea(x, y) and self.is_sea(x + 3 * dx, y + 3 * dy) and (best is None or n < best[0]):
                best = (n, dx, dy, x, y)
        if not best:
            return
        n, dx, dy, px, py = best
        x, y = fx + 2 * dx, fy + 2 * dy
        while (x, y) != (px, py):
            if self.is_land(x, y) and not self.used[y][x]:
                self.t[y][x] = ROAD_H if dx else ROAD_V
                self.used[y][x] = True
            x, y = x + dx, y + dy
        self.t[py][px] = PIER_H if dx else PIER_V
        self.objects.append([OBJ_PIER, px, py, index, dx & 0xFF, dy & 0xFF])

    def single(self, island, first, obj, w=1, h=1):
        cx, cy, r = island
        s = self.site(cx + self.rnd.uniform(-2, 2), cy + self.rnd.uniform(-2, 2), w, h)
        assert s, f'theatre {self.n}: no room for object {obj} at {island}'
        self.place(first, s[0], s[1], w, h)
        self.objects.append([obj, s[0], s[1], 0, 0, 0])

    def carrier_lane(self):
        """the longest run of open sea, 5 rows deep, in the middle band"""
        best = None
        for y in range(BANDS[1] + 4, BANDS[2] - 4):
            x = 4
            while x < W - 4:
                if all(self.is_sea(x, y + k) for k in range(-2, 3)):
                    x0 = x
                    while x < W - 4 and all(self.is_sea(x, y + k) for k in range(-2, 3)):
                        x += 1
                    if best is None or x - x0 > best[0]:
                        best = (x - x0, x0, x, y)
                x += 1
        n, x0, x1, y = best
        assert n > 24, f'theatre {self.n}: no room for the carrier'
        self.objects.insert(0, [OBJ_CARRIER, (x0 + x1) // 2, y, x0 + 4, x1 - 4, 0])

    def spawns(self):
        for x, y in ((2, 20), (W - 3, 64), (2, 105), (80, 2), (80, H - 3)):
            self.objects.append([OBJ_SPAWN, x, y, 0, 0, 0])

    # -------------------------------------------------------------- output
    def write(self):
        data = bytearray(b'RRTH' + struct.pack('>HHHH', W, H, len(self.objects), len(BANDS)))
        for row in self.t:
            data += bytes(row)
        for o in self.objects:
            data += bytes(v & 0xFF for v in o)
        for b, pal in zip(BANDS, self.palettes):
            # one sea (deep, wave crests) for all the bands: their seams don't show
            pal = self.palettes[0][:2] + pal[2:]
            data += bytes((b, 0)) + b''.join(struct.pack('>H', int(c, 16)) for c in pal)
        os.makedirs(os.path.join(ROOT, 'disk'), exist_ok=True)
        with open(os.path.join(ROOT, 'disk', f'theatre{self.n}.bin'), 'wb') as f:
            f.write(data)
        os.makedirs(os.path.join(ROOT, 'art', 'maps'), exist_ok=True)
        tmj = {
            'type': 'map', 'version': '1.10', 'orientation': 'orthogonal', 'renderorder': 'right-down',
            'width': W, 'height': H, 'tilewidth': 16, 'tileheight': 16, 'infinite': False,
            'layers': [{'type': 'tilelayer', 'name': 'terrain', 'width': W, 'height': H, 'x': 0, 'y': 0,
                        'opacity': 1, 'visible': True, 'data': [t + 1 for row in self.t for t in row]},
                       {'type': 'objectgroup', 'name': 'objects', 'x': 0, 'y': 0, 'opacity': 1, 'visible': True,
                        'objects': [{'id': i + 1, 'name': '', 'type': str(o[0]), 'x': o[1] * 16, 'y': o[2] * 16,
                                     'width': 16, 'height': 16, 'rotation': 0, 'visible': True}
                                    for i, o in enumerate(self.objects)]}],
            'tilesets': [{'firstgid': 1, 'source': '../tiles.tsx'}],
        }
        with open(os.path.join(ROOT, 'art', 'maps', f'theatre{self.n}.tmj'), 'w') as f:
            json.dump(tmj, f)
        kinds = {}
        for o in self.objects:
            kinds[o[0]] = kinds.get(o[0], 0) + 1
        print(f'theatre {self.n}: {len(data)} bytes, objects {kinds}')


def build(n, seed, islands, palettes, factories, radars, shipyard, airstrip):
    t = Theatre(n, seed, islands, palettes)
    for i, k in enumerate(factories):
        t.factory(islands[k], i)
    for k in radars:
        t.single(islands[k], RADAR, OBJ_RADAR)
    t.single(islands[shipyard], SHIPYARD, OBJ_SHIPYARD, 4, 2)
    t.single(islands[airstrip], AIRSTRIP, OBJ_AIRSTRIP, 5, 1)
    t.carrier_lane()
    t.spawns()
    t.write()


def main():
    # pens: deep sea, wave crest, shallow, surf, beach, land, land dark, bright
    build(1, 11,
          [(28, 20, 10), (78, 17, 12), (128, 22, 11),
           (36, 60, 12), (96, 56, 9), (138, 66, 9),
           (24, 106, 10), (80, 105, 12), (130, 108, 11)],
          [['0x026', '0x6CE', '0x29B', '0xDDD', '0xEC8', '0x6A3', '0x352', '0xFFE'],    # tropical
           ['0x027', '0x6CE', '0x2AC', '0xDDD', '0xDB7', '0x3A4', '0x142', '0xEED'],    # jungle
           ['0x036', '0x7DD', '0x3AA', '0xDDD', '0xED9', '0xAA4', '0x662', '0xFFE']],   # savanna
          factories=[0, 2, 3, 5, 6, 8], radars=[1, 4, 7], shipyard=7, airstrip=4)
    # Ember Flats: desert in the north, red rock, then black volcanic islands
    build(2, 23,
          [(24, 18, 11), (70, 22, 9), (122, 16, 12),
           (30, 64, 9), (82, 60, 12), (136, 58, 10),
           (36, 106, 12), (92, 108, 9), (136, 104, 11)],
          [['0x034', '0x6BC', '0x388', '0xDDD', '0xEB7', '0xC95', '0x863', '0xFED'],    # desert
           ['0x035', '0x6BC', '0x389', '0xDDD', '0xDA7', '0xA53', '0x632', '0xFDB'],    # red rock
           ['0x024', '0x5AB', '0x267', '0xCCC', '0x776', '0x444', '0x222', '0xF84']],   # volcanic
          factories=[0, 2, 3, 5, 6, 8], radars=[1, 4, 7], shipyard=4, airstrip=7)
    # Frostline: sea ice, tundra, snowy rock
    build(3, 37,
          [(30, 22, 12), (84, 16, 10), (132, 24, 10),
           (22, 60, 10), (74, 64, 11), (128, 62, 12),
           (40, 104, 11), (90, 108, 10), (134, 106, 10)],
          [['0x124', '0x9BE', '0x46A', '0xEEF', '0x9AB', '0xBCD', '0x789', '0xFFF'],    # ice (pilot: land off-white so structures read)
           ['0x125', '0x9BE', '0x46A', '0xDDE', '0xBBA', '0x897', '0x564', '0xEEF'],    # tundra
           ['0x013', '0x8AD', '0x359', '0xDDE', '0x99A', '0xABC', '0x667', '0xFFF']],   # snowy rock
          factories=[0, 2, 3, 5, 6, 8], radars=[1, 4, 7], shipyard=1, airstrip=4)


if __name__ == '__main__':
    main()
