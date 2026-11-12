// ROTOR RAIDER - the attract demo's pilot (host tool, not part of the game).
// (c) 2026 Desert Palms Software. Game by Paul Ross Seibert.
//
// Flies Coral Reach with the real rules (src/logic.c) from the demo's fixed state - classic
// steering, difficulty 3, the dice at DEMO_SEED, the frame counter at DEMO_FRAME - to a factory:
// it steers so a bomb's landing mark comes onto it, brakes there, drops bombs, and when the
// factory falls it lets go of the stick and watches. It writes the flight as a scenario
// (press / wait lines, counted in frames) on stdout:
//
//   cc -std=c11 -I src src/logic.c src/enemies.c src/theatre.c src/hiscore.c src/missionmap.c \
//      src/attract.c src/demodata.c tools/demopilot.c -o /tmp/demopilot
//   /tmp/demopilot disk/theatre1.bin [factory index, 10] > tools/demoflight.agk.body
//
// then put the header comment of tools/demoflight.agk on it and convert it with the kit's
// techniques/attract/tools/scen2demo.py into src/demodata.c. tests/unit/test_logic.c replays
// the recording against the real theatre file and checks it still destroys the factory.
// Re-record after any change to the flight, the AA guns or the dice.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logic.h"

static tGame g;
static uint8_t s_pFile[40000];
static tTheatre th;

static void load(const char *path) {
	FILE *f = fopen(path, "rb");
	if(!f) { fprintf(stderr, "can't read %s\n", path); exit(1); }
	size_t n = fread(s_pFile, 1, sizeof(s_pFile), f);
	fclose(f);
	if(!theatreParse(s_pFile, (uint32_t)n, &th)) { fprintf(stderr, "%s isn't a theatre\n", path); exit(1); }
	logicInit(&g, th.map.pTiles, th.map.uwW, th.map.uwH);
	logicLoadTheatre(&g, &th);
}

int main(int argc, char **argv) {
	int iFact = argc > 2 ? atoi(argv[2]) : 10;
	int nMax = 1500;
	load(argc > 1 ? argv[1] : "disk/theatre1.bin");
	// the demo's fixed state (logicStartDemo and logicLoadTheatre set the same)
	g.steer = STEER_CLASSIC;
	g.difficulty = 3;
	g.state = GS_PLAY;
	g.lfsr = DEMO_SEED;
	g.frame = DEMO_FRAME;
	const tStruct *pF = &g.pStructs[iFact];
	int tx = pF->tx * 16 + 24, ty = pF->ty * 16 + 24;
	fprintf(stderr, "target factory %d at %d,%d from the gunship at %d,%d\n", iFact, tx, ty, g.chop.x, g.chop.y);
	static uint8_t pBits[4000];
	int nBombs = 0, nLast = -100, nDone = -1;
	for(int f = 0; f < nMax; ++f) {
		tInput in = {0};
		int16_t rx, ry;
		logicReticle(&g, &rx, &ry);
		int ox = rx - tx, oy = ry - ty;              // where a bomb would land, from the target
		int rd = logicDist((int16_t)ox, (int16_t)oy);
		uint8_t ubWant = logicHeadingTo((int16_t)-ox, (int16_t)-oy);   // the way to move so it does
		int8_t turn = logicTurnToward(g.chop.heading, ubWant);
		int diff = ((ubWant - g.chop.heading + 8) & 15) - 8;
		int isAligned = diff >= -1 && diff <= 1;
		int speed = abs(g.chop.vx) > abs(g.chop.vy) ? abs(g.chop.vx) : abs(g.chop.vy);
		in.dx = (diff >= 2 || diff <= -2 || (rd < 110 && rd >= 40 && diff != 0)) ? turn : 0;   // (a dead zone: no wobble in the cruise)
		if(rd > 110) {
			if(rd > 200 || speed < 14) { if(isAligned || rd > 400) in.dy = -1; }
			else if(speed > 22) in.dy = 1;      // brake
		}
		else if(rd > 24) { if(speed > 9) in.dy = 1; else if(speed < 6 && isAligned) in.dy = -1; }
		else if(speed > 1) in.dy = 1;
		// bombs: when a bomb's mark is within the factory's footprint (and its blast), 30 frames apart
		if(g.chop.alt >= BOMB_MIN_ALT && !g.chop.landed && g.chop.bombs && f - nLast > 30 && abs(ox) < 26 && abs(oy) < 26 && nBombs < 4) {
			in.fire2 = 1;
			nLast = f;
			++nBombs;
		}
		if(nDone >= 0) { in.dx = 0; in.dy = 0; in.fire2 = 0; }   // the factory is down: it hangs there and watches
		pBits[f] = (uint8_t)((in.dx < 0 ? 1 : 0) | (in.dx > 0 ? 2 : 0) | (in.dy < 0 ? 4 : 0) | (in.dy > 0 ? 8 : 0) | (in.fire ? 16 : 0) | (in.fire2 ? 32 : 0));
		logicUpdate(&g, &in);
		if(g.state != GS_PLAY) { fprintf(stderr, "the gunship was lost at frame %d\n", f); return 1; }
		if(g.factoriesLeft < 6 && nDone < 0) nDone = f;
		if(nDone >= 0 && f > nDone + 100) { nMax = f + 1; break; }
	}
	if(nDone < 0) { fprintf(stderr, "no factory fell in %d frames\n", nMax); return 1; }
	for(int i = 0; i < nMax;) {
		int j = i;
		while(j < nMax && pBits[j] == pBits[i]) ++j;
		uint8_t b = pBits[i];
		if(!b) printf("wait %d\n", j - i);
		else {
			char sz[64] = "";
			const char *pN[6] = {"left", "right", "up", "down", "fire", "fire2"};
			for(int k = 0; k < 6; ++k) if(b & (1 << k)) { if(*sz) strcat(sz, "+"); strcat(sz, pN[k]); }
			printf("press %s %d\n", sz, j - i);
		}
		i = j;
	}
	fprintf(stderr, "%d frames; %d bombs dropped, the factory fell at frame %d, score %u, damage %d\n", nMax, nBombs, nDone, (unsigned)g.score, g.chop.damage);
	return 0;
}
