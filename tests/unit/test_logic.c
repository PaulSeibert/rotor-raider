// Host-side tests for src/logic.c (ROTOR RAIDER). Run with: agk unit
#include <stdio.h>
#include <string.h>
#include "logic.h"
#include "cardmap.h"

static int s_failures;

#define CHECK(cond) do { \
	if(!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++s_failures; } \
} while(0)

static tGame g;
static uint8_t s_pMap[MAP_W_MAX * MAP_H_MAX];
static uint8_t s_pFile[THEATRE_HEADER + MAP_W_MAX * MAP_H_MAX + 6 * STRUCT_MAX + 18 * BANDS_MAX];
static tTheatre s_sTh;
static const tInput NONE = {0};

static void step(tInput in, int frames) {
	for(int i = 0; i < frames; ++i) logicUpdate(&g, &in);
}

// An all-sea theatre, the chopper flying at (x, y)
static void newGame(uint8_t ubSteer, int16_t x, int16_t y) {
	for(int i = 0; i < MAP_W_MAX * MAP_H_MAX; ++i) s_pMap[i] = 0;
	logicInit(&g, s_pMap, MAP_W_MAX, MAP_H_MAX);
	g.state = GS_PLAY;
	g.steer = ubSteer;
	g.chop.x = x;
	g.chop.y = y;
	g.chop.alt = ALT_CRUISE;
}

static void testHeadingTable(void) {
	CHECK(g_pHeadX[0] == 0 && g_pHeadY[0] == -64);    // north
	CHECK(g_pHeadX[4] == 64 && g_pHeadY[4] == 0);     // east
	CHECK(g_pHeadX[8] == 0 && g_pHeadY[8] == 64);     // south
	CHECK(g_pHeadX[12] == -64 && g_pHeadY[12] == 0);  // west
	CHECK(g_pHeadX[2] > 40 && g_pHeadY[2] < -40);     // north-east
}

static void testStickToHeading(void) {
	CHECK(logicHeadingFor(0, -1) == 0);
	CHECK(logicHeadingFor(1, -1) == 2);
	CHECK(logicHeadingFor(1, 0) == 4);
	CHECK(logicHeadingFor(1, 1) == 6);
	CHECK(logicHeadingFor(0, 1) == 8);
	CHECK(logicHeadingFor(-1, 1) == 10);
	CHECK(logicHeadingFor(-1, 0) == 12);
	CHECK(logicHeadingFor(-1, -1) == 14);
}

static void testTurnShorterWay(void) {
	CHECK(logicTurnToward(0, 4) == 1);
	CHECK(logicTurnToward(0, 12) == -1);
	CHECK(logicTurnToward(14, 2) == 1);    // through north
	CHECK(logicTurnToward(2, 14) == -1);
	CHECK(logicTurnToward(5, 5) == 0);
}

static void testClassicRotatesAndThrusts(void) {
	newGame(STEER_CLASSIC, 1000, 1000);
	g.chop.heading = 0;
	step((tInput){.dx = 1}, TURN_FRAMES * 4);
	CHECK(g.chop.heading == 4);                       // a quarter turn: east
	CHECK(g.chop.vx == 0 && g.chop.vy == 0);          // turning alone doesn't move
	step((tInput){.dy = -1}, 60);
	CHECK(g.chop.vx >= V_MAX - 2 && g.chop.vy == 0);  // full speed east
	CHECK(g.chop.x > 1060);
	int16_t x = g.chop.x;
	uint8_t ubStop = 0;
	for(uint8_t k = 0; k < 40 && g.chop.vx > 0; ++k, ++ubStop) step((tInput){.dy = 1}, 1);   // brake
	CHECK(g.chop.vx == 0 && ubStop < 30);             // stopped, soon
	CHECK(g.chop.x > x);                              // it coasted a bit first
}

// Down held on after stopping: it backs off, tail first, slowly; let go and
// it comes to a hover (Raid on Bungeling Bay's stick)
static void testClassicReverses(void) {
	newGame(STEER_CLASSIC, 1000, 1000);
	g.chop.heading = 4;                               // nose east
	step((tInput){.dy = -1}, 60);
	step((tInput){.dy = 1}, 120);                     // brake, then keep holding
	CHECK(g.chop.vx < 0 && g.chop.vy == 0);           // going west, tail first
	CHECK(g.chop.vx >= -V_REVERSE && g.chop.vx <= -V_REVERSE + 2);   // at reverse speed, no more
	CHECK(g.chop.heading == 4);                       // still facing east
	int16_t x = g.chop.x;
	step((tInput){.dy = 1}, 32);
	CHECK(g.chop.x < x);
	step(NONE, 300);
	CHECK(g.chop.vx == 0 && g.chop.vy == 0);          // let go: it settles to a hover
}

static void testSameSpeedEveryWay(void) {
	for(uint8_t h = 0; h < 16; ++h) {
		newGame(STEER_CLASSIC, 1000, 1000);
		g.chop.heading = h;
		step((tInput){.dy = -1}, 40);
		uint8_t m = (uint8_t)((h + 8) & 15);       // the opposite heading
		int16_t vx = g.chop.vx, vy = g.chop.vy;
		newGame(STEER_CLASSIC, 1000, 1000);
		g.chop.heading = m;
		step((tInput){.dy = -1}, 40);
		CHECK(g.chop.vx == -vx && g.chop.vy == -vy);
	}
}

static void testClassicGlidesThenRests(void) {
	newGame(STEER_CLASSIC, 1000, 1000);
	g.chop.heading = 8;
	step((tInput){.dy = -1}, 40);
	CHECK(g.chop.vy == V_MAX);
	step(NONE, 40);
	CHECK(g.chop.vy > 10);                    // still gliding south
	int16_t y = g.chop.y;
	step(NONE, 200);
	CHECK(g.chop.vy == 0 && g.chop.vx == 0);  // at rest
	CHECK(g.chop.y > y + 40);                 // it glid a good way
}

static void testClassicTurnWhileGliding(void) {
	// flat out east, then off the stick and turn north: still going east
	newGame(STEER_CLASSIC, 1000, 1000);
	g.chop.heading = 4;
	step((tInput){.dy = -1}, 40);
	step((tInput){.dx = -1}, TURN_FRAMES * 4);
	CHECK(g.chop.heading == 0);
	CHECK(g.chop.vx > 25 && g.chop.vy == 0);  // the nose turned, the motion didn't
	// thrust now pushes north on top of the slide east
	step((tInput){.dy = -1}, 10);
	CHECK(g.chop.vx > 10 && g.chop.vy < -10);
}

static void testClassicFullSpeedIsExact(void) {
	newGame(STEER_CLASSIC, 1000, 1000);
	g.chop.heading = 4;
	step((tInput){.dy = -1}, 80);
	CHECK(g.chop.vx == V_MAX && g.chop.vy == 0);
	newGame(STEER_CLASSIC, 1000, 1000);
	g.chop.heading = 6;                       // diagonal: the same speed along it
	step((tInput){.dy = -1}, 120);
	CHECK(g.chop.vx == 28 && g.chop.vy == 28);
	// sliding north, then thrusting south-west: it ends up flying south-west
	newGame(STEER_CLASSIC, 1000, 1000);
	g.chop.vy = -V_MAX;
	g.chop.heading = 10;
	step((tInput){.dy = -1}, 200);
	CHECK(g.chop.vx == -28 && g.chop.vy == 28);
}

static void testArcadeTurnsTowardStickAndFlies(void) {
	newGame(STEER_ARCADE, 1000, 1000);
	g.chop.heading = 0;
	step((tInput){.dx = 1, .dy = 1}, 1);              // south-east: heading 6, via east
	CHECK(g.chop.heading == 0 || g.chop.heading == 1);
	step((tInput){.dx = 1, .dy = 1}, TURN_FRAMES * 6 + 60);
	CHECK(g.chop.heading == 6);
	CHECK(g.chop.vx > 20 && g.chop.vy > 20);          // flying south-east
	step(NONE, 80);
	CHECK(g.chop.vx == 0 && g.chop.vy == 0);          // stick released: it slows to a hover
}

static void testWorldEdgesHold(void) {
	newGame(STEER_ARCADE, 40, 40);
	step((tInput){.dx = -1, .dy = -1}, 200);
	CHECK(g.chop.x >= 16 && g.chop.y >= 16);
	newGame(STEER_ARCADE, MAP_W_MAX * TILE - 40, MAP_H_MAX * TILE - 40);
	step((tInput){.dx = 1, .dy = 1}, 200);
	CHECK(g.chop.x <= MAP_W_MAX * TILE - 16 && g.chop.y <= MAP_H_MAX * TILE - 16);
}

static void testCameraFollowsWithinMapAndLimitsStep(void) {
	newGame(STEER_ARCADE, 1000, 1000);
	g.camX = 0; g.camY = 0;
	int16_t px = g.camX, py = g.camY;
	for(int i = 0; i < 200; ++i) {
		logicCameraUpdate(&g);
		int16_t dx = g.camX - px, dy = g.camY - py;
		CHECK(dx <= CAM_MAX_STEP && dx >= -CAM_MAX_STEP && dy <= CAM_MAX_STEP && dy >= -CAM_MAX_STEP);
		px = g.camX; py = g.camY;
	}
	CHECK(g.camX == 1000 - SCREEN_W / 2 && g.camY == 1000 - PLAY_H / 2);   // settled on the chopper
	newGame(STEER_ARCADE, 20, 20);
	for(int i = 0; i < 300; ++i) logicCameraUpdate(&g);
	CHECK(g.camX == 0 && g.camY == 0);                // clamped at the map's corner
	newGame(STEER_ARCADE, MAP_W_MAX * TILE - 20, MAP_H_MAX * TILE - 20);
	for(int i = 0; i < 400; ++i) logicCameraUpdate(&g);
	CHECK(g.camX == MAP_W_MAX * TILE - SCREEN_W && g.camY == MAP_H_MAX * TILE - PLAY_H);
}

static void testTileAt(void) {
	newGame(STEER_ARCADE, 100, 100);
	s_pMap[3 * MAP_W_MAX + 5] = 15;
	CHECK(logicTileAt(&g.map, 5 * 16 + 3, 3 * 16 + 15) == 15);
	CHECK(logicTileAt(&g.map, 5 * 16 + 16, 3 * 16) == 0);
	CHECK(logicTileAt(&g.map, -5, 10) == 0);          // outside: sea
}

static void testScrollMaths(void) {
	CHECK(logicScrollDelay(0) == 0 && logicScrollByteOffset(0) == -2);
	CHECK(logicScrollDelay(1) == 15 && logicScrollByteOffset(1) == 0);
	CHECK(logicScrollDelay(16) == 0 && logicScrollByteOffset(16) == 0);
	CHECK(logicScrollDelay(17) == 15 && logicScrollByteOffset(17) == 2);
}

static void testDeterministic(void) {
	newGame(STEER_CLASSIC, 500, 500);
	uint16_t a = logicRandom(&g), b = logicRandom(&g);
	newGame(STEER_CLASSIC, 500, 500);
	CHECK(logicRandom(&g) == a && logicRandom(&g) == b);
	CHECK(a != b);
}

// The carrier sailing east from (1000, 600) in a lane 1000 px wide
static void withCarrier(void) {
	logicCarrierPlace(&g, 1000, 600, 600, 1600);
}

static void testCarrierPatrolsItsLane(void) {
	newGame(STEER_CLASSIC, 500, 500);
	withCarrier();
	CHECK(g.carrier.dir == 1);
	step(NONE, 2 * 50 * 60);          // two minutes
	CHECK(g.carrier.x >= 600 && g.carrier.x <= 1600);
	CHECK(g.carrier.y == 600);
	int16_t x = g.carrier.x;
	step(NONE, 100);
	CHECK(g.carrier.x != x);           // still moving
}

static void testCarrierTurnsAtLaneEnd(void) {
	newGame(STEER_CLASSIC, 500, 500);
	logicCarrierPlace(&g, 1590, 600, 600, 1600);
	step(NONE, 200);
	CHECK(g.carrier.dir == -1 && g.carrier.x < 1600);
}

static void testLandsWhenSlowOverDeck(void) {
	newGame(STEER_CLASSIC, 1000, 600);
	withCarrier();
	g.chop.x = g.carrier.x;
	g.chop.y = g.carrier.y;
	step(NONE, LAND_DELAY + ALT_CRUISE + 5);
	CHECK(g.chop.landed && g.chop.alt == 0);
	CHECK(g.events & EV_LANDED || g.chop.landed);
	// it rides with the ship
	int16_t dx = (int16_t)(g.chop.x - g.carrier.x);
	step(NONE, 200);
	CHECK(g.chop.x - g.carrier.x == dx);
}

static void testNoLandingAtSea(void) {
	newGame(STEER_CLASSIC, 300, 300);
	withCarrier();
	step(NONE, 300);
	CHECK(!g.chop.landed && g.chop.alt == ALT_CRUISE);
}

static void testNoLandingWhileMoving(void) {
	newGame(STEER_CLASSIC, 1000, 600);
	withCarrier();
	g.chop.x = g.carrier.x - 40;
	g.chop.y = g.carrier.y;
	g.chop.heading = 4;
	step((tInput){.dy = -1}, 30);     // flying over the deck at speed
	CHECK(!g.chop.landed);
}

static void testDeckRepairsAndReloads(void) {
	newGame(STEER_CLASSIC, 1000, 600);
	withCarrier();
	g.chop.x = g.carrier.x;
	g.chop.y = g.carrier.y;
	g.chop.damage = 60;
	g.chop.bombs = 0;
	step(NONE, LAND_DELAY + ALT_CRUISE + 5);
	CHECK(g.chop.landed);
	step(NONE, 50 * 20);
	CHECK(g.chop.damage == 0 && g.chop.bombs == BOMBS_MAX);
}

static void testTakeOffClimbs(void) {
	newGame(STEER_CLASSIC, 1000, 600);
	withCarrier();
	g.chop.x = g.carrier.x;
	g.chop.y = g.carrier.y;
	step(NONE, LAND_DELAY + ALT_CRUISE + 5);
	CHECK(g.chop.landed);
	step((tInput){.dy = -1}, 2);
	CHECK(!g.chop.landed && g.chop.alt > 0);
	step((tInput){.dy = -1}, ALT_CRUISE);
	CHECK(g.chop.alt == ALT_CRUISE);
}

static void testArcadeLandsToo(void) {
	newGame(STEER_ARCADE, 1000, 600);
	withCarrier();
	g.chop.x = g.carrier.x;
	g.chop.y = g.carrier.y;
	step(NONE, LAND_DELAY + ALT_CRUISE + 5);
	CHECK(g.chop.landed);
	step((tInput){.dx = 1}, 2);       // any push takes off
	CHECK(!g.chop.landed);
}

// A theatre file: an all-sea map w x h, the given objects, one band
static uint32_t makeTheatre(uint16_t w, uint16_t h, const tTheatreObj *pObjs, uint16_t n) {
	uint8_t *p = s_pFile;
	*p++ = 'R'; *p++ = 'R'; *p++ = 'T'; *p++ = 'H';
	*p++ = (uint8_t)(w >> 8); *p++ = (uint8_t)w;
	*p++ = (uint8_t)(h >> 8); *p++ = (uint8_t)h;
	*p++ = (uint8_t)(n >> 8); *p++ = (uint8_t)n;
	*p++ = 0; *p++ = 1;
	for(uint32_t i = 0; i < (uint32_t)w * h; ++i) *p++ = 0;
	for(uint16_t i = 0; i < n; ++i) {
		*p++ = pObjs[i].type; *p++ = pObjs[i].tx; *p++ = pObjs[i].ty;
		*p++ = pObjs[i].a; *p++ = pObjs[i].b; *p++ = pObjs[i].c;
	}
	*p++ = 0; *p++ = 0;
	for(int i = 0; i < 8; ++i) { *p++ = 0x0F; *p++ = (uint8_t)(0x10 + i); }
	return (uint32_t)(p - s_pFile);
}

// A game in a theatre with a carrier, a factory and a turret; the gunship
// flying (not on deck) at (x, y), facing north
static void newTheatre(int16_t x, int16_t y) {
	const tTheatreObj pObjs[] = {
		{OBJ_CARRIER, 20, 60, 10, 40, 0},
		{OBJ_FACTORY, 30, 30, 0, 0, 0},       // px 480..527 x 480..527
		{OBJ_TURRET, 31, 28, 0, 0, 0},        // px 496..511 x 448..463: where the gun's rounds land
	};
	uint32_t ulSize = makeTheatre(80, 80, pObjs, 3);
	CHECK(theatreParse(s_pFile, ulSize, &s_sTh));
	logicInit(&g, s_sTh.map.pTiles, s_sTh.map.uwW, s_sTh.map.uwH);
	logicLoadTheatre(&g, &s_sTh);
	g.state = GS_PLAY;
	g.steer = STEER_CLASSIC;
	g.chop.landed = 0;
	g.chop.alt = ALT_CRUISE;
	g.chop.x = x;
	g.chop.y = y;
	g.chop.heading = 0;
	g.ubDirty = 0;
}

static void testTheatreParse(void) {
	const tTheatreObj pObjs[] = {{OBJ_FACTORY, 2, 2, 0, 0, 0}, {OBJ_TURRET, 8, 3, 0, 0, 0}};
	uint32_t ulSize = makeTheatre(20, 10, pObjs, 2);
	CHECK(theatreParse(s_pFile, ulSize, &s_sTh));
	CHECK(s_sTh.map.uwW == 20 && s_sTh.map.uwH == 10);
	CHECK(s_sTh.map.pTiles == s_pFile + THEATRE_HEADER);
	CHECK(s_sTh.uwObjs == 2 && s_sTh.pObjs[1].type == OBJ_TURRET && s_sTh.pObjs[1].tx == 8);
	CHECK(s_sTh.ubBands == 1 && s_sTh.pBands[0].ubRow == 0);
	CHECK(s_sTh.pBands[0].pColors[0] == 0x0F10 && s_sTh.pBands[0].pColors[7] == 0x0F17);
	CHECK(!theatreParse(s_pFile, ulSize - 1, &s_sTh));     // short
	s_pFile[0] = 'X';
	CHECK(!theatreParse(s_pFile, ulSize, &s_sTh));         // not a theatre
}

static void testLoadTheatre(void) {
	newTheatre(100, 100);
	CHECK(g.ubStructs == 2 && g.factoriesLeft == 1);
	CHECK(g.pStructs[0].type == OBJ_FACTORY && g.pStructs[0].hp == FACTORY_HP);
	CHECK(g.pStructs[1].type == OBJ_TURRET && g.pStructs[1].hp == TURRET_HP);
	CHECK(g.ubFactoryStructs == 1 && g.pFactoryStructs[0] == 0);   // (the repairs' list)
	CHECK(g.carrier.x == 20 * TILE + 8 && g.carrier.xMin == 10 * TILE && g.carrier.xMax == 40 * TILE);
	CHECK(logicTileAt(&g.map, 30 * TILE, 30 * TILE) == TILE_FACTORY);       // drawn into the map
	CHECK(logicTileAt(&g.map, 32 * TILE, 32 * TILE) == TILE_FACTORY + 8);
	CHECK(logicTileAt(&g.map, 31 * TILE, 28 * TILE) == TILE_TURRET);
}

static void testStartsOnDeck(void) {
	newTheatre(100, 100);
	logicLoadTheatre(&g, &s_sTh);
	CHECK(g.chop.landed && g.chop.alt == 0 && logicOverDeck(&g));
	CHECK(g.chop.bombs == BOMBS_MAX && g.chop.damage == 0);
}

static void testCannonFiresAndFalls(void) {
	newTheatre(500, 700);
	step((tInput){.fire = 1}, 1);
	CHECK(g.events & EV_CANNON);
	CHECK(g.pShots[0].active && g.pShots[0].vy < 0 && g.pShots[0].alt == ALT_CRUISE - CANNON_DROP);   // (it moved this frame)
	step((tInput){.fire = 1}, 5);
	CHECK(!g.pShots[1].active);              // one shot a press (one button)
	step(NONE, 20);
	CHECK(!g.pShots[0].active);              // it hit the ground ahead
}

static void testCannonKillsTurret(void) {
	newTheatre(504, 500);
	uint32_t ulScore = g.score;
	for(int i = 0; i < TURRET_HP; ++i) {
		step((tInput){.fire = 1}, 1);
		step(NONE, 20);
	}
	CHECK(g.pStructs[1].hp == 0);
	CHECK(logicTileAt(&g.map, 31 * TILE, 28 * TILE) == TILE_TURRET + 1);   // a crater
	CHECK(g.ubDirty >= 1 && g.pDirty[0][0] == 31 && g.pDirty[0][1] == 28);
	CHECK(g.score > ulScore);
	CHECK(g.pStructs[0].hp == FACTORY_HP);   // the gun doesn't hurt a factory
}

static void testReticleIsWhereTheBombLands(void) {
	newTheatre(504, 600);
	int16_t rx, ry;
	logicReticle(&g, &rx, &ry);
	step((tInput){.fire2 = 1}, 1);
	CHECK(g.events & EV_BOMB);
	CHECK(g.chop.bombs == BOMBS_MAX - 1);
	CHECK(g.pBombs[0].active);
	int n = 0;
	while(g.pBombs[0].active && n < 100) {
		int16_t gx = (int16_t)(g.pBombs[0].x), gy = (int16_t)(g.pBombs[0].y);
		step(NONE, 1);
		++n;
		if(!g.pBombs[0].active) {
			CHECK(gx - rx <= 3 && rx - gx <= 3 && gy - ry <= 3 && ry - gy <= 3);
		}
	}
	CHECK(n > 10 && n < 40);
}

static void testThreeBombsDestroyAFactory(void) {
	// hovering south of the factory: the bombs land 24 px ahead (north)
	newTheatre(504, 504 + 24);
	for(int i = 0; i < 3; ++i) {
		step((tInput){.fire2 = 1}, 1);
		step(NONE, 40);
		if(i == 0) {
			CHECK(g.pStructs[0].hp == FACTORY_HP - BOMB_DAMAGE);
			CHECK(logicTileAt(&g.map, 30 * TILE, 30 * TILE) == TILE_FACTORY + 9);    // damaged
		}
	}
	CHECK(g.pStructs[0].hp == 0 && g.factoriesLeft == 0);
	CHECK(logicTileAt(&g.map, 31 * TILE, 31 * TILE) == TILE_FACTORY + 18 + 4);   // rubble
	CHECK(g.events & EV_FACTORY || g.score >= 1000);
}

// A bomb landing at (x, y) this frame: 1 if it reached the factory (px 480..527 both ways), 2 the
// turret above it (496..511 x 448..463), 3 both
static uint8_t blastAt(int16_t x, int16_t y) {
	newTheatre(100, 900);
	g.pBombs[0] = (tShot){.x = x, .y = y, .active = 1, .alt = 1};
	step(NONE, 1);
	return (uint8_t)((g.pStructs[0].hp < FACTORY_HP) | (g.pStructs[1].hp < TURRET_HP) << 1);
}

// A blast reaches what's within BLAST_R of it, to the pixel (bombsUpdate's quick tests
// mustn't change that)
static void testBlastReachEdges(void) {
	CHECK(blastAt(504, 480 + 48 + BLAST_R - 1) == 1);          // below the factory: just in reach
	CHECK(blastAt(504, 480 + 48 + BLAST_R) == 0);              // just out
	CHECK(blastAt(504, 480 - BLAST_R) == 3);                   // above it: it and the turret
	CHECK(blastAt(504, 448 - BLAST_R) == 2);                   // the turret's top edge
	CHECK(blastAt(504, 448 - BLAST_R - 1) == 0);
	CHECK(blastAt(480 - BLAST_R, 504) == 1);                   // the factory's sides
	CHECK(blastAt(480 - BLAST_R - 1, 504) == 0);
	CHECK(blastAt(480 + 48 + BLAST_R - 1, 504) == 1);
	CHECK(blastAt(480 + 48 + BLAST_R, 504) == 0);
}

static void testDamagedFactoryRepairs(void) {
	newTheatre(504, 504 + 24);
	step((tInput){.fire2 = 1}, 1);
	step(NONE, 40);
	CHECK(g.pStructs[0].hp == FACTORY_HP - BOMB_DAMAGE && g.pStructs[0].stage == 1);
	g.ubDirty = 0;
	step(NONE, FACTORY_REPAIR * BOMB_DAMAGE + 10);
	CHECK(g.pStructs[0].hp == FACTORY_HP && g.pStructs[0].stage == 0);
	CHECK(logicTileAt(&g.map, 30 * TILE, 30 * TILE) == TILE_FACTORY);
	CHECK(g.ubDirty == 9);
}

static void testOneButtonHoldDropsABomb(void) {
	newTheatre(500, 900);
	step((tInput){.fire = 1}, 1);
	CHECK(g.events & EV_CANNON);             // the press fires the gun at once
	step((tInput){.fire = 1}, BOMB_HOLD - 2);
	CHECK(!g.pBombs[0].active);
	step((tInput){.fire = 1}, 1);
	CHECK(g.pBombs[0].active && g.chop.bombs == BOMBS_MAX - 1);
	step((tInput){.fire = 1}, 40);
	CHECK(g.chop.bombs == BOMBS_MAX - 1);    // one a hold
}

static void testTwoButtonsAutofire(void) {
	newTheatre(500, 900);
	step((tInput){.fire2 = 1}, 1);           // a second button: from now on fire repeats
	step(NONE, 1);
	CHECK(g.isTwoButton);
	uint8_t n = 0;
	for(int i = 0; i < CANNON_REPEAT * 2 + 1; ++i) {
		step((tInput){.fire = 1}, 1);
		n = (uint8_t)(n + ((g.events & EV_CANNON) != 0));
	}
	CHECK(n == 3);
	CHECK(g.chop.bombs == BOMBS_MAX - 1);    // and holding fire doesn't bomb
}

static void testNoBombsOnDeckOrEmpty(void) {
	newTheatre(500, 900);
	g.chop.bombs = 0;
	step((tInput){.fire2 = 1}, 1);
	CHECK(!g.pBombs[0].active);
	newTheatre(500, 900);
	logicLoadTheatre(&g, &s_sTh);            // on deck
	step((tInput){.fire2 = 1}, 1);
	CHECK(!g.pBombs[0].active && g.chop.bombs == BOMBS_MAX);
}

static void testHeadingTo(void) {
	CHECK(logicHeadingTo(0, -100) == 0);
	CHECK(logicHeadingTo(100, -100) == 2);
	CHECK(logicHeadingTo(100, 0) == 4);
	CHECK(logicHeadingTo(100, 100) == 6);
	CHECK(logicHeadingTo(0, 100) == 8);
	CHECK(logicHeadingTo(-100, 100) == 10);
	CHECK(logicHeadingTo(-100, 0) == 12);
	CHECK(logicHeadingTo(-100, -100) == 14);
	CHECK(logicHeadingTo(40, -100) == 1);      // 22 degrees
	CHECK(logicHeadingTo(-100, -40) == 13);
}

// The theatre game, with a turret 60 px south-west of the gunship
static void turretGame(void) {
	newTheatre(560, 400);
	g.pStructs[1].tx = 32; g.pStructs[1].ty = 27;      // (520, 440)
	g.pStructs[1].timer = 0;
	g.uwJetTimer = g.uwBomberTimer = g.uwBoatTimer = 60000;
}

static void testTurretFiresFlakInRange(void) {
	turretGame();
	step(NONE, 1);                                // the tell first...
	CHECK(g.pStructs[1].link & TURRET_TELL);
	CHECK(!(g.events & EV_FLAK) && !g.pEShots[0].active);
	step(NONE, TURRET_TELL_NORMAL);               // ... then the round
	CHECK(g.pEShots[0].active && g.pEShots[0].kind == ESHOT_FLAK);
	CHECK(!(g.pStructs[1].link & TURRET_TELL));
	turretGame();
	g.chop.x = 1200;                              // out of range
	step(NONE, 40);
	CHECK(!g.pEShots[0].active);
	CHECK(!(g.pStructs[1].link & TURRET_TELL));
}

// The AA guns' tell (playbook F1): no round, flak or missile, without the gun
// winding up (TURRET_TELL, its barrels held still) for logicTurretTell frames
// just before it; at least 12 frames (the playbook's floor) on every difficulty,
// hardest included (Paul, 2026-11-11). Over 3 minutes of hovering in range on
// every difficulty, on every alert level.
// The table as it plays: 16 normal; 24 / 20 / 16 / 16 / 12 on skills 1-5; GODMODE (0) is
// normal. Paul asked for 14 on skill 4; a gun is looked at every 4th frame, so 14 plays as
// 16 and the table says 16 (skill 4 stays harder through damage, aim and cooldown).
static void testAaTellTable(void) {
	static const uint8_t pWant[6] = {16, 24, 20, 16, 16, 12};
	for(uint8_t d = 0; d <= 5; ++d) {
		turretGame();
		logicSetDifficulty(&g, d);
		CHECK(logicTurretTell(&g) == pWant[d]);
		CHECK(logicTurretTell(&g) >= 12);
	}
	// what plays: exactly the table (steps of 4 frames)
	static const uint8_t pPlays[6] = {16, 24, 20, 16, 16, 12};
	for(uint8_t d = 0; d <= 5; ++d) {
		turretGame();
		logicSetDifficulty(&g, d);
		step(NONE, 1);
		CHECK(g.pStructs[1].link & TURRET_TELL);
		uint8_t n = 1;
		while((g.pStructs[1].link & TURRET_TELL) && n < 60) { step(NONE, 1); ++n; }
		--n;                                      // (pictures it was seen winding up)
		CHECK(n == pPlays[d]);
		CHECK(n >= pWant[d]);
	}
	turretGame();
	logicSetDifficulty(&g, 0);
	CHECK(logicTurretTell(&g) == TURRET_TELL_NORMAL);
	logicSetDifficulty(&g, 3);
	CHECK(logicTurretTell(&g) == TURRET_TELL_NORMAL);
}

static void testNoShotWithoutATell(void) {
	for(uint8_t d = 1; d <= 5; ++d) {
		for(uint8_t a = 0; a <= 4; a = (uint8_t)(a + 2)) {
			turretGame();
			logicSetDifficulty(&g, d);
			g.ubFactories = 6; g.factoriesLeft = (uint8_t)(6 - a);   // alert a (missiles from 3)
			g.cruiser.sunk = 1;
			uint8_t ubTell = logicTurretTell(&g);
			CHECK(ubTell >= 12 && (d != 3 || ubTell <= 18));
			uint16_t uwTellRun = 0, uwShots = 0;
			uint8_t ubAim = g.pStructs[1].link & 7, isMoved = 0;
			for(int f = 0; f < 50 * 180; ++f) {
				g.chop.damage = 0;                    // (it hovers on, unhurt)
				uint8_t ubBefore = 0;
				for(int k = 0; k < ESHOT_MAX; ++k) ubBefore = (uint8_t)(ubBefore + g.pEShots[k].active);
				step(NONE, 1);
				uint8_t ubAfter = 0;
				for(int k = 0; k < ESHOT_MAX; ++k) ubAfter = (uint8_t)(ubAfter + g.pEShots[k].active);
				if(ubAfter > ubBefore || (g.events & (EV_FLAK | EV_MISSILE))) {
					++uwShots;
					CHECK(uwTellRun >= ubTell);       // it wound up for the whole tell
					CHECK(!isMoved);                  // with the barrels held still
					CHECK(g.pStructs[1].link & TURRET_FLASH);
				}
				if(g.pStructs[1].link & TURRET_TELL) {
					if(uwTellRun && (g.pStructs[1].link & 7) != ubAim) isMoved = 1;
					if(!uwTellRun) { ubAim = g.pStructs[1].link & 7; isMoved = 0; }
					++uwTellRun;
				}
				else uwTellRun = 0;
				for(int k = 0; k < ESHOT_MAX; ++k) if(g.pEShots[k].life <= 1) g.pEShots[k].active = 0;   // (no bursts)
			}
			CHECK(uwShots >= 40);                     // and it does fire (every 2-4 s)
		}
	}
}

// A gunship that lands (or gets out of range) during the tell isn't shot at;
// a hit on the gun doesn't cut its tell short
static void testTellHoldsFireAndCantBeCut(void) {
	turretGame();
	step(NONE, 1);
	CHECK(g.pStructs[1].link & TURRET_TELL);
	g.chop.x = 1200;
	step(NONE, 40);
	for(int k = 0; k < ESHOT_MAX; ++k) CHECK(!g.pEShots[k].active);
	turretGame();
	step(NONE, 1);
	uint16_t uwTimer = g.pStructs[1].timer;
	CHECK(uwTimer > 0);
	g.pStructs[1].hp = 2;
	tShot *pS = &g.pShots[0];                     // a cannon round onto the gun
	pS->active = 1; pS->kind = SHOT_CANNON; pS->alt = 2; pS->vx = pS->vy = 0; pS->fx = pS->fy = 0;
	pS->x = (int16_t)(g.pStructs[1].tx * TILE + 7); pS->y = (int16_t)(g.pStructs[1].ty * TILE + 6);
	step(NONE, 1);
	CHECK(g.pStructs[1].hp == 1);
	CHECK(g.pStructs[1].timer + 4 >= uwTimer && g.pStructs[1].timer && !g.pEShots[0].active);
}

// Flak flies at 1.5 px a frame (playbook F2: 1-1.5) and still bursts where it
// was aimed: by a hovering gunship
// Effects are panned by where they happen on screen: a turret's flak on the
// left of the screen plays on a left channel, a boom on the right on a right
// one, the gunship's own gun (mid-screen) on either
static void testSfxPanning(void) {
	turretGame();
	g.camX = 520 - 60;                            // the turret 60 px from the left edge
	step(NONE, 1 + TURRET_TELL_NORMAL);
	CHECK(g.events & EV_FLAK);
	CHECK(logicEventX(&g, EV_FLAK) == 520);
	g.camX = 520 - 60;
	CHECK(logicSfxChannel(&g, EV_FLAK) == SFX_LEFT);
	g.camX = 520 - 300;
	CHECK(logicSfxChannel(&g, EV_FLAK) == SFX_RIGHT);
	g.camX = 520 - 170;
	CHECK(logicSfxChannel(&g, EV_FLAK) == SFX_EITHER);
	CHECK(SFX_LEFT == 0 || SFX_LEFT == 3);        // Paula's left pair
	CHECK(SFX_RIGHT == 1 || SFX_RIGHT == 2);      // and right pair
	newGame(STEER_CLASSIC, 400, 400);
	g.chop.landed = 0;
	step((tInput){.fire = 1}, 1);
	step(NONE, 1);
	step((tInput){.fire = 1}, 1);
	CHECK(g.events & EV_CANNON);
	g.camX = (int16_t)(g.chop.x - SCREEN_W / 2);
	CHECK(logicSfxChannel(&g, EV_CANNON) == SFX_EITHER);
}

// The pacing counts: flak in view is threat, the cruiser's shells aren't
static void testThreatCounts(void) {
	turretGame();
	g.camX = 520 - 160; g.camY = 440 - 100;
	uint16_t uwFired = g.uwShotsFired;
	step(NONE, 1 + TURRET_TELL_NORMAL);
	uint8_t a, s;
	g.camX = 520 - 160; g.camY = 440 - 100;
	logicThreat(&g, &a, &s);
	CHECK(s == 1 && a == 0 && g.uwShotsFired == uwFired + 1);
	g.pEShots[0].kind = ESHOT_SHELL;
	logicThreat(&g, &a, &s);
	CHECK(s == 0);
	g.camX = 2000;
	g.pEShots[0].kind = ESHOT_FLAK;
	logicThreat(&g, &a, &s);
	CHECK(s == 0);
}

// The turrets' list is top to bottom, and every gun on screen is drawn
static void testTurretsSortedAndAllDrawn(void) {
	const tTheatreObj pObjs[] = {
		{OBJ_CARRIER, 20, 60, 10, 40, 0},
		{OBJ_TURRET, 10, 30, 0, 0, 0}, {OBJ_TURRET, 12, 5, 0, 0, 0}, {OBJ_TURRET, 14, 18, 0, 0, 0},
		{OBJ_TURRET, 3, 12, 0, 0, 0}, {OBJ_TURRET, 9, 13, 0, 0, 0},
	};
	uint32_t ulSize = makeTheatre(80, 80, pObjs, 6);
	CHECK(theatreParse(s_pFile, ulSize, &s_sTh));
	logicInit(&g, s_sTh.map.pTiles, s_sTh.map.uwW, s_sTh.map.uwH);
	logicLoadTheatre(&g, &s_sTh);
	CHECK(g.ubTurrets == 5);
	for(uint8_t i = 1; i < g.ubTurrets; ++i) CHECK(g.pStructs[g.pTurrets[i - 1]].ty <= g.pStructs[g.pTurrets[i]].ty);
	tDraw pDraw[DRAW_MAX];
	for(int16_t cy = 0; cy < 400; cy = (int16_t)(cy + 7)) {
		g.camX = 0; g.camY = cy;
		uint8_t n = logicDraw(&g, pDraw), ubGuns = 0, ubWant = 0;
		for(uint8_t i = 0; i < n; ++i) ubGuns = (uint8_t)(ubGuns + (pDraw[i].bob == BOB_GUN));
		for(uint8_t i = 0; i < g.ubTurrets; ++i) {
			int16_t sy = (int16_t)((g.pStructs[g.pTurrets[i]].ty << 4) - cy);
			ubWant = (uint8_t)(ubWant + (sy > -16 && sy < PLAY_H));
		}
		CHECK(ubGuns == ubWant);
	}
}

static void testFlakSpeed(void) {
	turretGame();
	step(NONE, 1 + TURRET_TELL_NORMAL);
	const tEShot *e = &g.pEShots[0];
	CHECK(e->active && e->kind == ESHOT_FLAK);
	int16_t ax = e->vx < 0 ? (int16_t)-e->vx : e->vx, ay = e->vy < 0 ? (int16_t)-e->vy : e->vy;
	int16_t wSpeed = ax > ay ? (int16_t)(ax + (ay * 41 >> 7)) : (int16_t)(ay + (ax * 41 >> 7));   // ~ hypot
	CHECK(wSpeed >= 21 && wSpeed <= 27);          // 24: 1.5 px a frame
	CHECK(FLAK_SPEED >= 16 && FLAK_SPEED <= 24);
}

// The gun turns a step at a time to aim at the gunship (north-east of it
// here), flashes when it fires, and is drawn on its turret; a crater has none
static void testTurretGunAimsAndFlashes(void) {
	turretGame();
	g.pStructs[1].link = 5;                       // aimed south-west
	g.pStructs[1].timer = 20;                     // (turning before its tell)
	uint8_t ubFlash = 0, ubTurns = 0, ubLast = 5;
	for(int i = 0; i < 40; ++i) {
		step(NONE, 1);
		uint8_t ubAim = g.pStructs[1].link & 7;
		if(ubAim != ubLast) { CHECK(((ubAim - ubLast) & 7) == 1 || ((ubLast - ubAim) & 7) == 1); ++ubTurns; ubLast = ubAim; }
		if(g.pStructs[1].link & TURRET_FLASH) ubFlash = 1;
	}
	CHECK((g.pStructs[1].link & 7) == 1);         // north-east
	CHECK(ubTurns == 4 && ubFlash);
	tDraw pDraw[DRAW_MAX];
	g.camX = 520 - 160; g.camY = 440 - 100;
	uint8_t n = logicDraw(&g, pDraw), ubGuns = 0;
	for(uint8_t i = 0; i < n; ++i) if(pDraw[i].bob == BOB_GUN) {
		++ubGuns;
		CHECK(pDraw[i].x == 520 - 8 - g.camX && pDraw[i].y == 440 - 8 - g.camY);
		CHECK((pDraw[i].frame & 7) == 1);
	}
	CHECK(ubGuns == 1);
	g.pStructs[1].link = 3 | TURRET_TELL;         // winding up: frames 16-23
	n = logicDraw(&g, pDraw);
	for(uint8_t i = 0; i < n; ++i) if(pDraw[i].bob == BOB_GUN) CHECK(pDraw[i].frame == 16 + 3 && pDraw[i].frame < g_pBobFrames[BOB_GUN]);
	g.pStructs[1].hp = 0;
	n = logicDraw(&g, pDraw);
	for(uint8_t i = 0; i < n; ++i) CHECK(pDraw[i].bob != BOB_GUN);
}

static void testFlakHurtsAndGodmodeShrugs(void) {
	turretGame();
	g.uwJetTimer = 60000;
	step(NONE, 80);                               // it bursts by the hovering gunship
	CHECK(g.chop.damage > 0);
	turretGame();
	logicSetDifficulty(&g, 0);
	step(NONE, 80);
	CHECK(g.chop.damage == 0);
}

static void testCrashRespawnAndGameOver(void) {
	newTheatre(800, 300);
	g.uwJetTimer = g.uwBomberTimer = g.uwBoatTimer = 60000;
	g.lives = 2;
	logicHitChopper(&g, DAMAGE_MAX);
	CHECK(g.state == GS_LOST && g.lives == 1 && g.chop.crashed);
	CHECK(g.events & EV_CRASH);
	step(NONE, LOST_FRAMES + 1);
	CHECK(g.state == GS_PLAY && !g.chop.crashed && g.chop.landed && logicOverDeck(&g) == 1);
	CHECK(g.chop.damage == 0 && g.chop.bombs == BOMBS_MAX);
	g.chop.landed = 0;
	logicHitChopper(&g, DAMAGE_MAX);
	step(NONE, LOST_FRAMES + 1);
	CHECK(g.state == GS_OVER);
	step(NONE, OVER_FRAMES + 1);
	CHECK(g.state == GS_TITLE);
}

static void testMissilesFromAlertThree(void) {
	turretGame();
	g.ubFactories = 6; g.factoriesLeft = 3;       // half of them gone
	CHECK(logicAlert(&g) == 3);
	uint8_t isMissile = 0;
	for(int i = 0; i < 30; ++i) {
		g.pStructs[1].timer = 0;
		step(NONE, 1);
		for(int k = 0; k < ESHOT_MAX; ++k) isMissile |= g.pEShots[k].active && g.pEShots[k].kind == ESHOT_MISSILE;
		for(int k = 0; k < ESHOT_MAX; ++k) g.pEShots[k].active = 0;
	}
	CHECK(isMissile);
}

static void testJetHuntsAndCannonKillsIt(void) {
	newTheatre(600, 600);
	g.uwBomberTimer = g.uwBoatTimer = 60000;
	g.pStructs[1].hp = 0;                         // (no turret)
	g.uwJetTimer = 1;
	step(NONE, 1);
	CHECK(g.pAir[0].active && g.pAir[0].kind == AIR_JET);
	// put it ahead of the gunship (north), coming south at it
	g.pAir[0].x = 600; g.pAir[0].y = 560; g.pAir[0].heading = 8; g.pAir[0].timer = 200;
	step((tInput){.fire = 1}, 1);
	step(NONE, 2);
	step((tInput){.fire = 1}, 1);
	step(NONE, 4);
	CHECK(!g.pAir[0].active);
	CHECK(g.score >= 200);
}

static void testBomberHitsTheCarrier(void) {
	newTheatre(100, 100);
	g.uwJetTimer = g.uwBoatTimer = 60000;
	g.pStructs[1].hp = 0;
	g.uwBomberTimer = 1;
	step(NONE, 1);
	CHECK(g.pAir[0].active && g.pAir[0].kind == AIR_BOMBER);
	g.pAir[0].x = (int16_t)(g.carrier.x - 100); g.pAir[0].y = g.carrier.y; g.pAir[0].heading = 4;
	uint8_t hp = g.carrier.hp;
	step(NONE, 120);
	CHECK(g.carrier.hp == hp - BOMBER_DAMAGE);
	CHECK(g.pAir[0].done);
}

static void testCarrierSunkMeansAirstrip(void) {
	const tTheatreObj pObjs[] = {
		{OBJ_CARRIER, 20, 60, 10, 40, 0},
		{OBJ_FACTORY, 30, 30, 0, 0, 0},
		{OBJ_AIRSTRIP, 50, 50, 0, 0, 0},
	};
	uint32_t ulSize = makeTheatre(80, 80, pObjs, 3);
	CHECK(theatreParse(s_pFile, ulSize, &s_sTh));
	logicInit(&g, s_sTh.map.pTiles, s_sTh.map.uwW, s_sTh.map.uwH);
	logicLoadTheatre(&g, &s_sTh);
	g.state = GS_PLAY;
	g.carrier.hp = 0;
	logicRespawn(&g);
	CHECK(g.chop.x == 50 * TILE + 8 && g.chop.y == 50 * TILE + 8);
	CHECK(logicOverDeck(&g) == 2);
	g.chop.damage = 40;
	step(NONE, 50 * 10);
	CHECK(g.chop.landed && g.chop.damage == 0);  // the airstrip repairs too
}

static void testBoatDeliversAndRepairs(void) {
	const tTheatreObj pObjs[] = {
		{OBJ_CARRIER, 20, 60, 10, 40, 0},
		{OBJ_FACTORY, 30, 30, 0, 0, 0},
		{OBJ_PIER, 33, 31, 0, 1, 0},          // factory 0, out to sea: east
	};
	uint32_t ulSize = makeTheatre(80, 80, pObjs, 3);
	CHECK(theatreParse(s_pFile, ulSize, &s_sTh));
	logicInit(&g, s_sTh.map.pTiles, s_sTh.map.uwW, s_sTh.map.uwH);
	logicLoadTheatre(&g, &s_sTh);
	g.state = GS_PLAY;
	g.uwJetTimer = g.uwBomberTimer = 60000;
	g.pStructs[0].hp = 10;
	g.pStructs[0].stage = 1;
	g.uwBoatTimer = 1;
	step(NONE, 1);
	CHECK(g.pBoats[0].active && g.pBoats[0].x > 33 * TILE + 8 && g.pBoats[0].y == 31 * TILE + 8);
	int n = 0;
	while(!g.pBoats[0].leaving && n < 10000) { step(NONE, 1); ++n; }
	CHECK(g.pBoats[0].leaving);
	CHECK(g.pStructs[0].hp >= 10 + BOAT_REPAIR);
}

static void testCruiserSailsAndShipyardSetsBack(void) {
	const tTheatreObj pObjs[] = {
		{OBJ_CARRIER, 40, 60, 20, 60, 0},
		{OBJ_FACTORY, 30, 30, 0, 0, 0},
		{OBJ_SHIPYARD, 10, 10, 0, 0, 0},
	};
	uint32_t ulSize = makeTheatre(80, 80, pObjs, 3);
	CHECK(theatreParse(s_pFile, ulSize, &s_sTh));
	logicInit(&g, s_sTh.map.pTiles, s_sTh.map.uwW, s_sTh.map.uwH);
	logicLoadTheatre(&g, &s_sTh);
	g.state = GS_PLAY;
	g.uwJetTimer = g.uwBomberTimer = g.uwBoatTimer = 60000;
	g.uwBuild = 5000;
	// a bomb on the shipyard: hover south of it, drop
	g.chop.landed = 0; g.chop.alt = ALT_CRUISE; g.chop.heading = 0;
	g.chop.x = 10 * TILE + 32; g.chop.y = 10 * TILE + 16 + 24;
	step((tInput){.fire2 = 1}, 1);
	step(NONE, 30);
	CHECK(g.uwBuild < 5000 + 31 - CRUISER_SETBACK + 1);
	g.uwBuild = CRUISER_BUILD - 1;
	step(NONE, 2);
	CHECK(g.cruiser.active && (g.events & EV_CRUISER || g.cruiser.hp == CRUISER_HP));
	uint8_t hp = g.carrier.hp;
	step(NONE, 50 * 60 * 3);
	CHECK(g.carrier.hp < hp);                 // it got in range and shelled the carrier
}

// The cruiser as a set piece: ignored for 45 s it closes in faster and fires
// twice as often, at 90 s it launches jets at the gunship every 20 s; a hit
// restarts the clock; sunk, 3000 points and the carrier repaired
static void testCruiserEscalatesAndRewards(void) {
	const tTheatreObj pObjs[] = {
		{OBJ_CARRIER, 40, 60, 20, 60, 0},
		{OBJ_FACTORY, 30, 30, 0, 0, 0},
		{OBJ_SHIPYARD, 10, 10, 0, 0, 0},
	};
	uint32_t ulSize = makeTheatre(80, 80, pObjs, 3);
	CHECK(theatreParse(s_pFile, ulSize, &s_sTh));
	logicInit(&g, s_sTh.map.pTiles, s_sTh.map.uwW, s_sTh.map.uwH);
	logicLoadTheatre(&g, &s_sTh);
	g.state = GS_PLAY;
	g.uwJetTimer = g.uwBomberTimer = g.uwBoatTimer = 60000;
	g.chop.landed = 0; g.chop.alt = ALT_CRUISE; g.chop.x = 300; g.chop.y = 300;
	logicSetDifficulty(&g, 0);
	g.uwBuild = CRUISER_BUILD - 1;
	step(NONE, 2);
	CHECK(g.cruiser.active && g.cruiser.ubLevel == 0);
	uint8_t isUp = 0, ubJets = 0;
	for(int f = 0; f < CRUISER_STALL1 + 10; ++f) {
		g.carrier.hp = CARRIER_HP;
		step(NONE, 1);
		if(g.events & EV_CRUISER_UP) { isUp = 1; CHECK(f >= CRUISER_STALL1 - 5); }
	}
	CHECK(isUp && g.cruiser.ubLevel == 1 && g.msg == MSG_CRUISER_UP);
	for(int f = 0; f < 600; ++f) { g.carrier.hp = CARRIER_HP; step(NONE, 1); }
	g.uwBuild = 0;
	// a hit restarts the clock
	uint8_t hp = g.cruiser.hp;
	CHECK(enemiesShot(&g, g.cruiser.x, g.cruiser.y, 0));
	CHECK(g.cruiser.hp == hp - 1 && g.cruiser.uwStall == 0 && g.cruiser.ubLevel == 1);
	for(int f = 0; f < CRUISER_STALL2 + 2 * CRUISER_JETS + 10; ++f) {
		g.carrier.hp = CARRIER_HP;
		uint8_t n = 0;
		for(int i = 0; i < AIR_MAX; ++i) n = (uint8_t)(n + g.pAir[i].active);
		step(NONE, 1);
		uint8_t m = 0;
		for(int i = 0; i < AIR_MAX; ++i) m = (uint8_t)(m + g.pAir[i].active);
		if(m > n) ++ubJets;
	}
	CHECK(g.cruiser.ubLevel == 2 && ubJets == 3);  // at 90 s, 110 s and 130 s
	// sunk: the reward
	g.carrier.hp = 20;
	uint32_t ulScore = g.score;
	g.cruiser.hp = 1;
	enemiesShot(&g, g.cruiser.x, g.cruiser.y, 0);
	CHECK(g.cruiser.sunk && !g.cruiser.active && (g.events & EV_CRUISER_SUNK));
	CHECK(g.score == ulScore + CRUISER_SCORE && g.carrier.hp == CARRIER_HP);
}

// ---- jets and the carrier's deck (1.4.1) ----
// Jets used to home on the gunship on the deck, overshoot and turn back
// round it for as long as it sat there, then ambush it on take-off. Now they
// break off while it is landed (or going down), and for a short grace after
// take-off; and no more than one jet commits to a gunship near the carrier.
#define DECK_NEAR 150          // px from the gunship: "over the carrier"
#define DECK_HOLD_MAX 100      // frames a jet may spend within DECK_NEAR (a fly-through takes about 90)

static tGame *s_pG;
static uint8_t s_pWas[AIR_MAX];
static uint16_t s_pRun[AIR_MAX];
static uint16_t s_uwMaxRun, s_uwHover, s_uwAttack, s_uwJetsSeen;

static void deckWatch(void) {
	for(int i = 0; i < AIR_MAX; ++i) {
		const tAir *a = &g.pAir[i];
		if(!a->active || a->kind != AIR_JET) { s_pWas[i] = 0; s_pRun[i] = 0; continue; }
		if(!s_pWas[i]) ++s_uwJetsSeen;
		s_pWas[i] = 1;
		int16_t d = logicDist((int16_t)(a->x - g.chop.x), (int16_t)(a->y - g.chop.y));
		if(d < DECK_NEAR) {
			++s_uwHover;
			if(++s_pRun[i] > s_uwMaxRun) s_uwMaxRun = s_pRun[i];
			if(!a->done) ++s_uwAttack;
		}
		else s_pRun[i] = 0;
	}
}

static void deckTheatre(void) {
	const tTheatreObj pObjs[] = {
		{OBJ_CARRIER, 20, 60, 10, 40, 0},
		{OBJ_FACTORY, 30, 30, 0, 0, 0},
		{OBJ_RADAR, 60, 20, 0, 0, 0},
	};
	uint32_t ulSize = makeTheatre(80, 80, pObjs, 3);
	CHECK(theatreParse(s_pFile, ulSize, &s_sTh));
	logicInit(&g, s_sTh.map.pTiles, s_sTh.map.uwW, s_sTh.map.uwH);
	logicLoadTheatre(&g, &s_sTh);
	g.state = GS_PLAY;
	g.steer = STEER_CLASSIC;
	g.uwBomberTimer = g.uwBoatTimer = 60000;
	g.uwJetTimer = 1;
	for(int i = 0; i < AIR_MAX; ++i) s_pWas[i] = 0, s_pRun[i] = 0;
	s_uwMaxRun = s_uwHover = s_uwAttack = s_uwJetsSeen = 0;
}

static void testJetsDoNotCampTheDeck(void) {
	deckTheatre();
	CHECK(g.chop.landed);
	// the cruiser sits by the carrier, 90 s ignored: a jet every 20 s at the gunship
	g.cruiser.active = 1;
	g.cruiser.hp = CRUISER_HP;
	g.cruiser.x = (int16_t)(g.carrier.x + 100);
	g.cruiser.y = g.carrier.y;
	g.cruiser.dir = -1;
	g.cruiser.ubLevel = 2;
	g.cruiser.uwStall = CRUISER_STALL2 - 1;
	// three jets already circling the deck when it sits down
	for(int k = 0; k < 3; ++k) {
		tAir *a = &g.pAir[k];
		a->active = 1; a->kind = AIR_JET; a->hp = JET_HP; a->timer = 60; a->done = 0; a->uwLife = 1500;
		a->x = (int16_t)(g.chop.x + (k - 1) * 70); a->y = (int16_t)(g.chop.y - 60 - 20 * k);
		a->heading = (uint8_t)(k * 5); a->fx = a->fy = 0;
	}
	// 80 s sitting on deck, a jet called every 6 s on top of the natural ones
	for(int f = 0; f < 4000; ++f) {
		if(!(f % 300)) g.uwJetTimer = 1;
		g.carrier.hp = CARRIER_HP;
		step(NONE, 1);
		deckWatch();
		CHECK(g.chop.landed);
	}
	printf("deck, landed 80 s: jets %u, frames within %d px %u, longest stay %u frames, attack frames %u\n",
		s_uwJetsSeen, DECK_NEAR, s_uwHover, s_uwMaxRun, s_uwAttack);
	CHECK(s_uwMaxRun <= DECK_HOLD_MAX);
	CHECK(s_uwAttack == 0);
	// take off with the jet timer due: the grace, then jets come again
	g.uwJetTimer = 1;
	s_uwMaxRun = s_uwHover = s_uwAttack = 0;
	step((tInput){.dy = -1}, 1);
	CHECK(!g.chop.landed);
	for(int f = 0; f < JET_DECK_GRACE - 2; ++f) {
		step((tInput){.dy = -1}, 1);
		deckWatch();
	}
	printf("deck, take-off grace %d frames: frames within %d px %u, longest stay %u, attack frames %u\n",
		JET_DECK_GRACE, DECK_NEAR, s_uwHover, s_uwMaxRun, s_uwAttack);
	CHECK(s_uwMaxRun <= DECK_HOLD_MAX && s_uwAttack == 0);
	// ... and then the jets are back in open play: one is called and hunts
	step((tInput){.dy = -1}, 400);
	uint8_t ubJets = 0;
	for(int i = 0; i < AIR_MAX; ++i) ubJets = (uint8_t)(ubJets + (g.pAir[i].active && g.pAir[i].kind == AIR_JET && !g.pAir[i].done));
	CHECK(ubJets >= 1);
}

// Two jets coming at a gunship that is flying near the carrier: one commits
static void testOneJetAtATimeNearTheCarrier(void) {
	deckTheatre();
	g.uwJetTimer = 60000;
	g.chop.landed = 0;
	g.chop.alt = ALT_CRUISE;
	g.chop.x = (int16_t)(g.carrier.x + 80);
	g.chop.y = (int16_t)(g.carrier.y - 80);
	for(int k = 0; k < 2; ++k) {
		tAir *a = &g.pAir[k];
		a->active = 1; a->kind = AIR_JET; a->hp = JET_HP; a->timer = 60; a->done = 0; a->uwLife = 1500;
		a->x = (int16_t)(g.carrier.x - 150 + k * 300); a->y = (int16_t)(g.carrier.y - 220);
		a->heading = k ? 12 : 4; a->fx = a->fy = 0;
	}
	uint16_t uwBad = 0;
	for(int f = 0; f < 600; ++f) {
		g.chop.x = (int16_t)(g.carrier.x + 80);
		g.chop.y = (int16_t)(g.carrier.y - 80);
		g.chop.damage = 0;
		step(NONE, 1);
		uint8_t n = 0;
		for(int i = 0; i < AIR_MAX; ++i) {
			const tAir *a = &g.pAir[i];
			n = (uint8_t)(n + (a->active && a->kind == AIR_JET && !a->done &&
				logicDist((int16_t)(a->x - g.carrier.x), (int16_t)(a->y - g.carrier.y)) < JET_DECK_ZONE));
		}
		if(n > 1) ++uwBad;
	}
	printf("two jets near the carrier: frames with both committed %u\n", uwBad);
	CHECK(uwBad == 0);
}

// ---- the interceptors' missile tell (1.4: Paul, 2026-11-11) ----
// A jet that means to fire flashes (its drawn frame is 8 + the heading: the
// nose and wingtips flare) for logicJetTell frames, a lock-on beep (EV_LOCKON)
// sounding as it starts, and only then does the missile leave. 16 frames on
// normal, scaled by difficulty like the AA table: 24 / 20 / 16 / 14 / 12.

// the flying gunship, hovering at (800, 500) with a radar up and no turret;
// the camera settled on it
static void jetTellGame(uint8_t ubDifficulty, uint8_t ubAlert) {
	deckTheatre();
	g.uwJetTimer = 60000;
	g.uwDeckGrace = 0;
	g.chop.landed = 0;
	g.chop.landTimer = 0;
	g.chop.alt = ALT_CRUISE;
	g.chop.x = 800; g.chop.y = 500;
	g.chop.vx = g.chop.vy = 0;
	g.pStructs[1].hp = 0;                         // (the factory is struct 1: nothing else fires)
	for(uint8_t i = 0; i < g.ubStructs; ++i) if(g.pStructs[i].type == OBJ_TURRET) g.pStructs[i].hp = 0;
	logicSetDifficulty(&g, ubDifficulty);
	g.ubFactories = 6; g.factoriesLeft = (uint8_t)(6 - ubAlert);
	g.cruiser.sunk = 1;
	step(NONE, 200);
	g.chop.damage = 0;
}

// a jet 130-ish px from the gunship, heading for it
static void jetPlace(uint8_t k) {
	static const int8_t pOx[8] = {120, 85, 0, -85, -120, -85, 0, 85}, pOy[8] = {0, 85, 120, 85, 0, -85, -120, -85};
	tAir *a = &g.pAir[0];
	a->active = 1; a->kind = AIR_JET; a->hp = JET_HP; a->timer = 60; a->done = 0; a->uwLife = 1500; a->ubTell = 0;
	a->x = (int16_t)(g.chop.x + pOx[k & 7]); a->y = (int16_t)(g.chop.y + pOy[k & 7]);
	a->fx = a->fy = 0;
	a->heading = logicHeadingTo((int16_t)(g.chop.x - a->x), (int16_t)(g.chop.y - a->y));
}

// the frame the first jet is drawn with (0xFF: not on screen)
static uint8_t jetFrame(void) {
	static tDraw pDraw[DRAW_MAX];
	uint8_t n = logicDraw(&g, pDraw);
	for(uint8_t i = 0; i < n; ++i) if(pDraw[i].bob == BOB_JET) return pDraw[i].frame;
	return 0xFF;
}

static uint8_t missilesLive(void) {
	uint8_t n = 0;
	for(int k = 0; k < ESHOT_MAX; ++k) n = (uint8_t)(n + (g.pEShots[k].active && g.pEShots[k].kind == ESHOT_MISSILE));
	return n;
}

static void testJetTellTable(void) {
	static const uint8_t pWant[6] = {16, 24, 20, 16, 14, 12};
	for(uint8_t d = 0; d <= 5; ++d) {
		jetTellGame(d, 0);
		CHECK(logicJetTell(&g) == pWant[d]);
		CHECK(logicJetTell(&g) >= 12);
	}
	jetTellGame(0, 0);
	CHECK(logicJetTell(&g) == JET_TELL_NORMAL);   // GODMODE plays like normal
	CHECK(g_pBobFrames[BOB_JET] == 16);           // 8 headings, then the same 8 flashing
	CHECK(MISSILE_SPEED == 44);                   // the missile's speed is unchanged
}

// No jet missile without its whole tell, on every difficulty and every alert
// level: the flash for logicJetTell pictures, started by the beep, ended by the missile
static void testNoJetMissileWithoutATell(void) {
	uint16_t uwTotal = 0;
	for(uint8_t d = 0; d <= 5; ++d) {
		for(uint8_t a = 0; a <= 5; ++a) {
			jetTellGame(d, a);
			uint8_t ubTell = logicJetTell(&g);
			uint16_t uwRun = 0, uwShots = 0, uwPlaced = 0;
			uint8_t isBeep = 0;
			for(int f = 0; f < 3000; ++f) {
				if(!g.pAir[0].active) jetPlace((uint8_t)(uwPlaced++));
				g.chop.damage = 0;
				g.chop.x = 800; g.chop.y = 500;
				step(NONE, 1);
				uint8_t ubFrame = jetFrame();
				uint8_t isFlash = ubFrame != 0xFF && ubFrame >= 8;
				uint8_t isFired = missilesLive() || (g.events & EV_MISSILE);
				if(g.events & EV_LOCKON) {
					CHECK(!uwRun);                    // a tell begins from nothing...
					CHECK(isFlash);                   // ... flashing at once
					isBeep = 1;
				}
				if(isFired) {
					++uwShots;
					CHECK(uwRun == ubTell);           // the whole tell, no more no less
					CHECK(isBeep);                    // started by the beep
					CHECK(!isFlash || !g.pAir[0].active);
					uwRun = 0;
					isBeep = 0;
				}
				else if(isFlash) ++uwRun;
				else { uwRun = 0; isBeep = 0; }
				for(int k = 0; k < ESHOT_MAX; ++k) g.pEShots[k].active = 0;
			}
			CHECK(uwShots >= 5);                      // and the jets do fire
			uwTotal = (uint16_t)(uwTotal + uwShots);
		}
	}
	printf("jet tell: %u missiles over 36 difficulty/alert runs, every one after its full tell\n", uwTotal);
}

// The tell is cancelled when the jet leaves the screen, breaks off, is lost
// from the gunship (it lands, or is more than JET_TELL_RANGE away): no missile
static void testJetTellAborts(void) {
	for(uint8_t ubWhy = 0; ubWhy < 4; ++ubWhy) {
		jetTellGame(3, 0);
		tAir *a = &g.pAir[0];
		jetPlace(0);                              // 120 px east: on screen? the middle is within 160 px: yes
		a->x = (int16_t)(g.chop.x + 90);
		a->y = (int16_t)(g.chop.y - 20);
		a->heading = 12;
		a->timer = 0;
		step(NONE, 1);
		CHECK((g.events & EV_LOCKON) && a->ubTell);
		CHECK(jetFrame() >= 8 && jetFrame() != 0xFF);
		step(NONE, 3);
		CHECK(a->ubTell);
		if(ubWhy == 0) { a->x = (int16_t)(g.chop.x + 400); a->heading = 4; }   // off screen, flying away
		if(ubWhy == 1) a->done = 1;                                           // it breaks off
		if(ubWhy == 2) g.chop.landed = 1;                                     // the gunship lands
		if(ubWhy == 3) { a->x = (int16_t)(g.chop.x + 150); a->y = (int16_t)(g.chop.y + 100); a->heading = 4; }   // on screen, but 200 px away
		uint8_t ubFrames = 0;
		for(int f = 0; f < 60; ++f) {
			step(NONE, 1);
			CHECK(!missilesLive() && !(g.events & EV_MISSILE));
			if(!a->ubTell && !ubFrames) ubFrames = (uint8_t)(f + 1);
		}
		CHECK(ubFrames == 1);                     // the tell stopped at once
		CHECK(!a->ubTell && (jetFrame() == 0xFF || jetFrame() < 8));
	}
	// after an abort the jet waits before it tries again
	jetTellGame(3, 0);
	tAir *a = &g.pAir[0];
	jetPlace(0);
	a->x = (int16_t)(g.chop.x + 90); a->y = (int16_t)(g.chop.y - 20); a->heading = 12; a->timer = 0;
	step(NONE, 2);
	CHECK(a->ubTell);
	a->x = (int16_t)(g.chop.x + 400); a->heading = 4;
	step(NONE, 1);
	CHECK(!a->ubTell && a->timer >= JET_TELL_PAUSE - 1);
}

static void testClearTheatreThenNext(void) {
	newTheatre(800, 300);
	g.uwJetTimer = g.uwBomberTimer = g.uwBoatTimer = 60000;
	g.pStructs[0].hp = 0;
	g.factoriesLeft = 0;
	step(NONE, 1);
	CHECK(g.state == GS_CLEAR);
	step(NONE, CLEAR_FRAMES + 1);
	CHECK(g.theatre == 1 && g.wantTheatre == 1 && g.state == GS_CARD);   // the next theatre's card first
	g.wantTheatre = 0xFF;                    // (as loading it does)
	g.theatre = THEATRES - 1;
	g.factoriesLeft = 0;
	g.state = GS_PLAY;
	step(NONE, CLEAR_FRAMES + 2);
	CHECK(g.state == GS_VICTORY);
}

static void testTitleStartsAGame(void) {
	newTheatre(800, 300);
	g.state = GS_TITLE;
	g.score = 1234;
	step((tInput){.fire = 1}, 3);
	CHECK(g.state == GS_TITLE);              // fire held from before: let go first
	step(NONE, 1);
	step((tInput){.dx = 1}, 1);
	CHECK(g.steer == STEER_ARCADE);
	step((tInput){.fire = 1}, 1);
	CHECK(g.state == GS_CARD && g.wantTheatre == 0 && g.score == 0 && g.lives == LIVES_START);   // the card first
}

static void testTestJumpOnlyInGodmode(void) {
	newTheatre(800, 300);
	CHECK(!logicTestJump(&g));
	logicSetDifficulty(&g, 0);
	CHECK(logicTestJump(&g));
	CHECK(g.chop.x == 30 * TILE + 24 && g.chop.y == 30 * TILE + 48 && g.chop.alt == ALT_CRUISE);
	step((tInput){.fire2 = 1}, 1);
	step(NONE, 30);
	CHECK(g.pStructs[0].hp == FACTORY_HP - BOMB_DAMAGE);   // the bomb lands on it
}

static void testLaterTheatresAreHarder(void) {
	newTheatre(800, 300);
	CHECK(logicFactoryHp(&g) == FACTORY_HP && logicCruiserBuild(&g) == CRUISER_BUILD);
	CHECK(logicMissileAlert(&g) == MISSILE_ALERT);    // Coral Reach: as in 1.0
	g.theatre = 1;
	logicLoadTheatre(&g, &s_sTh);
	CHECK(g.pStructs[0].hp == FACTORY_HP + BOMB_DAMAGE);   // four bombs
	CHECK(logicCruiserBuild(&g) < CRUISER_BUILD && logicMissileAlert(&g) < MISSILE_ALERT);
	g.theatre = 2;
	logicLoadTheatre(&g, &s_sTh);
	CHECK(g.pStructs[0].hp == FACTORY_HP + 2 * BOMB_DAMAGE); // five
	// a boat's delivery mends up to the theatre's strength
	g.pStructs[0].hp = 30;
	logicStructRepair(&g, &g.pStructs[0], 20);
	CHECK(g.pStructs[0].hp == FACTORY_HP + 2 * BOMB_DAMAGE);
}

static void testHomePointer(void) {
	newTheatre(100, 100);                     // carrier at (328, 968): far south-east
	uint8_t h;
	int16_t d;
	CHECK(logicHomeBearing(&g, &h, &d));
	CHECK(h == logicHeadingTo(328 - 100, 968 - 100) && d > 800);
	tDraw pDraw[DRAW_MAX];
	g.camX = 0; g.camY = 0;
	uint8_t n = logicDraw(&g, pDraw), isFound = 0;
	for(uint8_t i = 0; i < n; ++i) {
		if(pDraw[i].bob != BOB_POINTER) continue;
		isFound = 1;
		// on the screen's edge box, toward the carrier (down, a little right)
		CHECK(pDraw[i].y == PLAY_H - 12 - 8);
		CHECK(pDraw[i].x > SCREEN_W / 2 - 8 && pDraw[i].x < SCREEN_W - 12);
	}
	CHECK(isFound);
	// on screen: no pointer
	g.camX = (int16_t)(g.carrier.x - SCREEN_W / 2); g.camY = (int16_t)(g.carrier.y - PLAY_H / 2);
	n = logicDraw(&g, pDraw);
	for(uint8_t i = 0; i < n; ++i) CHECK(pDraw[i].bob != BOB_POINTER);
}

static void testReloadAndRepairPrompts(void) {
	newTheatre(500, 900);
	g.chop.bombs = 1;
	step((tInput){.fire2 = 1}, 1);
	CHECK(g.msg == MSG_RELOAD);
	newTheatre(500, 900);
	g.uwJetTimer = g.uwBomberTimer = g.uwBoatTimer = 60000;
	logicHitChopper(&g, DAMAGE_WARN);
	CHECK(g.msg == MSG_REPAIR);
}

// ---- the hall of fame (1.5) ----

static void endGameWith(uint32_t ulScore, uint8_t ubTheatre, uint8_t ubState) {
	newTheatre(800, 300);
	g.uwJetTimer = g.uwBomberTimer = g.uwBoatTimer = 60000;
	g.score = ulScore;
	g.theatre = ubTheatre;
	g.killed = 7;
	g.state = ubState;
	g.stateTimer = 1;
	step(NONE, 1);
}

// Defaults best first; the HUD's HI is the best; the file's config matches the game
static void testHallDefaults(void) {
	newTheatre(800, 300);
	for(uint8_t i = 1; i < HI_COUNT; ++i) CHECK(g.table.pEntries[i].score < g.table.pEntries[i - 1].score);
	CHECK(g.hiScore == hiTop(&g.table) && g.hiScore == 10000 && g.newRank == HI_COUNT);
	CHECK(g_sHiConfig.ubReachedMax == THEATRES + 1);
	uint8_t pDisk[HI_BYTES];
	hiPack(&g.table, &g_sHiConfig, pDisk);
	tHiTable t = g.table;
	t.pEntries[2].score = 1;
	CHECK(hiUnpack(&t, &g_sHiConfig, pDisk) && t.pEntries[2].score == g.table.pEntries[2].score);
	pDisk[10] ^= 4;                               // damaged: the table stays as it is
	t.pEntries[2].score = 1;
	CHECK(!hiUnpack(&t, &g_sHiConfig, pDisk) && t.pEntries[2].score == 1);
	CHECK(logicReachedWord(1)[0] == 'C' && logicReachedWord(2)[0] == 'E' && logicReachedWord(3)[0] == 'F' &&
		logicReachedWord(THEATRES + 1)[0] == 'A');
}

// A score that makes the table: the initials with the stick, then the hall of
// fame with the entry in place, to be saved; the theatre reached and the
// factories destroyed go with it
static void testScoreMakesTheHallOfFame(void) {
	endGameWith(4500, 1, GS_OVER);                  // Ember Flats
	CHECK(g.state == GS_ENTRY && g.newRank == 2);
	CHECK(g.table.pEntries[3].score == 3000 && g.table.pEntries[4].score == 1500);   // shifted down
	CHECK(g.table.pEntries[2].reached == 2 && g.table.pEntries[2].extra == 7 && g.table.pEntries[2].score == 4500);
	CHECK(g.hiScore == 10000);
	step((tInput){.fire = 1}, 3);                   // fire held from the end screen: nothing
	CHECK(g.edit.ubPos == 0);
	step(NONE, 1);
	step((tInput){.dy = -1}, 1);                    // A -> B
	step(NONE, 1);
	step((tInput){.fire = 1}, 1);                   // next letter (starts as B)
	step(NONE, 1);
	step((tInput){.dy = -1}, 1);                    // -> C
	step(NONE, 1);
	step((tInput){.fire = 1}, 1);
	step(NONE, 1);
	step((tInput){.dy = -1}, 1);                    // -> D
	step(NONE, 1);
	CHECK(g.state == GS_ENTRY && g.edit.ubPos == 2);
	step((tInput){.fire = 1}, 1);
	CHECK(g.state == GS_SCORES && g.scoresChanged);
	CHECK(g.table.pEntries[2].name[0] == 'B' && g.table.pEntries[2].name[1] == 'C' && g.table.pEntries[2].name[2] == 'D');
	// fire doesn't leave the table until it has been saved (and was let go first)
	step(NONE, 1);
	step((tInput){.fire = 1}, 1);
	CHECK(g.state == GS_SCORES);
	g.scoresChanged = 0;                            // (main.c saved it)
	step(NONE, 1);
	step((tInput){.fire = 1}, 1);
	CHECK(g.state == GS_TITLE && g.newRank == HI_COUNT);
	CHECK(g.table.pEntries[2].score == 4500);
}

static void testHallTimesOut(void) {
	// the table leaves by itself after SCORES_FRAMES
	endGameWith(4500, 0, GS_OVER);
	step(NONE, HI_EDIT_FRAMES + 2);                 // no input at all: the initials stand as they are
	CHECK(g.state == GS_SCORES && g.table.pEntries[2].name[0] == 'A' && g.table.pEntries[2].reached == 1);
	g.scoresChanged = 0;
	step(NONE, SCORES_FRAMES + 1);
	CHECK(g.state == GS_TITLE);
	// too low: straight to the title
	endGameWith(100, 0, GS_OVER);
	CHECK(g.state == GS_TITLE && g.table.pEntries[4].score == 500);
	// a new best raises the HI
	endGameWith(12345, 2, GS_OVER);
	CHECK(g.state == GS_ENTRY && g.newRank == 0 && g.hiScore == 12345 && hiTop(&g.table) == 12345);
}

// All three theatres cleared: reached ALL (the victory screen leads to the hall too)
static void testVictoryReachesAll(void) {
	endGameWith(15000, THEATRES - 1, GS_VICTORY);
	CHECK(g.state == GS_ENTRY && g.newRank == 0 && g.table.pEntries[0].reached == THEATRES + 1);
	CHECK(g.table.pEntries[1].score == 10000);
}

// GODMODE games never count: no entry, the table and the HI untouched
static void testGodmodeNeverCounts(void) {
	newTheatre(800, 300);
	logicSetDifficulty(&g, 0);
	g.score = 99999;
	g.state = GS_OVER;
	g.stateTimer = 1;
	step(NONE, 1);
	CHECK(g.state == GS_TITLE && g.table.pEntries[0].score == 10000 && g.hiScore == 10000);
	// ... and its score doesn't raise the live HI either (a game that counts does)
	for(uint8_t ubSkill = 0; ubSkill <= 3; ubSkill += 3) {
		newTheatre(30 * TILE + 24, 30 * TILE + 48);
		g.uwJetTimer = g.uwBomberTimer = g.uwBoatTimer = 60000;
		logicSetDifficulty(&g, ubSkill);
		g.hiScore = 500;
		g.pStructs[0].hp = 1;
		step((tInput){.fire2 = 1}, 1);
		step(NONE, 40);
		CHECK(g.score >= 1000);
		CHECK(g.hiScore == (ubSkill ? g.score : 500));
	}
}

// Game over: fire skips the screen after 100 frames of it
static void testGameOverFireSkips(void) {
	newTheatre(800, 300);
	g.uwJetTimer = g.uwBomberTimer = g.uwBoatTimer = 60000;
	g.score = 100;
	g.state = GS_OVER;
	g.stateTimer = OVER_FRAMES;
	step((tInput){.fire = 1}, 50);
	CHECK(g.state == GS_OVER);                      // too early
	step((tInput){.fire = 1}, 60);
	CHECK(g.state == GS_TITLE);
}

// The factories destroyed over a game are counted, from the start of a game
static void testFactoriesKilledCounted(void) {
	newTheatre(800, 300);
	g.uwJetTimer = g.uwBomberTimer = g.uwBoatTimer = 60000;
	g.killed = 5;
	logicStartGame(&g);
	CHECK(g.killed == 0);
	g.state = GS_PLAY;
	g.wantTheatre = 0xFF;
	g.chop.x = 30 * TILE + 24;
	g.chop.y = 30 * TILE + 48;
	g.chop.alt = ALT_CRUISE;
	g.pStructs[0].hp = BOMB_DAMAGE;
	step((tInput){.fire2 = 1}, 1);
	step(NONE, 40);
	CHECK(g.pStructs[0].hp == 0 && g.killed == 1);
}

// Tests only: Ctrl+L, E ends the game now
static void testTestEnd(void) {
	newTheatre(800, 300);
	g.score = 4500;
	CHECK(logicTestEnd(&g));
	step(NONE, 2);
	CHECK(g.state == GS_ENTRY && g.lives == 0);
	CHECK(!logicTestEnd(&g));                       // not in play: nothing
}

// ---- the theatre card (1.5) ----

// A new game from the title: the card first, once the theatre is loaded
static void startToTheCard(void) {
	newTheatre(800, 300);
	g.uwJetTimer = g.uwBomberTimer = g.uwBoatTimer = 60000;
	g.state = GS_TITLE;
	step(NONE, 1);
	step((tInput){.fire = 1}, 1);
	CHECK(g.state == GS_CARD && g.wantTheatre == 0);
	g.wantTheatre = 0xFF;                           // (main.c loads the theatre)
	logicLoadTheatre(&g, &s_sTh);
	g.state = GS_CARD;
}

// The card waits for the load, then runs its time; nothing else moves; then the play
static void testCardThenPlay(void) {
	startToTheCard();
	step(NONE, 1);
	uint16_t uwJet = g.uwJetTimer, uwAlert = g.uwAlertClock;
	int16_t cx = g.carrier.x;
	step(NONE, CARD_FRAMES - 2);
	CHECK(g.state == GS_CARD);
	CHECK(g.uwJetTimer == uwJet && g.uwAlertClock == uwAlert && g.carrier.x == cx);   // the world waits
	step(NONE, 1);
	CHECK(g.state == GS_CARD && mmCardWiping(&g.card) == CARD_WIPE);              // its time is up: the wipe (main.c clears the picture)
	step(NONE, CARD_WIPE);
	CHECK(g.state == GS_PLAY && g.msg == MSG_THEATRE && g.msgTimer > 200);       // then its name over the play
	CHECK(g.chop.landed);
	// a theatre still loading doesn't run the card down
	startToTheCard();
	g.wantTheatre = 1;
	step(NONE, CARD_FRAMES + 5);
	CHECK(g.state == GS_CARD);
}

// A fresh press of fire skips the card; fire held from the title does not
static void testCardFreshFireSkips(void) {
	startToTheCard();
	step((tInput){.fire = 1}, 40);                  // still held from the title
	CHECK(g.state == GS_CARD);
	step(NONE, 5);
	step((tInput){.fire = 1}, 1);
	CHECK(g.state == GS_CARD && mmCardWiping(&g.card));                          // skipped: the wipe
	step(NONE, CARD_WIPE);
	CHECK(g.state == GS_PLAY);
}

// After a theatre is cleared the next one's card comes before its play
static void testCardAfterClear(void) {
	newTheatre(800, 300);
	g.uwJetTimer = g.uwBomberTimer = g.uwBoatTimer = 60000;
	g.pStructs[0].hp = 0;
	g.factoriesLeft = 0;
	step(NONE, 1);
	CHECK(g.state == GS_CLEAR);
	step(NONE, CLEAR_FRAMES + 1);
	CHECK(g.theatre == 1 && g.wantTheatre == 1 && g.state == GS_CARD);
	g.wantTheatre = 0xFF;
	logicLoadTheatre(&g, &s_sTh);
	CHECK(g.card.ubMission == 2 && g.card.ubOf == THEATRES && g.card.uwLeft == CARD_FRAMES);
	step(NONE, CARD_FRAMES + CARD_WIPE);
	CHECK(g.state == GS_PLAY);
}

static tMmImage s_sCardImg;
static uint8_t s_pCardBuf[MM_BYTES(CARD_W, CARD_H, CARD_PLANES)] __attribute__((aligned(4)));

// The card's picture: land and coast from the tile map, the factory (red, then dark
// once destroyed), the carrier, the arrow over the gunship, the route's three nodes
static void testCardMap(void) {
	newTheatre(800, 300);
	g.theatre = 1;                                  // Ember Flats: Coral Reach is behind
	// an island: land 20..29 x 20..24, a coast tile at (19, 20)
	for(int ty = 20; ty < 25; ++ty)
		for(int tx = 20; tx < 30; ++tx) g.map.pTiles[ty * g.map.uwW + tx] = TILE_LAND0;
	g.map.pTiles[20 * g.map.uwW + 19] = 5;
	mmImageInit(&s_sCardImg, s_pCardBuf, CARD_W, CARD_H, CARD_PLANES);
	cardMapDraw(&s_sCardImg, &g);
	const int x0 = CARD_MAP_X + 1, y0 = CARD_MAP_Y + 1;
	CHECK(mmGet(&s_sCardImg, x0 + 25, y0 + 22) == CARD_PEN_LAND);
	CHECK(mmGet(&s_sCardImg, x0 + 19, y0 + 20) == CARD_PEN_COAST);
	CHECK(mmGet(&s_sCardImg, x0 + 10, y0 + 22) == CARD_PEN_SEA && mmGet(&s_sCardImg, x0 + 30, y0 + 22) == CARD_PEN_SEA);
	CHECK(mmGet(&s_sCardImg, CARD_MAP_X, CARD_MAP_Y) == CARD_PEN_COAST);          // the frame
	CHECK(mmGet(&s_sCardImg, CARD_MAP_X + 81, CARD_MAP_Y + 81) == CARD_PEN_COAST);   // (the 80 x 80 test map's)
	// the factory (3 x 3 tiles at 30, 30): red; destroyed: dark
	CHECK(mmGet(&s_sCardImg, x0 + 31, y0 + 31) == CARD_PEN_RED);
	g.pStructs[0].hp = 0;
	cardMapDraw(&s_sCardImg, &g);
	CHECK(mmGet(&s_sCardImg, x0 + 31, y0 + 31) == CARD_PEN_BLACK && mmGet(&s_sCardImg, x0 + 29, y0 + 31) != CARD_PEN_RED);
	// the carrier at tile (20, 60): a yellow bar, 6 tiles wide
	CHECK(mmGet(&s_sCardImg, x0 + 20, y0 + 60) == CARD_PEN_YELLOW && mmGet(&s_sCardImg, x0 + 17, y0 + 60) == CARD_PEN_YELLOW);
	CHECK(mmGet(&s_sCardImg, x0 + 23, y0 + 60) == CARD_PEN_SEA);
	// the gunship on the deck: the arrow's tip above the bar
	int cx = g.chop.x >> 4, cy = g.chop.y >> 4;
	CHECK(mmGet(&s_sCardImg, x0 + cx, y0 + cy - 3) == CARD_PEN_WHITE);
	// the route: Coral Reach done (a filled yellow node), this one current (a white ring, hollow
	// inside, an arrow), Frostline to come (a hollow node with a grey outline)
	CHECK(mmGet(&s_sCardImg, CARD_ROUTE_X + 6, CARD_ROUTE_Y + 6) == CARD_PEN_YELLOW);
	CHECK(mmGet(&s_sCardImg, CARD_ROUTE_X, CARD_ROUTE_Y + CARD_ROUTE_STEP) == CARD_PEN_WHITE);
	CHECK(mmGet(&s_sCardImg, CARD_ROUTE_X + 6, CARD_ROUTE_Y + CARD_ROUTE_STEP + 6) == 0);
	CHECK(mmGet(&s_sCardImg, CARD_ROUTE_X, CARD_ROUTE_Y + 2 * CARD_ROUTE_STEP) == CARD_PEN_LAND);
	CHECK(mmGet(&s_sCardImg, CARD_ROUTE_X + 6, CARD_ROUTE_Y + 2 * CARD_ROUTE_STEP + 6) == 0);
	// the first theatre: nothing done yet, the first node current
	g.theatre = 0;
	cardMapDraw(&s_sCardImg, &g);
	CHECK(mmGet(&s_sCardImg, CARD_ROUTE_X, CARD_ROUTE_Y) == CARD_PEN_WHITE);
	CHECK(mmGet(&s_sCardImg, CARD_ROUTE_X + 6, CARD_ROUTE_Y + 6) == 0);
}

static void testCardBrief(void) {
	char sz[48];
	newTheatre(800, 300);                           // one factory, Coral Reach
	cardBrief(&g, sz);
	CHECK(strcmp(sz, "1 FACTORIES  3 BOMBS EACH  CRUISER 3:00") == 0);
	g.theatre = 1;
	g.ubFactories = 6;
	cardBrief(&g, sz);
	CHECK(strcmp(sz, "6 FACTORIES  4 BOMBS EACH  CRUISER 2:30") == 0);
	g.theatre = 2;
	cardBrief(&g, sz);
	CHECK(strcmp(sz, "6 FACTORIES  5 BOMBS EACH  CRUISER 2:00") == 0);
	CHECK(strlen(sz) <= 40);
}

// ---- the attract loop and the demo (1.5) ----

// The real Coral Reach (disk/theatre1.bin), read fresh each time: the game changes its tiles
static uint8_t s_pReal[THEATRE_HEADER + MAP_W_MAX * MAP_H_MAX + 6 * 64 + 18 * BANDS_MAX];
static tTheatre s_sReal;

static void loadRealTheatre(void) {
	char sz[600];
	const char *szHere = __FILE__;               // .../tests/unit/test_logic.c
	size_t n = strlen(szHere);
	snprintf(sz, sizeof(sz), "%.*sdisk/theatre1.bin", (int)(n - strlen("tests/unit/test_logic.c")), szHere);
	FILE *f = fopen(sz, "rb");
	CHECK(f != 0);
	if(!f) return;
	size_t nRead = fread(s_pReal, 1, sizeof(s_pReal), f);
	fclose(f);
	CHECK(theatreParse(s_pReal, (uint32_t)nRead, &s_sReal));
	g.map = s_sReal.map;
}

// What main.c does when the rules ask for a theatre
static void mainLoadsTheatre(void) {
	loadRealTheatre();
	g.wantTheatre = 0xFF;
	logicLoadTheatre(&g, &s_sReal);
}

static void freshRealGame(void) {
	loadRealTheatre();
	logicInit(&g, s_sReal.map.pTiles, s_sReal.map.uwW, s_sReal.map.uwH);
	logicLoadTheatre(&g, &s_sReal);
	g.state = GS_TITLE;
	g.steer = STEER_ARCADE;                      // the player's own choices, to come back
	logicSetDifficulty(&g, 5);
}

// Left alone on the title: the hall of fame, then the demo (on normal, classic steering)
static void toTheDemo(void) {
	step(NONE, ATTRACT_IDLE - 1);
	CHECK(g.state == GS_TITLE);
	step(NONE, 1);
	CHECK(g.state == GS_SCORES && g.scoresAttract && g.newRank == HI_COUNT);
	step(NONE, SCORES_FRAMES - 1);
	CHECK(g.state == GS_SCORES);
	step(NONE, 1);
	CHECK(g.demo && g.state == GS_PLAY && g.wantTheatre == 0 && g.steer == STEER_CLASSIC && g.difficulty == 3);
	mainLoadsTheatre();
	CHECK(g.state == GS_PLAY && g.demoSteer == STEER_ARCADE && g.demoDifficulty == 5);
}

typedef struct { uint32_t frames; int16_t x, y; uint8_t damage, factories, lost; uint32_t score; uint16_t lfsr; int16_t xMid, yMid; } tDemoRun;

static void playTheDemo(tDemoRun *r) {
	memset(r, 0, sizeof(*r));
	while(g.demo && r->frames < 3000) {
		step(NONE, 1);
		++r->frames;
		if(!g.demo) break;
		if(r->frames == 300) { r->xMid = g.chop.x; r->yMid = g.chop.y; }
		r->x = g.chop.x;
		r->y = g.chop.y;
		r->damage = g.chop.damage;
		r->factories = g.factoriesLeft;
		r->score = g.score;
		r->lfsr = g.lfsr;
		if(g.state != GS_PLAY) r->lost = 1;
	}
}

// Title -> hall of fame -> the recorded flight on the real Coral Reach: it takes off, flies to the
// factory and bombs it down without being lost; then the title, the player's choices back,
// nothing counted, a fresh Coral Reach behind it
static void testAttractLoopAndDemo(void) {
	freshRealGame();
	g.hiScore = 500;                              // a HI the demo's 1,000 would beat if it counted
	toTheDemo();
	tDemoRun r;
	playTheDemo(&r);
	printf("demo: %u frames, factories left %d, score %u, damage %d, lost %d\n", (unsigned)r.frames, r.factories, (unsigned)r.score, r.damage, r.lost);
	CHECK(r.frames == demoFrames(g_pDemo, g_pDemo_RUNS) + 1);     // its inputs, then the frame that sees the end
	CHECK(r.factories == 5 && r.score >= 1000 && !r.lost && r.damage < DAMAGE_WARN);   // the factory fell, the gunship flew on
	CHECK(!g.demo && g.state == GS_TITLE);
	CHECK(g.steer == STEER_ARCADE && g.difficulty == 5 && g.score == 0 && g.lives == LIVES_START);
	CHECK(g.hiScore == 500 && g.table.pEntries[0].score == 10000 && g.newRank == HI_COUNT);   // nothing counted
	CHECK(g.wantTheatre == 0);                    // main.c loads Coral Reach fresh behind the title
	mainLoadsTheatre();
	CHECK(g.factoriesLeft == 6);
	// and round again: the title idles for another 12 s before the hall of fame
	step(NONE, ATTRACT_IDLE - 1);
	CHECK(g.state == GS_TITLE);
	step(NONE, 1);
	CHECK(g.state == GS_SCORES);
}

// The demo flies the same every time, whatever the game did before: the dice, the frame
// counter and the theatre are all set fresh
static void testDemoIsDeterministic(void) {
	freshRealGame();
	toTheDemo();
	tDemoRun a;
	playTheDemo(&a);
	mainLoadsTheatre();
	// a game in between that leaves things dirty: the dice, the frame, the enemies, the clocks
	g.lfsr = 0x1234;
	g.frame = 777;
	g.uwJetTimer = 5;
	g.uwBoatTimer = 7;
	g.ubAlertTime = 2;
	g.uwAlertClock = 4000;
	g.pAir[0].active = 1;
	g.pBoats[0].active = 1;
	g.pEShots[0].active = 1;
	g.chop.damage = 50;
	g.chop.bombs = 2;
	g.carrier.hp = 40;
	g.cruiser.sunk = 1;
	g.uwBuild = 5000;
	g.score = 4000;
	g.killed = 3;
	g.theatre = 2;
	step(NONE, ATTRACT_IDLE + SCORES_FRAMES);
	CHECK(g.demo);
	mainLoadsTheatre();
	tDemoRun b;
	playTheDemo(&b);
	CHECK(memcmp(&a, &b, sizeof(a)) == 0);
	CHECK(a.x == b.x && a.score == b.score && a.lfsr == b.lfsr && a.xMid == b.xMid);
}

// Fire ends the demo and the hall of fame (a press, after it was let go); to start a game it must be let go again
static void testFireEndsTheAttractPages(void) {
	freshRealGame();
	step(NONE, ATTRACT_IDLE);
	CHECK(g.state == GS_SCORES);
	step(NONE, 10);
	step((tInput){.fire = 1}, 1);
	CHECK(g.state == GS_TITLE && !g.demo);
	step((tInput){.fire = 1}, 30);                // still held: no game
	CHECK(g.state == GS_TITLE);
	step(NONE, ATTRACT_IDLE + SCORES_FRAMES);     // the demo
	CHECK(g.demo);
	mainLoadsTheatre();
	step(NONE, 60);
	CHECK(g.demo && g.chop.alt > 0);              // it's flying
	step((tInput){.fire = 1}, 1);
	CHECK(!g.demo && g.state == GS_TITLE && g.steer == STEER_ARCADE && g.difficulty == 5 && g.score == 0);
	step((tInput){.fire = 1}, 30);
	CHECK(g.state == GS_TITLE);                   // still held
	step(NONE, 1);
	step((tInput){.fire = 1}, 1);
	CHECK(g.state == GS_CARD && !g.demo);         // let go and pressed again: a game (its card first)
}

// Somebody there (the stick moved) keeps the title from going idle; GODMODE and the
// difficulty keys do nothing to a running demo
static void testTitleIdleAndDemoDifficulty(void) {
	freshRealGame();
	step(NONE, 500);
	step((tInput){.dx = 1}, 1);                   // the steering choice
	CHECK(g.steer == STEER_ARCADE);
	step(NONE, ATTRACT_IDLE - 1);
	CHECK(g.state == GS_TITLE);
	step(NONE, 1);
	CHECK(g.state == GS_SCORES);
	step(NONE, SCORES_FRAMES);
	mainLoadsTheatre();
	CHECK(g.demo);
	logicSetDifficulty(&g, 0);                    // Ctrl+L, 0 in the middle of a demo
	CHECK(g.difficulty == 3);
	step(NONE, 30);
	CHECK(g.demo);
}

int main(void) {
	testHeadingTable();
	testStickToHeading();
	testTurnShorterWay();
	testClassicRotatesAndThrusts();
	testSameSpeedEveryWay();
	testClassicReverses();
	testClassicGlidesThenRests();
	testClassicTurnWhileGliding();
	testClassicFullSpeedIsExact();
	testArcadeTurnsTowardStickAndFlies();
	testWorldEdgesHold();
	testCameraFollowsWithinMapAndLimitsStep();
	testTileAt();
	testScrollMaths();
	testDeterministic();
	testCarrierPatrolsItsLane();
	testCarrierTurnsAtLaneEnd();
	testLandsWhenSlowOverDeck();
	testNoLandingAtSea();
	testNoLandingWhileMoving();
	testDeckRepairsAndReloads();
	testTakeOffClimbs();
	testArcadeLandsToo();
	testTheatreParse();
	testLoadTheatre();
	testStartsOnDeck();
	testCannonFiresAndFalls();
	testCannonKillsTurret();
	testReticleIsWhereTheBombLands();
	testThreeBombsDestroyAFactory();
	testBlastReachEdges();
	testDamagedFactoryRepairs();
	testOneButtonHoldDropsABomb();
	testTwoButtonsAutofire();
	testNoBombsOnDeckOrEmpty();
	testHeadingTo();
	testTurretFiresFlakInRange();
	testTurretGunAimsAndFlashes();
	testAaTellTable();
	testNoShotWithoutATell();
	testTellHoldsFireAndCantBeCut();
	testFlakSpeed();
	testSfxPanning();
	testThreatCounts();
	testTurretsSortedAndAllDrawn();
	testFlakHurtsAndGodmodeShrugs();
	testCrashRespawnAndGameOver();
	testMissilesFromAlertThree();
	testJetHuntsAndCannonKillsIt();
	testBomberHitsTheCarrier();
	testCarrierSunkMeansAirstrip();
	testBoatDeliversAndRepairs();
	testCruiserSailsAndShipyardSetsBack();
	testCruiserEscalatesAndRewards();
	testJetsDoNotCampTheDeck();
	testOneJetAtATimeNearTheCarrier();
	testJetTellTable();
	testNoJetMissileWithoutATell();
	testJetTellAborts();
	testClearTheatreThenNext();
	testTitleStartsAGame();
	testTestJumpOnlyInGodmode();
	testLaterTheatresAreHarder();
	testHomePointer();
	testReloadAndRepairPrompts();
	testHallDefaults();
	testScoreMakesTheHallOfFame();
	testHallTimesOut();
	testVictoryReachesAll();
	testGodmodeNeverCounts();
	testGameOverFireSkips();
	testFactoriesKilledCounted();
	testTestEnd();
	testCardThenPlay();
	testCardFreshFireSkips();
	testCardAfterClear();
	testCardMap();
	testCardBrief();
	testAttractLoopAndDemo();
	testDemoIsDeterministic();
	testFireEndsTheAttractPages();
	testTitleIdleAndDemoDifficulty();
	if(s_failures) {
		printf("%d check(s) failed\n", s_failures);
		return 1;
	}
	printf("all logic tests passed\n");
	return 0;
}
