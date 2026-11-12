// ROTOR RAIDER - the theatre card's map picture (see cardmap.h).
// (c) 2026 Desert Palms Software. Game by Paul Ross Seibert.

#include "cardmap.h"

// Code that runs once per theatre: small, not inlined
#if defined(__GNUC__) && !defined(__clang__)
#define COLD __attribute__((noinline, optimize("Os")))
#else
#define COLD __attribute__((noinline))
#endif

// a tile's pen on the map: land (a plain one or a structure's), coast, or the sea (0: the image's own)
static uint8_t tilePen(uint8_t t) {
	if(t >= TILE_LAND0 || t == TILE_LAND_PLAIN) return CARD_PEN_LAND;
	if(t > 0 && t < TILE_SEA0) return CARD_PEN_COAST;
	return CARD_PEN_SEA;
}

COLD void cardMapDraw(tMmImage *pI, const tGame *g) {
	const tMap *pMap = &g->map;
	const int16_t x0 = CARD_MAP_X + 1, y0 = CARD_MAP_Y + 1;
	mmFill(pI, 0);
	mmFrame(pI, CARD_MAP_X, CARD_MAP_Y, (int16_t)(pMap->uwW + 2), (int16_t)(pMap->uwH + 2), CARD_PEN_COAST);
	// the island: runs of one pen along each row of tiles
	const uint8_t *pRow = pMap->pTiles;
	for(uint16_t ty = 0; ty < pMap->uwH; ++ty, pRow += pMap->uwW) {
		uint16_t tx = 0;
		while(tx < pMap->uwW) {
			uint8_t ubPen = tilePen(pRow[tx]);
			uint16_t tx1 = (uint16_t)(tx + 1);
			while(tx1 < pMap->uwW && tilePen(pRow[tx1]) == ubPen) ++tx1;
			if(ubPen) mmBox(pI, (int16_t)(x0 + tx), (int16_t)(y0 + ty), (int16_t)(tx1 - tx), 1, ubPen, 0);
			tx = tx1;
		}
	}
	// the structures that matter: factories (red; destroyed, a dark mark), the shipyard
	for(uint8_t i = 0; i < g->ubStructs; ++i) {
		const tStruct *s = &g->pStructs[i];
		if(s->type == OBJ_FACTORY) {
			mmBox(pI, (int16_t)(x0 + s->tx), (int16_t)(y0 + s->ty), 3, 3, s->hp ? CARD_PEN_RED : CARD_PEN_BLACK, 0);
		}
		else if(s->type == OBJ_SHIPYARD) mmBox(pI, (int16_t)(x0 + s->tx), (int16_t)(y0 + s->ty), 4, 2, CARD_PEN_ORANGE, 0);
		else if(s->type == OBJ_AIRSTRIP) mmBox(pI, (int16_t)(x0 + s->tx), (int16_t)(y0 + s->ty), 5, 1, CARD_PEN_WHITE, 0);
	}
	// the carrier (96 px: 6 tiles) and the "you are here" arrow over the gunship, which starts on its deck
	if(g->carrier.hp) {
		int16_t cx = (int16_t)(g->carrier.x >> 4), cy = (int16_t)(g->carrier.y >> 4);
		mmBox(pI, (int16_t)(x0 + cx - 3), (int16_t)(y0 + cy - 1), 6, 3, CARD_PEN_YELLOW, 0);
	}
	mmArrow(pI, (int16_t)(x0 + (g->chop.x >> 4)), (int16_t)(y0 + (g->chop.y >> 4) - 3), CARD_PEN_WHITE);
	// the route across the theatres, and what the legend's squares say
	mmRouteV(pI, CARD_ROUTE_X, CARD_ROUTE_Y, CARD_ROUTE_STEP, THEATRES, g->theatre, CARD_PEN_YELLOW, CARD_PEN_WHITE, CARD_PEN_LAND);
	mmBox(pI, 8, CARD_LEGEND_Y, 6, 6, CARD_PEN_RED, 0);
	mmBox(pI, 112, CARD_LEGEND_Y, 6, 6, CARD_PEN_YELLOW, 0);
	mmBox(pI, 216, CARD_LEGEND_Y, 6, 6, CARD_PEN_WHITE, 0);
}

COLD void cardBrief(const tGame *g, char *szOut) {
	// "N FACTORIES  B BOMBS EACH  CRUISER M:SS"
	uint8_t n = 0;
	uint8_t ubF = g->ubFactories, ubB = (uint8_t)(logicFactoryHp(g) / BOMB_DAMAGE);
	uint16_t uwFrames = logicCruiserBuild(g);   // 50 a second
	uint8_t ubMin = 0;
	while(uwFrames >= 3000) { uwFrames = (uint16_t)(uwFrames - 3000); ++ubMin; }
	uint8_t ubSec = 0;
	while(uwFrames >= 50) { uwFrames = (uint16_t)(uwFrames - 50); ++ubSec; }
	const char *pParts[7] = {" FACTORIES  ", " BOMBS EACH  CRUISER ", ":", 0, 0, 0, 0};
	char szF[3], szB[3], szM[3], szS[3];
	char *pD[4] = {szF, szB, szM, szS};
	uint8_t pV[4] = {ubF, ubB, ubMin, ubSec};
	for(uint8_t i = 0; i < 4; ++i) {   // each 0-99: tens and units, by subtraction
		uint8_t t = 0, v = pV[i], k = 0;
		while(v >= 10) { v = (uint8_t)(v - 10); ++t; }
		if(t || i == 3) pD[i][k++] = (char)('0' + t);
		pD[i][k++] = (char)('0' + v);
		pD[i][k] = 0;
	}
	const char *pList[7] = {szF, pParts[0], szB, pParts[1], szM, pParts[2], szS};
	for(uint8_t i = 0; i < 7; ++i)
		for(const char *p = pList[i]; *p && n < 40; ++p) szOut[n++] = *p;
	szOut[n] = 0;
}
