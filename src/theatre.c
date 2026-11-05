// ROTOR RAIDER - reading a theatre file (portable C, unit-tested on the host).
// (c) 2026 Desert Palms Software. Game by Paul Ross Seibert.

#include "logic.h"

static uint16_t word(const uint8_t *p) {
	return (uint16_t)((p[0] << 8) | p[1]);
}

uint8_t theatreParse(uint8_t *pData, uint32_t ulSize, tTheatre *pOut) {
	if(ulSize < THEATRE_HEADER || pData[0] != 'R' || pData[1] != 'R' || pData[2] != 'T' || pData[3] != 'H') return 0;
	uint16_t uwW = word(pData + 4), uwH = word(pData + 6), uwObjs = word(pData + 8), uwBands = word(pData + 10);
	if(!uwW || !uwH || uwW > MAP_W_MAX || uwH > MAP_H_MAX || uwBands > BANDS_MAX) return 0;
	uint32_t ulTiles = (uint32_t)uwW * uwH;
	if(ulSize < THEATRE_HEADER + ulTiles + 6UL * uwObjs + 18UL * uwBands) return 0;
	pOut->map.pTiles = pData + THEATRE_HEADER;
	pOut->map.uwW = uwW;
	pOut->map.uwH = uwH;
	pOut->pObjs = (const tTheatreObj *)(pData + THEATRE_HEADER + ulTiles);
	pOut->uwObjs = uwObjs;
	const uint8_t *p = pData + THEATRE_HEADER + ulTiles + 6UL * uwObjs;
	pOut->ubBands = (uint8_t)uwBands;
	for(uint8_t b = 0; b < uwBands; ++b, p += 18) {
		pOut->pBands[b].ubRow = p[0];
		for(uint8_t i = 0; i < 8; ++i) pOut->pBands[b].pColors[i] = word(p + 2 + 2 * i);
	}
	return 1;
}
