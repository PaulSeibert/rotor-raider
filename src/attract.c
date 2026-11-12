// attract: see attract.h and techniques/attract/TECHNIQUE.md.
// Plain C, no libgcc maths.

#include "attract.h"

#if defined(__GNUC__) && !defined(__clang__)
#define AT_COLD __attribute__((noinline, optimize("Os")))
#else
#define AT_COLD __attribute__((noinline))
#endif

static void toPage(tAttract *pA, uint8_t ubPage) {
	pA->ubPage = ubPage;
	pA->uwTimer = 0;
	pA->isArmed = 0;
}

AT_COLD void attractInit(tAttract *pA, const tAttractCfg *pCfg) {
	pA->pCfg = pCfg;
	toPage(pA, ATTRACT_TITLE);
}

AT_COLD void attractReset(tAttract *pA) {
	toPage(pA, ATTRACT_TITLE);
}

AT_COLD uint8_t attractUpdate(tAttract *pA, uint8_t isFire, uint8_t isTouched) {
	if(!isFire) pA->isArmed = 1;
	uint8_t isPress = isFire && pA->isArmed;
	if(pA->ubPage == ATTRACT_TITLE) {
		if(isPress) {
			pA->isArmed = 0;
			return ATTRACT_START;
		}
		if(isTouched || isFire) pA->uwTimer = 0;       // somebody is there
		else if(++pA->uwTimer >= pA->pCfg->uwIdle) {
			toPage(pA, ATTRACT_SCORES);
			return ATTRACT_PAGE;
		}
		return ATTRACT_NOTHING;
	}
	// any other page: fire ends it
	if(isPress) {
		toPage(pA, ATTRACT_TITLE);
		return ATTRACT_PAGE;
	}
	if(pA->ubPage == ATTRACT_SCORES) {
		if(++pA->uwTimer >= pA->pCfg->uwScores) {
			toPage(pA, ATTRACT_DEMO);
			return ATTRACT_PAGE;
		}
	}
	else if(pA->ubPage == ATTRACT_CREDITS) {
		if(++pA->uwTimer >= pA->pCfg->uwCredits) {
			toPage(pA, ATTRACT_TITLE);
			return ATTRACT_PAGE;
		}
	}
	return ATTRACT_NOTHING;                            // (the demo ends by attractDemoDone)
}

AT_COLD void attractDemoDone(tAttract *pA) {
	toPage(pA, pA->pCfg->uwCredits ? ATTRACT_CREDITS : ATTRACT_TITLE);
}

// ------------------------------------------------------------------ demo ---

AT_COLD void demoStart(tDemo *pD, const uint8_t (*pRuns)[2], uint16_t uwRuns) {
	pD->pRuns = pRuns;
	pD->uwRuns = uwRuns;
	pD->uwRun = 0;
	pD->ubLeft = uwRuns ? pRuns[0][0] : 0;
}

AT_COLD uint8_t demoNext(tDemo *pD, uint8_t *pBits) {
	// (runs of 0 frames are skipped)
	while(pD->uwRun < pD->uwRuns && !pD->ubLeft) {
		++pD->uwRun;
		pD->ubLeft = pD->uwRun < pD->uwRuns ? pD->pRuns[pD->uwRun][0] : 0;
	}
	if(pD->uwRun >= pD->uwRuns) return 0;
	*pBits = pD->pRuns[pD->uwRun][1];
	--pD->ubLeft;
	return 1;
}

AT_COLD uint32_t demoFrames(const uint8_t (*pRuns)[2], uint16_t uwRuns) {
	uint32_t ulN = 0;
	for(uint16_t i = 0; i < uwRuns; ++i) ulN += pRuns[i][0];
	return ulN;
}
