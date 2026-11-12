/**
 * missionmap: a mission preview card (plain C, no Amiga headers, so `agk unit`
 * tests it on the host). techniques/missionmap/TECHNIQUE.md is the spec.
 * Extracted from Rotor Rescue's preview card and used by Rotor Raider 1.5.
 *
 * The card: "MISSION n OF m", the mission's name, a one-line brief and a
 * picture - a map of what's ahead, a "you are here" marker, the route so far.
 *
 *   - the PICTURE is drawn once into an image in ordinary (fast) RAM by the
 *     game's own renderer (mmFill, mmBox, mmArrow... are the drawing helpers),
 *     then copied into chip RAM by the CPU when the card shows (mmCopy). Drawing
 *     it straight into the frame took 1.6 frames in Rotor Rescue; the copy is
 *     longword moves, about half a frame for 15 KB;
 *   - the CARD's clock (mmCardUpdate) knows its length and the fresh-press rule:
 *     fire held from the title or the end screen doesn't skip it, a press after
 *     it was let go does;
 *   - the TEXT helpers (mmHeading, mmCentre) lay the lines out.
 *
 * Images are interleaved like ACE's BMF_INTERLEAVED bitmaps: a row is
 * ubPlanes plane-rows one after another, each uwW / 8 bytes.
 */
#ifndef _MISSIONMAP_H_
#define _MISSIONMAP_H_

#include <stdint.h>

typedef struct {
	uint8_t *pData;        // the caller's buffer, MM_BYTES(w, h, planes) long, longword-aligned
	uint16_t uwW, uwH;     // pixels; uwW a multiple of 32 (the copy moves longwords)
	uint8_t ubPlanes;
	uint16_t uwRowBytes;   // uwW / 8: one plane-row
	uint16_t uwStride;     // ubPlanes x uwRowBytes: one row, all planes
} tMmImage;

#define MM_BYTES(w, h, planes) ((uint32_t)(w) / 8 * (h) * (planes))

/** Set the image up over pBuf (it isn't cleared: mmFill it). */
void mmImageInit(tMmImage *pI, uint8_t *pBuf, uint16_t uwW, uint16_t uwH, uint8_t ubPlanes);

/** Every pixel in pen ubPen. */
void mmFill(tMmImage *pI, uint8_t ubPen);

/** One pixel in a pen (outside the image: nothing). */
void mmPixel(tMmImage *pI, int16_t x, int16_t y, uint8_t ubPen);

/** The pen of a pixel (0 outside), for tests. */
uint8_t mmGet(const tMmImage *pI, int16_t x, int16_t y);

/** A filled box, w x h from (x, y) (clipped); ubHatch: 0 solid, 1 and 2 a checker
 *  of the pen and the pixels already there (the two phases), for "ground held by
 *  the enemy". */
void mmBox(tMmImage *pI, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t ubPen, uint8_t ubHatch);

/** An outline of a box (1 px). */
void mmFrame(tMmImage *pI, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t ubPen);

/** A "you are here" arrow: a downward triangle 7 px wide whose tip is at (x, y). */
void mmArrow(tMmImage *pI, int16_t x, int16_t y, uint8_t ubPen);

/** A right-pointing arrow: 4 px long and 7 tall, the tip at (x, y). */
void mmArrowR(tMmImage *pI, int16_t x, int16_t y, uint8_t ubPen);

/** The route: nodes along a vertical line at x, from y0, uwStep px apart: a node
 *  before ubCurrent is done (a filled box in penDone), the current one a box
 *  in penNow with an arrow pointing at it from the left, the rest hollow in penTodo. The line
 *  between them is penTodo, or penDone between two done nodes. A node is
 *  MM_NODE px square. */
#define MM_NODE 12
void mmRouteV(tMmImage *pI, int16_t x, int16_t y0, uint16_t uwStep, uint8_t ubNodes, uint8_t ubCurrent,
	uint8_t ubPenDone, uint8_t ubPenNow, uint8_t ubPenTodo);

/** Copy the image into a destination bitmap's first plane-row pDst (an even
 *  address). uwDstStep: bytes from one plane-row of the destination to the next
 *  (for an interleaved ACE bitmap: BytesPerRow / planes). The destination has the
 *  same number of planes as the image. */
void mmCopy(const tMmImage *pI, uint8_t *pDst, uint16_t uwDstStep);

// ------------------------------------------------------------------ card ---

typedef struct {
	uint16_t uwLeft;       // frames to the end
	uint8_t isFireWas;     // fire as of last frame (the fresh-press rule)
	uint8_t ubMission, ubOf;
	uint8_t ubWipe;        // the wipe's frames left (0: not wiping)
	uint8_t ubWipeFrames;  // how many it takes
	uint8_t isEnded;       // the card has ended: it is wiping
} tMmCard;

/** The card starts: mission ubMission (1-based) of ubOf, shown for uwFrames, then a wipe of
 *  ubWipe frames (0: none). Fire is taken as held (it was, to start the game): only a press
 *  after it's let go skips. */
void mmCardBegin(tMmCard *pC, uint8_t ubMission, uint8_t ubOf, uint16_t uwFrames, uint8_t ubWipe);

enum { MM_CARD_SHOW, MM_CARD_WIPE, MM_CARD_DONE };

/** One frame: MM_CARD_SHOW while the card is up; MM_CARD_WIPE from the frame its time runs out or
 *  fire is freshly pressed, for ubWipe frames; then MM_CARD_DONE (start the mission).
 *
 *  The wipe is the cost lesson of Rotor Raider's card. The picture sits in the play's own
 *  bitplanes, and clearing a 288 x 144 x 3-plane rectangle is one big blit that the blitter,
 *  starved by six bitplanes of display DMA, takes tens of milliseconds over: queued with the
 *  play's first pictures it dropped a frame (184%; a blit queued behind it made 102% at best).
 *  So the card's end takes pictures of its own: clear the picture a STRIP at a time
 *  while mmCardWiping(), the card wiping away top to bottom, with nothing else going on;
 *  the mission starts on a light picture. A game whose picture is small or whose bus has room can
 *  use a wipe of 0 and clear it in one go. */
uint8_t mmCardUpdate(tMmCard *pC, uint8_t isFire);

/** Non-zero while the wipe runs, as of the start of a picture (before mmCardUpdate): clear
 *  a strip of the picture in the buffer this picture draws. */
static inline uint8_t mmCardWiping(const tMmCard *pC) { return pC->ubWipe; }

/** Which strip to clear in this picture (0, 0, 1, 1, 2, 2 ...: two buffers take a picture
 *  each): a wipe of 2 x n frames clears n strips of the picture's height / n rows. */
static inline uint8_t mmCardWipeStrip(const tMmCard *pC) { return (uint8_t)((pC->ubWipeFrames - pC->ubWipe) >> 1); }

// ------------------------------------------------------------------ text ---

/** "MISSION 2 OF 3" into szOut (at most ubMax + 1 bytes): szWord is the game's word
 *  (MISSION, THEATRE, LEVEL...); n and of are 1-99. */
void mmHeading(char *szOut, uint8_t ubMax, const char *szWord, uint8_t ubN, uint8_t ubOf);

/** szText centred in ubCols columns of spaces, szOut ubCols + 1 bytes (cut if longer). */
void mmCentre(char *szOut, const char *szText, uint8_t ubCols);

#endif // _MISSIONMAP_H_
