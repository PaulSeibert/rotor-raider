// ROTOR RAIDER - the Desert Palms Software logo at boot.
// (c) 2026 Desert Palms Software. Game by Paul Ross Seibert. (From Rotor Rescue.)
//
// A 288x242 picture in 4 bitplanes (16 colours), loaded from the floppy
// (disk/dpslogo.bin, made by art/tools/make_dps_logo.py) and freed again once
// it's been shown. Colours 12-15 change on every line where the picture
// needs it (the copper reloads them), so the shield's sunset stripes and the
// purple horizon keep their gradients: about 60 colours on screen.
//
// Its own two copper lists, outside ACE's: while the logo shows, COP1LC
// points at one of them. It fades in and out by rewriting every colour
// MOVE in the list that's about to show.

#include "splash.h"
#include <ace/managers/copper.h>
#include <ace/managers/memory.h>
#include <ace/utils/custom.h>
#include <agk/copw.h>
#include <agk/debug.h>
#include <agk/pack.h>
#include "dps_logo.h"

#define SPLASH_Y0 ((256 - DPS_LOGO_H) / 2)
#define LOGO_ROW_BYTES (DPS_LOGO_W / 8)
#define LOGO_WORDS (DPS_LOGO_W / 16)
// centred: (320 - W) / 2 px in, 8 fetch units per 16 px from the usual 0x38 / 0xD0
#define DDF_START (0x38 + ((320 - DPS_LOGO_W) / 2) / 2)
#define DDF_STOP (DDF_START + (LOGO_WORDS - 1) * 8)
#define COP_COUNT (64 + 16 + DPS_LOGO_H * (1 + DPS_LOGO_ROW_REGS) + 8)
#define COLOR_MAX (16 + DPS_LOGO_H * DPS_LOGO_ROW_REGS)

#define FADE_STEPS 16
#define FRAMES_PER_STEP 3       // a step's work over 3 frames: the colours, then each list
#define HOLD_FRAMES 130

static UBYTE *s_pData;                // chip RAM: the picture (and its colour tables)
static tCopCmd *s_pList[2];           // chip RAM
// Per colour MOVE (the same in both lists): the byte offset of its value in
// the list, and of its colour in s_pFaded. A fade step fades the picture's
// unique colours once, then copies them into the MOVEs.
static UWORD s_pMoveOffs[COLOR_MAX];
static UWORD s_pColorOffs[COLOR_MAX];
static UWORD s_uwColors;
static UWORD s_pFaded[DPS_LOGO_COLORS];
static UBYTE s_pFade[FADE_STEPS + 1][16];
static UBYTE s_ubShown;               // the list COP1LC points at
static UWORD s_uwFrame, s_uwHoldEnd;
static UBYTE s_ubLevel, s_ubListLevel[2], s_ubPhase;
static UBYTE s_isFadingOut, s_isOn;

static void listWrite(tCopCmd *pList, UBYTE ubViewPosY, const void *pBlankChip) {
	tAgkCopWriter sW;
	agkCopInit(&sW, pList, 0, ubViewPosY);
	ULONG ulBlank = (ULONG)pBlankChip;
	for(UBYTE s = 0; s < 8; ++s) {
		// no sprites: every channel at an empty sprite
		agkCopMove(&sW, (UWORD *)&g_pCustom->sprpt[s], (UWORD)(ulBlank >> 16));
		agkCopMove(&sW, (UWORD *)&g_pCustom->sprpt[s] + 1, (UWORD)(ulBlank & 0xFFFF));
	}
	agkCopMove(&sW, &g_pCustom->bplcon0, 0x0200);
	agkCopMove(&sW, &g_pCustom->bplcon1, 0);
	agkCopMove(&sW, &g_pCustom->bplcon2, 0);
	agkCopMove(&sW, &g_pCustom->ddfstrt, DDF_START);
	agkCopMove(&sW, &g_pCustom->ddfstop, DDF_STOP);
	agkCopMove(&sW, &g_pCustom->bpl1mod, LOGO_ROW_BYTES * (DPS_LOGO_PLANES - 1));
	agkCopMove(&sW, &g_pCustom->bpl2mod, LOGO_ROW_BYTES * (DPS_LOGO_PLANES - 1));
	const UBYTE *pPlanes = s_pData + DPS_LOGO_PLANES_OFFS;
	for(UBYTE p = 0; p < DPS_LOGO_PLANES; ++p) agkCopPtrs(&sW, p, (ULONG)(pPlanes + p * LOGO_ROW_BYTES));

	// the first line's 16 colours, then what changes line by line
	const UWORD *pFirst = (const UWORD *)(s_pData + DPS_LOGO_FIRST_OFFS);
	s_uwColors = 0;
	for(UBYTE i = 0; i < 16; ++i) {
		s_pMoveOffs[s_uwColors] = (UWORD)(sW.uwPos * sizeof(tCopCmd) + 2);
		s_pColorOffs[s_uwColors++] = (UWORD)(pFirst[i] * 2);
		agkCopMove(&sW, &g_pCustom->color[i], 0);
	}
	// Changes go at the start of their line (the picture starts 16 px in: room
	// for a dozen MOVEs). Not at the end of the line before: on line 255 the
	// MOVEs would run into line 256 before the copper reached the wait for the
	// end of line 255, and it would wait for a line 255 that never comes.
	agkCopWait(&sW, SPLASH_Y0, 0);
	agkCopMove(&sW, &g_pCustom->bplcon0, (DPS_LOGO_PLANES << 12) | 0x0200);
	const UWORD *pRows = (const UWORD *)(s_pData + DPS_LOGO_ROWS_OFFS);
	for(UWORD y = 1; y < DPS_LOGO_H; ++y) {
		const UWORD *pRow = pRows + y * DPS_LOGO_ROW_REGS, *pPrev = pRow - DPS_LOGO_ROW_REGS;
		UBYTE isWaited = 0;
		for(UBYTE r = 0; r < DPS_LOGO_ROW_REGS; ++r) {
			if(pRow[r] == pPrev[r]) continue;
			if(!isWaited) {
				agkCopWait(&sW, (UWORD)(SPLASH_Y0 + y), 0);
				isWaited = 1;
			}
			s_pMoveOffs[s_uwColors] = (UWORD)(sW.uwPos * sizeof(tCopCmd) + 2);
			s_pColorOffs[s_uwColors++] = (UWORD)(pRow[r] * 2);
			agkCopMove(&sW, &g_pCustom->color[DPS_LOGO_FIRST_ROW_REG + r], 0);
		}
	}
	agkCopWait(&sW, SPLASH_Y0 + DPS_LOGO_H, 0);
	agkCopMove(&sW, &g_pCustom->bplcon0, 0x0200);
	agkCopFill(&sW, COP_COUNT);
}

// The unique colours at brightness ubLevel (0..FADE_STEPS)
static UBYTE s_ubFadedLevel = 0xFF;
static void colorsFade(UBYTE ubLevel) {
	if(ubLevel == s_ubFadedLevel) return;
	s_ubFadedLevel = ubLevel;
	const UBYTE *pMul = s_pFade[ubLevel];
	const UWORD *pSrc = (const UWORD *)s_pData;
	UWORD *pDst = s_pFaded;
	for(UWORD i = DPS_LOGO_COLORS; i--;) {
		UWORD c = *pSrc++;
		*pDst++ = (UWORD)((pMul[c >> 8] << 8) | (pMul[(c >> 4) & 0xF] << 4) | pMul[c & 0xF]);
	}
}

// Every colour MOVE of list b at brightness ubLevel
static void listFade(UBYTE b, UBYTE ubLevel) {
	colorsFade(ubLevel);
	UBYTE *pList = (UBYTE *)s_pList[b];
	const UBYTE *pFaded = (const UBYTE *)s_pFaded;
	const UWORD *pMove = s_pMoveOffs, *pColor = s_pColorOffs;
	for(UWORD i = s_uwColors; i--;) {
		*(UWORD *)(pList + *pMove++) = *(const UWORD *)(pFaded + *pColor++);
	}
	s_ubListLevel[b] = ubLevel;
}

UBYTE splashCreate(UBYTE ubViewPosY, const void *pBlankChip) {
	s_isOn = 0;
	s_pData = memAllocChip(DPS_LOGO_BYTES);
	if(!s_pData) return 0;
	if(agkLoadFile("dpslogo.bin.pk", s_pData, DPS_LOGO_BYTES) != DPS_LOGO_BYTES) {
		memFree(s_pData, DPS_LOGO_BYTES);
		s_pData = 0;
		return 0;
	}
	for(UBYTE l = 0; l <= FADE_STEPS; ++l)
		for(UBYTE c = 0; c < 16; ++c) s_pFade[l][c] = (UBYTE)((c * l + FADE_STEPS / 2) / FADE_STEPS);
	for(UBYTE b = 0; b < 2; ++b) {
		s_pList[b] = memAllocChip(COP_COUNT * sizeof(tCopCmd));
		listWrite(s_pList[b], ubViewPosY, pBlankChip);
		listFade(b, 0);
	}
	s_ubShown = 0;
	s_uwFrame = 0;
	s_ubLevel = 0;
	s_ubPhase = 0;
	s_isFadingOut = 0;
	s_uwHoldEnd = FADE_STEPS * FRAMES_PER_STEP + HOLD_FRAMES;
	s_isOn = 1;
	agkState("splash_colors", s_uwColors);
	agkEnd();
	return 1;
}

void splashShow(void) {
	if(!s_isOn) return;
	g_pCustom->cop1lc = (ULONG)s_pList[s_ubShown];
	g_pCustom->copjmp1 = 1;
}

// A fade step takes FRAMES_PER_STEP frames: the new brightness of the unique
// colours, then one list (it shows from the next picture), then the other:
// no frame does more than one of them.
UBYTE splashProcess(UBYTE isSkip) {
	if(!s_isOn) return 0;
	++s_uwFrame;
	if(!s_isFadingOut && (isSkip || s_uwFrame >= s_uwHoldEnd)) {
		s_isFadingOut = 1;
		agkPrint("AGK splash out\n");
	}
	UBYTE ubPhase = s_ubPhase;
	s_ubPhase = (UBYTE)(ubPhase + 1 == FRAMES_PER_STEP ? 0 : ubPhase + 1);
	if(!ubPhase) {
		if(!s_isFadingOut) {
			if(s_ubLevel < FADE_STEPS) ++s_ubLevel;
		}
		else if(s_ubLevel) --s_ubLevel;
		else if(!s_ubListLevel[0] && !s_ubListLevel[1]) {
			// black, both lists: the game's copper list can take over
			s_isOn = 0;
			agkPrint("AGK splash done\n");
			return 0;
		}
		colorsFade(s_ubLevel);
		return 1;
	}
	UBYTE b = (UBYTE)(s_ubShown ^ 1);
	if(s_ubListLevel[b] != s_ubLevel) {
		listFade(b, s_ubLevel);
		s_ubShown = b;
		g_pCustom->cop1lc = (ULONG)s_pList[b];   // from the next picture
	}
	return 1;
}

void splashDestroy(void) {
	s_isOn = 0;
	for(UBYTE b = 0; b < 2; ++b) {
		if(s_pList[b]) memFree(s_pList[b], COP_COUNT * sizeof(tCopCmd));
		s_pList[b] = 0;
	}
	if(s_pData) memFree(s_pData, DPS_LOGO_BYTES);
	s_pData = 0;
}
