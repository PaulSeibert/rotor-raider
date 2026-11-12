// missionmap: see missionmap.h and techniques/missionmap/TECHNIQUE.md.
// Plain C, no libgcc maths.

#include "missionmap.h"

#if defined(__GNUC__) && !defined(__clang__)
#define MM_COLD __attribute__((noinline, optimize("Os")))
#else
#define MM_COLD __attribute__((noinline))
#endif

MM_COLD void mmImageInit(tMmImage *pI, uint8_t *pBuf, uint16_t uwW, uint16_t uwH, uint8_t ubPlanes) {
	pI->pData = pBuf;
	pI->uwW = uwW;
	pI->uwH = uwH;
	pI->ubPlanes = ubPlanes;
	pI->uwRowBytes = (uint16_t)(uwW >> 3);
	pI->uwStride = (uint16_t)(pI->uwRowBytes * ubPlanes);
}

MM_COLD void mmFill(tMmImage *pI, uint8_t ubPen) {
	// longwords (the buffer is longword-aligned and a row a multiple of 4 bytes)
	uint32_t *p = (uint32_t *)pI->pData;
	uint16_t uwLongs = (uint16_t)(pI->uwRowBytes >> 2);
	for(uint16_t y = 0; y < pI->uwH; ++y)
		for(uint8_t pl = 0; pl < pI->ubPlanes; ++pl) {
			uint32_t ulB = (ubPen >> pl) & 1 ? 0xFFFFFFFFUL : 0;
			for(uint16_t k = 0; k < uwLongs; ++k) *p++ = ulB;
		}
}

// the byte of image row y, plane pl, column byte k: 16 x 16 -> 32 bit multiplies
// (mulu.w), not libgcc's 32-bit one (the image is under 64 KB)
static inline uint8_t *mmAt(const tMmImage *pI, uint16_t y, uint8_t pl, uint16_t k) {
	uint16_t uwPl = pl;
	return pI->pData + (uint32_t)y * pI->uwStride + (uint32_t)uwPl * pI->uwRowBytes + k;
}

MM_COLD void mmPixel(tMmImage *pI, int16_t x, int16_t y, uint8_t ubPen) {
	if(x < 0 || y < 0 || x >= (int16_t)pI->uwW || y >= (int16_t)pI->uwH) return;
	uint8_t ubBit = (uint8_t)(0x80 >> (x & 7));
	for(uint8_t pl = 0; pl < pI->ubPlanes; ++pl) {
		uint8_t *p = mmAt(pI, (uint16_t)y, pl, (uint16_t)(x >> 3));
		if((ubPen >> pl) & 1) *p |= ubBit;
		else *p &= (uint8_t)~ubBit;
	}
}

MM_COLD uint8_t mmGet(const tMmImage *pI, int16_t x, int16_t y) {
	if(x < 0 || y < 0 || x >= (int16_t)pI->uwW || y >= (int16_t)pI->uwH) return 0;
	uint8_t ubBit = (uint8_t)(0x80 >> (x & 7)), ubPen = 0;
	const uint8_t *p = mmAt(pI, (uint16_t)y, 0, (uint16_t)(x >> 3));
	for(uint8_t pl = 0; pl < pI->ubPlanes; ++pl, p += pI->uwRowBytes)
		if(*p & ubBit) ubPen |= (uint8_t)(1 << pl);
	return ubPen;
}

// Row y from x0, w pixels, in a pen through a byte pattern (0xFF solid, 0xAA / 0x55
// a checker): a head byte, whole bytes, a tail byte, per plane
MM_COLD static void mmSpan(tMmImage *pI, int16_t x0, int16_t y, int16_t w, uint8_t ubPen, uint8_t ubPat) {
	if(y < 0 || y >= (int16_t)pI->uwH) return;
	int16_t x1 = (int16_t)(x0 + w);
	if(x0 < 0) x0 = 0;
	if(x1 > (int16_t)pI->uwW) x1 = (int16_t)pI->uwW;
	if(x1 <= x0) return;
	uint16_t k0 = (uint16_t)(x0 >> 3), k1 = (uint16_t)((x1 - 1) >> 3);
	uint8_t ubHead = (uint8_t)(0xFF >> (x0 & 7)), ubTail = (uint8_t)(0xFF << (7 - ((x1 - 1) & 7)));
	uint8_t *pRow = mmAt(pI, (uint16_t)y, 0, 0);
	for(uint8_t pl = 0; pl < pI->ubPlanes; ++pl, pRow += pI->uwRowBytes) {
		uint8_t isOn = (ubPen >> pl) & 1;
		if(k0 == k1) {
			uint8_t m = (uint8_t)(ubHead & ubTail & ubPat);
			pRow[k0] = isOn ? (uint8_t)(pRow[k0] | m) : (uint8_t)(pRow[k0] & ~m);
			continue;
		}
		uint8_t m = (uint8_t)(ubHead & ubPat);
		pRow[k0] = isOn ? (uint8_t)(pRow[k0] | m) : (uint8_t)(pRow[k0] & ~m);
		for(uint16_t k = (uint16_t)(k0 + 1); k < k1; ++k) {
			if(isOn) pRow[k] |= ubPat;
			else pRow[k] &= (uint8_t)~ubPat;
		}
		m = (uint8_t)(ubTail & ubPat);
		pRow[k1] = isOn ? (uint8_t)(pRow[k1] | m) : (uint8_t)(pRow[k1] & ~m);
	}
}

MM_COLD void mmBox(tMmImage *pI, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t ubPen, uint8_t ubHatch) {
	for(int16_t j = 0; j < h; ++j) {
		uint8_t ubPat = ubHatch ? (uint8_t)(((y + j + ubHatch) & 1) ? 0x55 : 0xAA) : 0xFF;
		mmSpan(pI, x, (int16_t)(y + j), w, ubPen, ubPat);
	}
}

MM_COLD void mmFrame(tMmImage *pI, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t ubPen) {
	mmSpan(pI, x, y, w, ubPen, 0xFF);
	mmSpan(pI, x, (int16_t)(y + h - 1), w, ubPen, 0xFF);
	for(int16_t j = 1; j < h - 1; ++j) {
		mmPixel(pI, x, (int16_t)(y + j), ubPen);
		mmPixel(pI, (int16_t)(x + w - 1), (int16_t)(y + j), ubPen);
	}
}

MM_COLD void mmArrow(tMmImage *pI, int16_t x, int16_t y, uint8_t ubPen) {
	// 7 wide at the top, a tip at (x, y): rows 3, 2, 1 px wide are 7, 5, 3, 1 (4 rows)
	for(int16_t r = 0; r < 4; ++r) mmSpan(pI, (int16_t)(x - 3 + r), (int16_t)(y - 3 + r), (int16_t)(7 - 2 * r), ubPen, 0xFF);
}

MM_COLD void mmArrowR(tMmImage *pI, int16_t x, int16_t y, uint8_t ubPen) {
	for(int16_t r = -3; r <= 3; ++r) {
		int16_t w = (int16_t)(4 - (r < 0 ? -r : r));
		mmSpan(pI, (int16_t)(x - 3), (int16_t)(y + r), w, ubPen, 0xFF);
	}
}

MM_COLD void mmRouteV(tMmImage *pI, int16_t x, int16_t y0, uint16_t uwStep, uint8_t ubNodes, uint8_t ubCurrent,
	uint8_t ubPenDone, uint8_t ubPenNow, uint8_t ubPenTodo) {
	for(uint8_t i = 0; i < ubNodes; ++i) {
		int16_t y = (int16_t)(y0 + i * (int16_t)uwStep);
		if(i + 1 < ubNodes) {
			// the line to the next node: done only between two done nodes
			uint8_t ubPen = i + 1 < ubCurrent ? ubPenDone : ubPenTodo;
			mmBox(pI, (int16_t)(x + MM_NODE / 2 - 1), (int16_t)(y + MM_NODE), 2, (int16_t)(uwStep - MM_NODE), ubPen, 0);
		}
	}
	for(uint8_t i = 0; i < ubNodes; ++i) {
		int16_t y = (int16_t)(y0 + i * (int16_t)uwStep);
		if(i < ubCurrent) mmBox(pI, x, y, MM_NODE, MM_NODE, ubPenDone, 0);
		else if(i == ubCurrent) {
			mmBox(pI, x, y, MM_NODE, MM_NODE, ubPenNow, 0);
			mmBox(pI, (int16_t)(x + 3), (int16_t)(y + 3), MM_NODE - 6, MM_NODE - 6, 0, 0);
			mmArrowR(pI, (int16_t)(x - 3), (int16_t)(y + MM_NODE / 2), ubPenNow);
		}
		else {
			mmBox(pI, x, y, MM_NODE, MM_NODE, 0, 0);
			mmFrame(pI, x, y, MM_NODE, MM_NODE, ubPenTodo);
		}
	}
}

MM_COLD void mmCopy(const tMmImage *pI, uint8_t *pDst, uint16_t uwDstStep) {
	const uint32_t *pSrc = (const uint32_t *)pI->pData;
	uint16_t uwLongs = (uint16_t)(pI->uwRowBytes >> 2);
	uint16_t uwRows = (uint16_t)(pI->uwH * pI->ubPlanes);   // (16 x 8 bits: mulu.w)
	for(uint16_t y = 0; y < uwRows; ++y, pDst += uwDstStep) {
		uint32_t *pD = (uint32_t *)pDst;   // (an even address: longs are fine on a 68000)
		for(uint16_t k = 0; k < uwLongs; ++k) *pD++ = *pSrc++;
	}
}

// ------------------------------------------------------------------ card ---

MM_COLD void mmCardBegin(tMmCard *pC, uint8_t ubMission, uint8_t ubOf, uint16_t uwFrames, uint8_t ubWipe) {
	pC->uwLeft = uwFrames;
	pC->isFireWas = 1;     // (the press that started the game, or the end screen's, doesn't skip)
	pC->ubMission = ubMission;
	pC->ubOf = ubOf;
	pC->ubWipe = 0;
	pC->ubWipeFrames = ubWipe;
	pC->isEnded = 0;
}

MM_COLD uint8_t mmCardUpdate(tMmCard *pC, uint8_t isFire) {
	uint8_t isPress = isFire && !pC->isFireWas;
	pC->isFireWas = isFire;
	if(pC->isEnded) {
		if(pC->ubWipe && --pC->ubWipe) return MM_CARD_WIPE;
		return MM_CARD_DONE;
	}
	if(pC->uwLeft) --pC->uwLeft;
	if(!isPress && pC->uwLeft) return MM_CARD_SHOW;
	pC->isEnded = 1;
	pC->ubWipe = pC->ubWipeFrames;
	return pC->ubWipeFrames ? MM_CARD_WIPE : MM_CARD_DONE;
}

// ------------------------------------------------------------------ text ---

MM_COLD void mmHeading(char *szOut, uint8_t ubMax, const char *szWord, uint8_t ubN, uint8_t ubOf) {
	uint8_t n = 0;
	for(const char *p = szWord; *p && n < ubMax; ++p) szOut[n++] = *p;
	const char *szNum[2] = {0, 0};
	char szA[3], szB[3];
	// " n OF m": two digits at most each (no divide: subtract tens)
	uint8_t pV[2] = {ubN, ubOf};
	char *pS[2] = {szA, szB};
	for(uint8_t i = 0; i < 2; ++i) {
		uint8_t t = 0, v = pV[i];
		while(v >= 10) { v = (uint8_t)(v - 10); ++t; }
		uint8_t k = 0;
		if(t) pS[i][k++] = (char)('0' + t);
		pS[i][k++] = (char)('0' + v);
		pS[i][k] = 0;
		szNum[i] = pS[i];
	}
	const char *pParts[5] = {" ", szNum[0], " OF ", szNum[1], 0};
	for(uint8_t i = 0; pParts[i]; ++i)
		for(const char *p = pParts[i]; *p && n < ubMax; ++p) szOut[n++] = *p;
	szOut[n] = 0;
}

MM_COLD void mmCentre(char *szOut, const char *szText, uint8_t ubCols) {
	uint8_t n = 0;
	while(szText[n] && n < ubCols) ++n;
	uint8_t ubPad = (uint8_t)((ubCols - n) >> 1);
	for(uint8_t i = 0; i < ubCols; ++i) szOut[i] = ' ';
	for(uint8_t i = 0; i < n; ++i) szOut[ubPad + i] = szText[i];
	szOut[ubCols] = 0;
}
