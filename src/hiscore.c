// hiscore: see hiscore.h and techniques/hiscore/TECHNIQUE.md.
// Plain C: no Amiga headers, no libgcc maths (a 68000 game's per-frame code
// must not call __udivsi3), so it also builds for `agk unit`.

#include "hiscore.h"

// Code that runs between games (menus, the hall of fame): small, not inlined
// into a game's per-frame update
#if defined(__GNUC__) && !defined(__clang__)
#define HI_COLD __attribute__((noinline, optimize("Os")))
#else
#define HI_COLD __attribute__((noinline))
#endif

HI_COLD void hiDefaults(tHiTable *pT, const tHiScore *pDefaults) {
	for(uint8_t i = 0; i < HI_COUNT; ++i) pT->pEntries[i] = pDefaults[i];
}

HI_COLD uint8_t hiRank(const tHiTable *pT, uint32_t ulScore) {
	if(!ulScore) return HI_COUNT;
	uint8_t r = 0;
	while(r < HI_COUNT && pT->pEntries[r].score >= ulScore) ++r;   // below an equal score
	return r;
}

HI_COLD uint8_t hiQualify(const tHiTable *pT, uint32_t ulScore, uint8_t isInvincible, uint8_t isDemo) {
	if(isInvincible || isDemo) return HI_COUNT;
	return hiRank(pT, ulScore);
}

HI_COLD uint8_t hiInsert(tHiTable *pT, uint32_t ulScore, uint8_t ubReached, uint16_t uwExtra) {
	uint8_t r = hiRank(pT, ulScore);
	if(r >= HI_COUNT) return HI_COUNT;
	for(uint8_t i = HI_COUNT - 1; i > r; --i) pT->pEntries[i] = pT->pEntries[i - 1];
	tHiScore *s = &pT->pEntries[r];
	s->name[0] = 'A';
	for(uint8_t c = 1; c < HI_NAME; ++c) s->name[c] = ' ';
	s->score = ulScore;
	s->reached = ubReached;
	s->extra = uwExtra;
	return r;
}

// ----------------------------------------------------------------- file ---
// Big-endian, so the file reads the same on any machine: the magic, then per
// entry the initials, reached, extra (2), score (4), then a 16-bit sum.

HI_COLD static uint16_t hiSum(const uint8_t *p) {
	uint16_t s = 0x5252;
	for(uint8_t i = 0; i < HI_BYTES - 2; ++i) s = (uint16_t)(((s << 1) | (s >> 15)) + p[i]);
	return s;
}

HI_COLD void hiPack(const tHiTable *pT, const tHiConfig *pCfg, uint8_t *pOut) {
	uint8_t *p = pOut;
	for(uint8_t i = 0; i < 4; ++i) *p++ = (uint8_t)pCfg->szMagic[i];
	for(uint8_t i = 0; i < HI_COUNT; ++i) {
		const tHiScore *s = &pT->pEntries[i];
		for(uint8_t c = 0; c < HI_NAME; ++c) *p++ = (uint8_t)s->name[c];
		*p++ = s->reached;
		*p++ = (uint8_t)(s->extra >> 8);
		*p++ = (uint8_t)s->extra;
		for(int8_t b = 24; b >= 0; b -= 8) *p++ = (uint8_t)(s->score >> b);
	}
	uint16_t uwSum = hiSum(pOut);
	*p++ = (uint8_t)(uwSum >> 8);
	*p = (uint8_t)uwSum;
}

// A character a name may hold: the stick's letters, and the digits the keyboard
// types (hiKeyChar). Without the digits a name like "K1 " saved fine and was
// thrown away at the next boot.
HI_COLD static uint8_t hiIsLetter(uint8_t c) {
	if(c >= '0' && c <= '9') return 1;
	for(const char *p = HI_LETTERS; *p; ++p) if((uint8_t)*p == c) return 1;
	return 0;
}

HI_COLD uint8_t hiUnpack(tHiTable *pT, const tHiConfig *pCfg, const uint8_t *pIn) {
	for(uint8_t i = 0; i < 4; ++i) if(pIn[i] != (uint8_t)pCfg->szMagic[i]) return 0;
	if(hiSum(pIn) != (uint16_t)((pIn[HI_BYTES - 2] << 8) | pIn[HI_BYTES - 1])) return 0;
	tHiScore pNew[HI_COUNT];
	const uint8_t *p = pIn + 4;
	for(uint8_t i = 0; i < HI_COUNT; ++i) {
		tHiScore *s = &pNew[i];
		for(uint8_t c = 0; c < HI_NAME; ++c) {
			if(!hiIsLetter(*p)) return 0;
			s->name[c] = (char)*p++;
		}
		s->reached = *p++;
		s->extra = (uint16_t)((p[0] << 8) | p[1]);
		p += 2;
		s->score = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
		p += 4;
		// best first, and sane
		if(!s->reached || s->reached > pCfg->ubReachedMax || (i && s->score > pNew[i - 1].score)) return 0;
	}
	for(uint8_t i = 0; i < HI_COUNT; ++i) pT->pEntries[i] = pNew[i];
	return 1;
}

// -------------------------------------------------------------- initials ---

HI_COLD void hiEditBegin(tHiEdit *pE, uint8_t ubRank, uint16_t uwFrames) {
	pE->ubRank = ubRank;
	pE->ubPos = 0;
	pE->ubHold = 0;
	pE->isFireWas = 1;     // (fire held from the end screen doesn't choose a letter)
	pE->isLeftWas = 0;
	pE->uwLeft = uwFrames;
	pE->uwFrames = uwFrames;
}

HI_COLD uint8_t hiEditUpdate(tHiEdit *pE, tHiTable *pT, int8_t dx, int8_t dy, uint8_t isFire) {
	if(pE->ubPos >= HI_NAME) return 1;
	tHiScore *s = &pT->pEntries[pE->ubRank];
	char *pC = &s->name[pE->ubPos];
	uint8_t isFireEdge = isFire && !pE->isFireWas;
	uint8_t isLeftEdge = dx < 0 && !pE->isLeftWas;
	pE->isFireWas = isFire;
	pE->isLeftWas = dx < 0;
	if(pE->uwLeft) --pE->uwLeft;
	// up/down: the next or previous letter, at once and then repeating
	if(dy) {
		if(!pE->ubHold) {
			const char *pL = HI_LETTERS;
			uint8_t n = 0, i = 0;
			while(pL[n]) ++n;
			while(i < n && pL[i] != *pC) ++i;
			i = (uint8_t)(dy < 0 ? (i + 1 >= n ? 0 : i + 1) : (i ? i - 1 : n - 1));
			*pC = pL[i];
			pE->ubHold = HI_EDIT_REPEAT;
		}
		else --pE->ubHold;
	}
	else pE->ubHold = 0;
	if(isLeftEdge && pE->ubPos) --pE->ubPos;   // back a letter
	if(isFireEdge) {
		if(++pE->ubPos < HI_NAME && s->name[pE->ubPos] == ' ') s->name[pE->ubPos] = *pC;   // start from the last letter
	}
	if(pE->ubPos >= HI_NAME || !pE->uwLeft) {
		pE->ubPos = HI_NAME;
		return 1;
	}
	return 0;
}

// What each raw keycode 0x00-0x40 types (0: nothing). Upper case; the same
// key with or without Shift is the same code. Note raw 0x01 is '1' (the first
// key after the backtick) and raw 0x00 is the backtick itself, which types
// nothing; the digit row is 0x01-0x0A.
static const char s_pKeyChars[0x41] = {
	0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 0, 0, 0, 0, '0',             // 0x00: ` 1-9 0 - = backslash, none, keypad 0
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 0, 0, 0, '1', '2', '3',         // 0x10: row 1, [ ], (none), keypad 1-3
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0, 0, 0, 0, '4', '5', '6',           // 0x20: row 2, ; ' #, (none), keypad 4-6
	0, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', 0, 0, 0, 0, 0, '7', '8', '9',               // 0x30: < row 3 , . /, (none), keypad 7-9
	' ',                                                                              // 0x40: Space
};

HI_COLD char hiKeyChar(uint8_t ubRawKey) {
	return ubRawKey < sizeof(s_pKeyChars) ? s_pKeyChars[ubRawKey] : 0;
}

HI_COLD uint8_t hiEditKey(tHiEdit *pE, tHiTable *pT, uint8_t ubRawKey) {
	if(pE->ubPos >= HI_NAME) return 1;
	if(ubRawKey == HI_KEY_NONE) return 0;
	pE->uwLeft = pE->uwFrames;                     // any key restarts the timeout
	tHiScore *s = &pT->pEntries[pE->ubRank];
	if(ubRawKey == HI_KEY_RETURN || ubRawKey == HI_KEY_NUMENTER || ubRawKey == HI_KEY_ESCAPE) {   // (Esc: ends it, the score stays)
		pE->ubPos = HI_NAME;
		return 1;
	}
	if(ubRawKey == HI_KEY_BACKSPACE) {
		if(pE->ubPos) s->name[--pE->ubPos] = ' ';
		return 0;
	}
	char c = hiKeyChar(ubRawKey);
	if(!c) return 0;
	s->name[pE->ubPos] = c;
	if(++pE->ubPos >= HI_NAME) return 1;
	return 0;
}

// --------------------------------------------------------------- display ---

HI_COLD void hiNumber(char *pOut, uint32_t ulValue, uint8_t ubDigits) {
	static const uint32_t pPow[9] = {1UL, 10UL, 100UL, 1000UL, 10000UL, 100000UL, 1000000UL, 10000000UL, 100000000UL};
	if(ubDigits > 9) ubDigits = 9;
	if(ubDigits < 9 && ulValue >= pPow[ubDigits]) ulValue = pPow[ubDigits] - 1;   // (the most that fits)
	for(uint8_t d = ubDigits; d--;) {
		uint8_t n = 0;
		while(ulValue >= pPow[d]) { ulValue -= pPow[d]; ++n; }   // (no divide)
		*pOut++ = (char)('0' + n);
	}
}

HI_COLD static char *hiPad(char *p, const char *sz, uint8_t ubWidth) {
	uint8_t n = 0;
	while(sz && sz[n] && n < ubWidth) *p++ = sz[n++];
	while(n++ < ubWidth) *p++ = ' ';
	return p;
}

HI_COLD void hiRow(char *szOut, const tHiTable *pT, uint8_t i, uint8_t isNew, const char *szReached, const char *szExtra) {
	const tHiScore *s = &pT->pEntries[i];
	char *p = szOut;
	*p++ = isNew ? '-' : ' ';
	*p++ = ' ';
	*p++ = (char)('1' + i);
	*p++ = ' ';
	*p++ = ' ';
	for(uint8_t c = 0; c < HI_NAME; ++c) *p++ = s->name[c];
	*p++ = ' ';
	*p++ = ' ';
	hiNumber(p, s->score, 6);
	p += 6;
	*p++ = ' ';
	*p++ = ' ';
	p = hiPad(p, szReached, HI_REACHED_W);
	*p++ = ' ';
	p = hiPad(p, szExtra, HI_EXTRA_W);
	*p++ = ' ';
	*p++ = isNew ? '-' : ' ';
	*p = 0;
}

static const char *const s_pDiskText[] = {
	"", "SAVING TO DISK...", "SAVED TO DISK", "DISK WRITE-PROTECTED: NOT SAVED", "DISK ERROR: NOT SAVED", "", "",
};

const char *hiDiskText(uint8_t ubStatus) {
	return ubStatus <= HI_DISK_DEFAULT ? s_pDiskText[ubStatus] : "";
}
