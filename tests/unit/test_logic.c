// Host-side tests for src/logic.c (ROTOR RAIDER). Run with: agk unit
#include <stdio.h>
#include "logic.h"

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
	step((tInput){.dy = 1}, 30);                      // brake
	CHECK(g.chop.vx == 0);
	CHECK(g.chop.x > x);                              // it coasted a bit first
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

static void testClassicDriftsToAStop(void) {
	newGame(STEER_CLASSIC, 1000, 1000);
	g.chop.heading = 8;
	step((tInput){.dy = -1}, 40);
	CHECK(g.chop.vy > 30);
	step(NONE, 60);
	CHECK(g.chop.vy == 0 && g.chop.vx == 0);
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
	step(NONE, 1);
	CHECK(g.events & EV_FLAK);
	CHECK(g.pEShots[0].active && g.pEShots[0].kind == ESHOT_FLAK);
	turretGame();
	g.chop.x = 1200;                              // out of range
	step(NONE, 5);
	CHECK(!g.pEShots[0].active);
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

static void testClearTheatreThenNext(void) {
	newTheatre(800, 300);
	g.uwJetTimer = g.uwBomberTimer = g.uwBoatTimer = 60000;
	g.pStructs[0].hp = 0;
	g.factoriesLeft = 0;
	step(NONE, 1);
	CHECK(g.state == GS_CLEAR);
	step(NONE, CLEAR_FRAMES + 1);
	CHECK(g.theatre == 1 && g.wantTheatre == 1 && g.state == GS_PLAY);
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
	CHECK(g.state == GS_PLAY && g.wantTheatre == 0 && g.score == 0 && g.lives == LIVES_START);
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

int main(void) {
	testHeadingTable();
	testStickToHeading();
	testTurnShorterWay();
	testClassicRotatesAndThrusts();
	testSameSpeedEveryWay();
	testClassicDriftsToAStop();
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
	testDamagedFactoryRepairs();
	testOneButtonHoldDropsABomb();
	testTwoButtonsAutofire();
	testNoBombsOnDeckOrEmpty();
	testHeadingTo();
	testTurretFiresFlakInRange();
	testFlakHurtsAndGodmodeShrugs();
	testCrashRespawnAndGameOver();
	testMissilesFromAlertThree();
	testJetHuntsAndCannonKillsIt();
	testBomberHitsTheCarrier();
	testCarrierSunkMeansAirstrip();
	testBoatDeliversAndRepairs();
	testCruiserSailsAndShipyardSetsBack();
	testClearTheatreThenNext();
	testTitleStartsAGame();
	testTestJumpOnlyInGodmode();
	if(s_failures) {
		printf("%d check(s) failed\n", s_failures);
		return 1;
	}
	printf("all logic tests passed\n");
	return 0;
}
