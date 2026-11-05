// ROTOR RAIDER - the 8-way terrain scroller (PF2, 3 bitplanes).
// (c) 2026 Desert Palms Software. Game by Paul Ross Seibert.
//
// Each plane is one ring of SCROLL_ROWS rows of SCROLL_BW bytes, and world
// pixel (x, y) lives at byte (y * SCROLL_BW + x / 8) mod SCROLL_RING. Moving
// right by SCROLL_BW * 8 px is the same as moving down a row, so the ring
// works for any camera position: scrolling is only moving the start pointer
// (and BPLCON1's fine scroll). The picture is consistent as long as the tiles
// held span no more than 24 columns and the ring's rows: 23 x 17 tiles are
// (the 21 words a line fetches, a tile of margin all round).
//
// Where the picture reaches the end of the ring, the copper points the
// planes back at its start (from the start of that line, before the fetch).
// The line before may run up to 42 bytes past the end: a guard row after the
// ring holds a copy of the ring's first row.
//
// New tiles come into the margin (a column and/or a row a frame at most):
// one small blit per tile and plane, A -> D.

#include "scroll.h"
#include <ace/managers/blit.h>
#include <ace/managers/memory.h>
#include <ace/utils/custom.h>
#include <agk/debug.h>

#define PLANE_BYTES SCROLL_PLANE_BYTES
#define QUEUE_MAX 160
#define TILES_MAX 128
#define HELD_W 23
#define HELD_H 17

static UBYTE *s_pRing;                  // chip RAM: the 3 planes, one after another
static const UBYTE *s_pTileSrc[TILES_MAX];   // plane 0 of each tile in the tileset
static UWORD s_uwTileMod, s_uwTilePlane;     // tileset: modulo for a tile, plane to plane
static WORD s_wHX0, s_wHY0;             // the tiles held: [X0, X0 + HELD_W) x [Y0, Y0 + HELD_H)
static UBYTE s_isGuardDirty;
typedef struct { WORD wTx, wTy; UWORD uwPos; UBYTE ubTile; } tQueued;
static tQueued s_pQueue[QUEUE_MAX];     // tiles that came into the margin, to draw
static UWORD s_uwQueued;
static UWORD s_pTileRowOffs[TILE + 1];  // tileset: bytes to a tile's row r (no multiplies a frame)

void scrollCreate(const tBitMap *pTiles) {
	s_pRing = memAllocChipClear(PLANE_BYTES * SCROLL_PLANES);
	UWORD uwRow = pTiles->BytesPerRow;   // interleaved: from a row to the next
	s_uwTileMod = (UWORD)(uwRow - 2);
	s_uwTilePlane = (UWORD)(pTiles->Planes[1] - pTiles->Planes[0]);
	UWORD uwCount = (UWORD)(pTiles->Rows / TILE);
	for(UWORD r = 0; r <= TILE; ++r) s_pTileRowOffs[r] = (UWORD)(r * uwRow);
	for(UWORD t = 0; t < TILES_MAX; ++t) {
		s_pTileSrc[t] = pTiles->Planes[0] + (ULONG)(t < uwCount ? t : 0) * TILE * uwRow;
	}
}

void scrollDestroy(void) {
	blitWait();
	memFree(s_pRing, PLANE_BYTES * SCROLL_PLANES);
}

// The ring position (bytes) of tile (tx, ty)'s top-left
static UWORD ringPos(WORD wTx, WORD wTy) {
	WORD y = (WORD)(wTy << 4);
	while(y < 0) y = (WORD)(y + SCROLL_ROWS);
	while(y >= SCROLL_ROWS) y = (WORD)(y - SCROLL_ROWS);
	WORD p = (WORD)((y << 5) + (y << 4) + (wTx << 1));   // y * 48 + tx * 2
	if(p < 0) p = (WORD)(p + SCROLL_RING);
	if(p >= SCROLL_RING) p = (WORD)(p - SCROLL_RING);
	return (UWORD)p;
}

static UBYTE tileAt(const tMap *pMap, WORD wTx, WORD wTy) {
	if(wTx < 0 || wTy < 0 || wTx >= (WORD)pMap->uwW || wTy >= (WORD)pMap->uwH) return 0;
	return pMap->pTiles[(UWORD)wTy * pMap->uwW + (UWORD)wTx];
}

// The blitter's constant registers for a run of tile blits
static void blitsBegin(void) {
	blitWait();
	g_pCustom->bltcon0 = 0x09F0;    // A -> D
	g_pCustom->bltcon1 = 0;
	g_pCustom->bltafwm = 0xFFFF;
	g_pCustom->bltalwm = 0xFFFF;
	g_pCustom->bltamod = s_uwTileMod;
	g_pCustom->bltdmod = SCROLL_BW - 2;
}

// Waiting on the blitter without a call (the CPU sets up the next blit while
// the last one runs: a wait is short)
static inline void blitWaitFast(void) {
	(void)g_pCustom->dmaconr;      // (the first read can miss a blit just started)
	while(g_pCustom->dmaconr & DMAF_BLTDONE) continue;
}

static inline void blitPiece(const UBYTE *pSrc, UBYTE *pDst, UWORD uwRows) {
	blitWaitFast();
	g_pCustom->bltapt = (APTR)pSrc;
	g_pCustom->bltdpt = (APTR)pDst;
	g_pCustom->bltsize = (UWORD)((uwRows << 6) | 1);
}

static void drawTileAt(UBYTE ubTile, UWORD uwPos) {
	if(ubTile >= TILES_MAX) ubTile = 0;
	const UBYTE *pSrc = s_pTileSrc[ubTile];
	UBYTE *pPlane = s_pRing;
	if(uwPos < SCROLL_RING - (TILE - 1) * SCROLL_BW) {
		// the usual case: 16 rows in one piece
		if(uwPos < SCROLL_BW) s_isGuardDirty = 1;
		UBYTE *pDst = pPlane + uwPos;
		for(UBYTE p = 0; p < SCROLL_PLANES; ++p) {
			blitWaitFast();
			g_pCustom->bltapt = (APTR)pSrc;
			g_pCustom->bltdpt = (APTR)pDst;
			g_pCustom->bltsize = (TILE << 6) | 1;
			pSrc += s_uwTilePlane;
			pDst += PLANE_BYTES;
		}
		return;
	}
	// across the ring's end: the rows before it, and the rest from its start
	UWORD uwRows1 = 0;
	for(UWORD p = uwPos; p < SCROLL_RING; p = (UWORD)(p + SCROLL_BW)) ++uwRows1;
	UWORD uwPos2 = (UWORD)(uwPos + uwRows1 * SCROLL_BW - SCROLL_RING);
	if(uwPos2 < SCROLL_BW) s_isGuardDirty = 1;
	for(UBYTE p = 0; p < SCROLL_PLANES; ++p) {
		blitPiece(pSrc, pPlane + uwPos, uwRows1);
		blitPiece(pSrc + s_pTileRowOffs[uwRows1], pPlane + uwPos2, (UWORD)(TILE - uwRows1));
		pSrc += s_uwTilePlane;
		pPlane += PLANE_BYTES;
	}
}

static inline void drawTile(UBYTE ubTile, WORD wTx, WORD wTy) {
	drawTileAt(ubTile, ringPos(wTx, wTy));
}

static void guardUpdate(void) {
	blitWaitFast();
	if(!s_isGuardDirty) return;
	s_isGuardDirty = 0;
	UBYTE *pPlane = s_pRing;
	for(UBYTE p = 0; p < SCROLL_PLANES; ++p, pPlane += PLANE_BYTES) {
		const ULONG *pSrc = (const ULONG *)pPlane;
		ULONG *pDst = (ULONG *)(pPlane + SCROLL_RING);
		for(UBYTE i = 0; i < SCROLL_BW / 4; ++i) pDst[i] = pSrc[i];
	}
}

static inline WORD heldX0(int16_t wCamX) { return (WORD)(((wCamX - 1) >> 4) - 1); }
static inline WORD heldY0(int16_t wCamY) { return (WORD)((wCamY >> 4) - 1); }

void scrollReset(const tMap *pMap, int16_t wCamX, int16_t wCamY) {
	s_wHX0 = heldX0(wCamX);
	s_wHY0 = heldY0(wCamY);
	blitsBegin();
	for(WORD y = s_wHY0; y < s_wHY0 + HELD_H; ++y)
		for(WORD x = s_wHX0; x < s_wHX0 + HELD_W; ++x) drawTile(tileAt(pMap, x, y), x, y);
	guardUpdate();
}

static inline void enqueue(const tMap *pMap, WORD x, WORD y, UWORD uwPos) {
	if(s_uwQueued < QUEUE_MAX) {
		tQueued *q = &s_pQueue[s_uwQueued++];
		q->wTx = x;
		q->wTy = y;
		q->uwPos = uwPos;
		q->ubTile = tileAt(pMap, x, y);
	}
}

// ring positions along a column (a tile down: 16 rows) and a row (2 bytes)
static inline UWORD posDown(UWORD uwPos) {
	uwPos = (UWORD)(uwPos + TILE * SCROLL_BW);
	return uwPos >= SCROLL_RING ? (UWORD)(uwPos - SCROLL_RING) : uwPos;
}
static inline UWORD posRight(UWORD uwPos) {
	uwPos = (UWORD)(uwPos + 2);
	return uwPos >= SCROLL_RING ? (UWORD)(uwPos - SCROLL_RING) : uwPos;
}

UWORD scrollUpdate(const tMap *pMap, int16_t wCamX, int16_t wCamY) {
	WORD wX0 = heldX0(wCamX), wY0 = heldY0(wCamY);
	if(wX0 == s_wHX0 && wY0 == s_wHY0) return 0;
	if(wX0 - s_wHX0 > 1 || s_wHX0 - wX0 > 1 || wY0 - s_wHY0 > 1 || s_wHY0 - wY0 > 1 ||
		s_uwQueued > QUEUE_MAX - HELD_W - HELD_H) {
		s_uwQueued = 0;
		scrollReset(pMap, wCamX, wCamY);   // a jump: everything again, at once
		return HELD_W * HELD_H;
	}
	UWORD uwBefore = s_uwQueued;
	// a new column, over the new rows' span
	if(wX0 != s_wHX0) {
		WORD x = wX0 < s_wHX0 ? wX0 : (WORD)(wX0 + HELD_W - 1);
		UWORD uwPos = ringPos(x, wY0);
		for(WORD y = wY0; y < wY0 + HELD_H; ++y, uwPos = posDown(uwPos)) enqueue(pMap, x, y, uwPos);
	}
	// a new row, over the columns kept
	if(wY0 != s_wHY0) {
		WORD y = wY0 < s_wHY0 ? wY0 : (WORD)(wY0 + HELD_H - 1);
		WORD xa = wX0 > s_wHX0 ? wX0 : s_wHX0, xb = wX0 < s_wHX0 ? (WORD)(wX0 + HELD_W) : (WORD)(s_wHX0 + HELD_W);
		if(wX0 < s_wHX0) ++xa;          // the new column (on the left) has it already
		if(wX0 > s_wHX0) --xb;          // ... or on the right
		UWORD uwPos = ringPos(xa, y);
		for(WORD x = xa; x < xb; ++x, uwPos = posRight(uwPos)) enqueue(pMap, x, y, uwPos);
	}
	s_wHX0 = wX0;
	s_wHY0 = wY0;
	return (UWORD)(s_uwQueued - uwBefore);
}

UWORD scrollDrainFor(UWORD uwCamStep) {
	if(!s_uwQueued) return 0;
	// A queued tile is a tile beyond the fetch: it shows once the camera has
	// moved up to 16 px further. Spread the work over the frames that leaves
	// (a blit stalls the CPU too: its code is on the same bus).
	UWORD uwFrames = 1;
	if(uwCamStep < 8) {
		UWORD uwStep = uwCamStep ? uwCamStep : 1;
		for(UWORD d = (UWORD)(uwStep * 2); d < TILE; d = (UWORD)(d + uwStep)) ++uwFrames;
	}
	// (queued + frames - 1) / frames, by the 68000's 16-bit divide
	ULONG ulQ = (ULONG)(s_uwQueued + uwFrames - 1);
	__asm__("divu.w %1,%0" : "+d"(ulQ) : "d"(uwFrames));
	UWORD uwBudget = (UWORD)ulQ;
	if(uwBudget < SCROLL_DRAIN_MIN) uwBudget = SCROLL_DRAIN_MIN;
	if(uwBudget > SCROLL_DRAIN_MAX) uwBudget = SCROLL_DRAIN_MAX;   // (the blit queue holds 256)
	return scrollDrain(uwBudget);
}

// Tiles are blitted directly, not through the blit queue: here the constant
// registers are set once for the run and each blit needs 3 writes, where a
// queued blit loads all 13 (measured: 24 raster lines a frame against 42)
UWORD scrollDrain(UWORD uwBudget) {
	if(!s_uwQueued) return 0;
	blitsBegin();
	// oldest first: the column or row the camera reaches first
	UWORD i = 0;
	for(; i < s_uwQueued && uwBudget; ++i, --uwBudget) {
		const tQueued *q = &s_pQueue[i];
		// a tile the camera has left again since: skip it
		if(q->wTx < s_wHX0 || q->wTx >= s_wHX0 + HELD_W || q->wTy < s_wHY0 || q->wTy >= s_wHY0 + HELD_H) continue;
		drawTileAt(q->ubTile, q->uwPos);
	}
	UWORD uwLeft = (UWORD)(s_uwQueued - i);
	for(UWORD j = 0; j < uwLeft; ++j) s_pQueue[j] = s_pQueue[i + j];
	s_uwQueued = uwLeft;
	guardUpdate();
	return uwLeft;
}

// A tile changed in the map: queued like a tile coming into view (drawn
// within a picture or two, with the others, not at once: a blit and a wait
// for each of a factory's 9 tiles cost a picture)
void scrollTileChanged(const tMap *pMap, UWORD uwTx, UWORD uwTy) {
	WORD x = (WORD)uwTx, y = (WORD)uwTy;
	if(x < s_wHX0 || x >= s_wHX0 + HELD_W || y < s_wHY0 || y >= s_wHY0 + HELD_H) return;
	enqueue(pMap, x, y, ringPos(x, y));
}

UWORD scrollDelayBits(int16_t wCamX) {
	return (UWORD)(logicScrollDelay((UWORD)wCamX) << 4);
}

ULONG scrollTop(int16_t wCamX, int16_t wCamY, UWORD *pWrapLine, ULONG *pWrapAddr) {
	// the start: row R, column C (bytes, 0..47) of the ring
	WORD r = wCamY;
	while(r >= SCROLL_ROWS) r = (WORD)(r - SCROLL_ROWS);
	WORD c = logicScrollByteOffset((UWORD)wCamX);
	while(c < 0) { c = (WORD)(c + SCROLL_BW); --r; }
	while(c >= SCROLL_BW) { c = (WORD)(c - SCROLL_BW); ++r; }
	if(r < 0) r = (WORD)(r + SCROLL_ROWS);
	if(r >= SCROLL_ROWS) r = (WORD)(r - SCROLL_ROWS);
	*pWrapLine = (UWORD)(SCROLL_ROWS - r);   // the first line that starts past the end
	*pWrapAddr = (ULONG)s_pRing + (UWORD)c;
	return (ULONG)s_pRing + (UWORD)((r << 5) + (r << 4) + c);
}
