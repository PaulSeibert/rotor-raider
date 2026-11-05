// ROTOR RAIDER - the 8-way terrain scroller (PF2). (c) 2026 Desert Palms Software.
#ifndef _SCROLL_H_
#define _SCROLL_H_

#include <ace/types.h>
#include <ace/utils/bitmap.h>
#include "logic.h"

#define SCROLL_BW 48            // bytes per ring row, per plane (384 px)
#define SCROLL_ROWS 280         // ring rows: 17 tile rows + the wrap's slack
#define SCROLL_RING (SCROLL_BW * SCROLL_ROWS)
#define SCROLL_PLANES 3
#define SCROLL_FETCH 42         // bytes a line fetches: 320 px + the early word

/** pTiles: the tileset (16 px wide, tiles stacked, 3 planes, interleaved). */
void scrollCreate(const tBitMap *pTiles);
void scrollDestroy(void);

/** The whole visible area for the camera, from the map (at the start). */
void scrollReset(const tMap *pMap, int16_t wCamX, int16_t wCamY);

/** For a camera move of up to a tile a frame: queues the tiles that came
 *  into the margin (scrollDrain draws them). Returns how many it queued. */
UWORD scrollUpdate(const tMap *pMap, int16_t wCamX, int16_t wCamY);

/** A tile changed in the map (destroyed, rebuilt): draw it if it's held. */
void scrollTileChanged(const tMap *pMap, UWORD uwTx, UWORD uwTy);

/** Draws up to uwBudget of the queued tiles; returns how many are left. */
UWORD scrollDrain(UWORD uwBudget);
/** Draws as many as the camera's speed (px a frame, the faster axis) needs
 *  now to have them all before they show. */
UWORD scrollDrainFor(UWORD uwCamStep);
#define SCROLL_DRAIN_MIN 4
#define SCROLL_DRAIN_MAX 20

/** For the copper: plane 0's address at the top of the play area, and the
 *  line where the ring wraps (PLAY_H or more: none) with plane 0's address
 *  there. Plane p is SCROLL_PLANE_BYTES * p further on. */
ULONG scrollTop(int16_t wCamX, int16_t wCamY, UWORD *pWrapLine, ULONG *pWrapAddr);
UWORD scrollDelayBits(int16_t wCamX);   // BPLCON1's PF2 nibble, shifted into place
#define SCROLL_PLANE_BYTES (SCROLL_RING + SCROLL_BW)   // the ring and its guard row

#endif // _SCROLL_H_
