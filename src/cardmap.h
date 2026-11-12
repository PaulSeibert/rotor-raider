/**
 * ROTOR RAIDER - the theatre card's map picture (portable C, unit-tested on the host).
 * (c) 2026 Desert Palms Software. Game by Paul Ross Seibert.
 *
 * The renderer behind the kit's missionmap technique: the theatre's island from the
 * tile map (a pixel a tile), the factories (standing red, destroyed dark), the
 * shipyard, the carrier and a "you are here" arrow at the gunship, and the route
 * across the three theatres. Drawn once into an image when a theatre loads;
 * main.c copies it into the frame buffer when the card shows.
 */
#ifndef _CARDMAP_H_
#define _CARDMAP_H_

#include "logic.h"
#include "missionmap.h"

// The image (on screen at CARD_X, CARD_Y; 3 planes like PF1)
#define CARD_X 16
#define CARD_Y 36
#define CARD_W 288
#define CARD_H 144
#define CARD_PLANES 3
#define CARD_STRIP_ROWS 16         // the wipe clears this many rows a picture
// In the image: the map's frame, and the route
#define CARD_MAP_X 8
#define CARD_MAP_Y 2
#define CARD_ROUTE_X 192
#define CARD_ROUTE_Y 24
#define CARD_ROUTE_STEP 40
#define CARD_LEGEND_Y 136          // the legend's squares (their words are text lines)
// The card's pens (PF1's: main.c's card palette recolours 2 and 3)
#define CARD_PEN_SEA 0             // see-through: the card's dark sea behind
#define CARD_PEN_BLACK 1           // a destroyed factory
#define CARD_PEN_COAST 2
#define CARD_PEN_LAND 3
#define CARD_PEN_WHITE 4
#define CARD_PEN_ORANGE 5          // the shipyard
#define CARD_PEN_YELLOW 6          // the carrier
#define CARD_PEN_RED 7             // a factory

void cardMapDraw(tMmImage *pI, const tGame *g);

/** "6 FACTORIES  3 BOMBS EACH  CRUISER 3:00" for the theatre g has loaded (41 bytes). */
void cardBrief(const tGame *g, char *szOut);

#endif // _CARDMAP_H_
