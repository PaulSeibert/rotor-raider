// ROTOR RAIDER - the Iron Combine fights back (portable C, unit-tested).
// (c) 2026 Desert Palms Software. Game by Paul Ross Seibert.
//
// AA turrets (flak, then homing missiles), interceptor jets (vectored by
// radar), bombers after the carrier, supply boats for the factories, and the
// shipyard's cruiser. How hard they press depends on the alert level (factories
// destroyed, time) and the difficulty.

#include "logic.h"

// Difficulty 0..5 (0 invincible: plays like 3). Tables, no maths.
static const int8_t s_pTurretCool[6] = {0, 60, 30, 0, -20, -35};
static const int16_t s_pJetExtra[6] = {0, 700, 350, 0, -250, -400};
static const int16_t s_pBomberExtra[6] = {0, 900, 450, 0, -350, -600};
static const uint8_t s_pFlakDamage[6] = {FLAK_DAMAGE, 4, 6, FLAK_DAMAGE, 10, 12};
static const uint8_t s_pAimMask[6] = {31, 63, 47, 31, 15, 7};
// the AA guns' tell (frames, a multiple of 4: a gun is looked at every 4th frame)
static const uint8_t s_pTurretTell[6] = {TURRET_TELL_NORMAL, 24, 20, TURRET_TELL_NORMAL, 12, 8};
// the alert level's effect (alert * 100, * 150, * 50: tables, no multiplies)
static const int16_t s_pAlertJet[6] = {0, 100, 200, 300, 400, 500};
static const int16_t s_pAlertBomber[6] = {0, 150, 300, 450, 600, 750};
static const int16_t s_pAlertBoat[6] = {0, 50, 100, 150, 200, 250};
static const int16_t s_pAlertCool[6] = {0, 8, 16, 24, 32, 40};
// each theatre presses harder: Coral Reach, Ember Flats, Frostline
static const int8_t s_pThTurretCool[THEATRES] = {0, -35, -60};
static const int16_t s_pThJet[THEATRES] = {0, -300, -500};
static const int16_t s_pThBomber[THEATRES] = {0, -300, -600};
static const int16_t s_pThBoat[THEATRES] = {0, -100, -200};
static inline uint8_t thIx(const tGame *g) { return g->theatre < THEATRES ? g->theatre : THEATRES - 1; }

static inline int16_t absw(int16_t v) { return v < 0 ? (int16_t)-v : v; }

// a random 0..n-1 (n small), without a divide
static uint8_t pick(tGame *g, uint8_t n) {
	uint8_t r = (uint8_t)logicRandom(g);
	while(r >= n) r = (uint8_t)(r - n);
	return r;
}

// d / 3 (d up to a few hundred), without a divide
static inline uint8_t third(int16_t d) {
	return (uint8_t)(((d < 0 ? 0 : d) * 85) >> 8);
}

// the frames flak takes to fly d px (up to a few hundred), at most 250
static inline uint8_t flakFrames(int16_t d) {
	int16_t f = divw(mulw(d < 0 ? 0 : d, 16), FLAK_SPEED);
	return (uint8_t)(f > 250 ? 250 : f);
}

uint8_t logicTurretTell(const tGame *g) { return s_pTurretTell[g->difficulty <= 5 ? g->difficulty : 3]; }

static inline void subStep(int16_t *pPos, int16_t v, uint8_t *pFrac) {
	int16_t f = (int16_t)(*pFrac + v);
	*pPos = (int16_t)(*pPos + (f >> 4));
	*pFrac = (uint8_t)(f & 15);
}

// a rough distance: the longer side plus half the shorter
static inline int16_t dist(int16_t dx, int16_t dy) {
	int16_t ax = absw(dx), ay = absw(dy);
	return ax > ay ? (int16_t)(ax + (ay >> 1)) : (int16_t)(ay + (ax >> 1));
}

int16_t logicDist(int16_t dx, int16_t dy) { return dist(dx, dy); }

uint8_t logicHeadingTo(int16_t dx, int16_t dy) {
	int16_t ax = absw(dx), ay = absw(dy);
	// steps of 22.5 degrees from north: tan 11.25 ~ 1/5, 33.75 ~ 2/3, 56.25 ~ 3/2, 78.75 ~ 5
	uint8_t k = (int16_t)(ax * 5) < ay ? 0 : (int16_t)(ax * 3) < (int16_t)(ay * 2) ? 1 :
		(int16_t)(ax * 2) < (int16_t)(ay * 3) ? 2 : ax < (int16_t)(ay * 5) ? 3 : 4;
	if(dx >= 0) return dy < 0 ? k : (uint8_t)(8 - k);
	return dy >= 0 ? (uint8_t)(8 + k) : (uint8_t)((16 - k) & 15);
}

static inline void headingVel(uint8_t h, int16_t wSpeed, int16_t *pVx, int16_t *pVy) {
	*pVx = (int16_t)((mulw(g_pHeadX[h], wSpeed) + 32) >> 6);
	*pVy = (int16_t)((mulw(g_pHeadY[h], wSpeed) + 32) >> 6);
}

static inline uint8_t turned(uint8_t h, uint8_t ubTo) {
	return (uint8_t)((h + logicTurnToward(h, ubTo)) & 15);
}

uint8_t logicAlert(const tGame *g) {
	uint8_t a = (uint8_t)(g->ubFactories - g->factoriesLeft + g->ubAlertTime);
	return a > 5 ? 5 : a;
}

static uint8_t radarsUp(const tGame *g) {
	uint8_t n = 0;
	for(uint8_t i = 0; i < g->ubStructs; ++i) n = (uint8_t)(n + (g->pStructs[i].type == OBJ_RADAR && g->pStructs[i].hp));
	return n;
}

static void boom(tGame *g, int16_t x, int16_t y, uint8_t isBig) {
	tBoom *b = &g->pBooms[0];
	for(uint8_t i = 0; i < BOOM_MAX; ++i) {
		if(!g->pBooms[i].active) { b = &g->pBooms[i]; break; }
	}
	b->active = 1;
	b->x = x;
	b->y = y;
	b->frame = 0;
	b->timer = 0;
	b->big = isBig;
	logicEventAt(g, isBig ? EV_BIGBOOM : EV_BOOM, x);
}

static void score(tGame *g, uint16_t uwPts) {
	g->score += uwPts;
	if(g->score > g->hiScore && g->difficulty) g->hiScore = g->score;
}

void enemiesReset(tGame *g) {
	for(uint8_t i = 0; i < ESHOT_MAX; ++i) g->pEShots[i].active = 0;
	for(uint8_t i = 0; i < AIR_MAX; ++i) g->pAir[i].active = 0;
	for(uint8_t i = 0; i < BOAT_MAX; ++i) g->pBoats[i].active = 0;
	g->cruiser.active = 0;
	g->cruiser.sunk = 0;
	g->uwBuild = 0;
	g->uwJetTimer = 1200;          // a quiet start
	g->uwBomberTimer = 2400;
	g->uwBoatTimer = 400;
	g->uwAlertClock = 0;
	g->ubAlertTime = 0;
}

// --------------------------------------------------------------- shots ---

static tEShot *eshotNew(tGame *g) {
	for(uint8_t i = 0; i < ESHOT_MAX; ++i) if(!g->pEShots[i].active) return &g->pEShots[i];
	return 0;
}

// Flak: to where the gunship will be when it gets there (give or take)
static void flakFire(tGame *g, int16_t x, int16_t y) {
	tEShot *s = eshotNew(g);
	if(!s) return;
	const tChopper *c = &g->chop;
	int16_t d = dist((int16_t)(c->x - x), (int16_t)(c->y - y));
	uint8_t ubFrames = (uint8_t)(flakFrames(d) + 1);
	uint8_t m = s_pAimMask[g->difficulty];
	int16_t tx = (int16_t)(c->x + ((c->vx * ubFrames) >> 4) + (int16_t)(logicRandom(g) & m) - (m >> 1));
	int16_t ty = (int16_t)(c->y + ((c->vy * ubFrames) >> 4) + (int16_t)(logicRandom(g) & m) - (m >> 1));
	s->active = 1;
	s->kind = ESHOT_FLAK;
	s->x = x;
	s->y = y;
	s->fx = s->fy = 0;
	headingVel(logicHeadingTo((int16_t)(tx - x), (int16_t)(ty - y)), FLAK_SPEED, &s->vx, &s->vy);
	s->life = (uint8_t)(flakFrames(dist((int16_t)(tx - x), (int16_t)(ty - y))) + 1);
	logicEventAt(g, EV_FLAK, x);
	++g->uwShotsFired;
}

static void missileFire(tGame *g, int16_t x, int16_t y) {
	tEShot *s = eshotNew(g);
	if(!s) return;
	s->active = 1;
	s->kind = ESHOT_MISSILE;
	s->x = x;
	s->y = y;
	s->fx = s->fy = 0;
	s->heading = logicHeadingTo((int16_t)(g->chop.x - x), (int16_t)(g->chop.y - y));
	headingVel(s->heading, MISSILE_SPEED, &s->vx, &s->vy);
	s->life = MISSILE_LIFE;
	logicEventAt(g, EV_MISSILE, x);
	++g->uwShotsFired;
}

static void shellFire(tGame *g, int16_t x, int16_t y) {
	tEShot *s = eshotNew(g);
	if(!s) return;
	s->active = 1;
	s->kind = ESHOT_SHELL;
	s->x = x;
	s->y = y;
	s->fx = s->fy = 0;
	int16_t tx = g->carrier.x, ty = g->carrier.y;
	headingVel(logicHeadingTo((int16_t)(tx - x), (int16_t)(ty - y)), SHELL_SPEED, &s->vx, &s->vy);
	s->life = (uint8_t)(third(dist((int16_t)(tx - x), (int16_t)(ty - y))) + 1);
	logicEventAt(g, EV_FLAK, x);
}

static void carrierHit(tGame *g, uint8_t ubDamage) {
	tCarrier *k = &g->carrier;
	if(!k->hp) return;
	k->hp = k->hp > ubDamage ? (uint8_t)(k->hp - ubDamage) : 0;
	logicEventAt(g, EV_CARRIER, k->x);
	boom(g, (int16_t)(k->x - 20 + (int16_t)(logicRandom(g) & 31)), k->y, 1);
	if(!k->hp) {
		logicEventAt(g, EV_SUNK, k->x);
		if(g->chop.landed && g->state == GS_PLAY) logicHitChopper(g, 255);
	}
}

static void eshotsUpdate(tGame *g) {
	tChopper *c = &g->chop;
	for(uint8_t i = 0; i < ESHOT_MAX; ++i) {
		tEShot *s = &g->pEShots[i];
		if(!s->active) continue;
		if(s->kind == ESHOT_MISSILE && !(s->life & 3)) {
			s->heading = turned(s->heading, logicHeadingTo((int16_t)(c->x - s->x), (int16_t)(c->y - s->y)));
			headingVel(s->heading, MISSILE_SPEED, &s->vx, &s->vy);
		}
		subStep(&s->x, s->vx, &s->fx);
		subStep(&s->y, s->vy, &s->fy);
		if(s->kind == ESHOT_MISSILE) {
			if(g->state == GS_PLAY && !c->landed && dist((int16_t)(c->x - s->x), (int16_t)(c->y - s->y)) < 9) {
				s->active = 0;
				boom(g, s->x, s->y, 0);
				logicHitChopper(g, MISSILE_DAMAGE);
				continue;
			}
		}
		if(--s->life) continue;
		s->active = 0;
		boom(g, s->x, s->y, 0);
		if(s->kind == ESHOT_FLAK) {
			if(g->state == GS_PLAY && !c->landed && dist((int16_t)(c->x - s->x), (int16_t)(c->y - s->y)) < FLAK_R) {
				logicHitChopper(g, s_pFlakDamage[g->difficulty]);
			}
		}
		else if(s->kind == ESHOT_SHELL) carrierHit(g, CRUISER_DAMAGE);
	}
}

// ------------------------------------------------------------- turrets ---

// A quarter of the turrets a frame (each one every 4 frames: its cooldown
// counts 4 at a time). Every round has a tell: when the cooldown ends with the
// gunship in range, the gun holds its aim and its barrels glow for
// logicTurretTell frames (TURRET_TELL), then it fires (TURRET_FLASH). If the
// gunship is out of range or on deck by then, it holds its fire.
static void turretsUpdate(tGame *g) {
	const tChopper *c = &g->chop;
	uint8_t isTarget = g->state == GS_PLAY && !c->landed && c->alt;
	uint8_t isMissile = logicAlert(g) >= logicMissileAlert(g);
	for(uint8_t i = (uint8_t)(g->frame & 3); i < g->ubStructs; i = (uint8_t)(i + 4)) {
		tStruct *s = &g->pStructs[i];
		if(s->type != OBJ_TURRET || !s->hp) continue;
		s->link &= 7 | TURRET_TELL;         // a muzzle flash lasts one turn (4 frames)
		uint8_t isTell = s->link & TURRET_TELL;
		int16_t x = (int16_t)(s->tx * TILE + 8), y = (int16_t)(s->ty * TILE + 8);
		int16_t d = 0x7FFF;
		if(isTarget) {
			int16_t dx = (int16_t)(c->x - x), dy = (int16_t)(c->y - y);
			d = dist(dx, dy);
			if(d <= TURRET_TRACK && !isTell) {   // the gun turns toward the gunship, a step a turn
				uint8_t ubTurn = (uint8_t)((FRAME8(logicHeadingTo(dx, dy)) - s->link) & 7);
				if(ubTurn) s->link = (uint8_t)((s->link + (ubTurn <= 4 ? 1 : 7)) & 7);
			}
		}
		if(s->timer) { s->timer = s->timer > 4 ? (uint16_t)(s->timer - 4) : 0; continue; }
		uint8_t ubTell = logicTurretTell(g);
		if(!isTell) {
			if(d > TURRET_RANGE) continue;
			s->link |= TURRET_TELL;             // wind up: fire ubTell frames from now
			s->timer = (uint16_t)(ubTell - 4);
			continue;
		}
		s->link &= 7;
		if(d > TURRET_RANGE) {                  // it got away (or landed): hold fire
			s->timer = 32;
			continue;
		}
		if(isMissile && !(logicRandom(g) & 3)) missileFire(g, x, y);
		else flakFire(g, x, y);
		s->link |= TURRET_FLASH;
		// the cooldown includes the tell, so a gun fires as often as before 1.4
		int16_t wCool = (int16_t)(TURRET_COOL - ubTell + s_pTurretCool[g->difficulty] + s_pThTurretCool[thIx(g)] -
			s_pAlertCool[logicAlert(g)] + (logicRandom(g) & 31));
		s->timer = (uint16_t)(wCool < 30 - ubTell ? 30 - ubTell : wCool);   // (a round every 30 frames at most)
	}
}

// ---------------------------------------------------------------- air ---

static tAir *airNew(tGame *g) {
	for(uint8_t i = 0; i < AIR_MAX; ++i) if(!g->pAir[i].active) return &g->pAir[i];
	return 0;
}

// a point at the edge of the map, from the theatre's spawns
static void spawnPoint(tGame *g, int16_t *pX, int16_t *pY) {
	if(!g->ubSpawns) {
		*pX = 16;
		*pY = 16;
		return;
	}
	uint8_t i = pick(g, g->ubSpawns);
	*pX = (int16_t)(g->pSpawnX[i] * TILE + 8);
	*pY = (int16_t)(g->pSpawnY[i] * TILE + 8);
}

static void airSpawnAt(tGame *g, uint8_t ubKind, int16_t x, int16_t y) {
	tAir *a = airNew(g);
	if(!a) return;
	a->active = 1;
	a->kind = ubKind;
	a->x = x;
	a->y = y;
	a->fx = a->fy = 0;
	int16_t tx = ubKind == AIR_JET ? g->chop.x : g->carrier.x, ty = ubKind == AIR_JET ? g->chop.y : g->carrier.y;
	a->heading = logicHeadingTo((int16_t)(tx - a->x), (int16_t)(ty - a->y));
	a->hp = ubKind == AIR_JET ? JET_HP : BOMBER_HP;
	a->timer = 60;
	a->done = 0;
	a->uwLife = 1500;
}

static void airSpawn(tGame *g, uint8_t ubKind) {
	int16_t x, y;
	spawnPoint(g, &x, &y);
	airSpawnAt(g, ubKind, x, y);
}

static uint8_t offMap(const tGame *g, int16_t x, int16_t y) {
	return x < -32 || y < -32 || x > (int16_t)(g->map.uwW * TILE + 32) || y > (int16_t)(g->map.uwH * TILE + 32);
}

static void airUpdate(tGame *g, uint8_t ubRadars) {
	tChopper *c = &g->chop;
	for(uint8_t i = 0; i < AIR_MAX; ++i) {
		tAir *a = &g->pAir[i];
		if(!a->active) continue;
		int16_t tx, ty;
		uint8_t ubTurnMask;     // turns when (frame & mask) == 0
		if(a->kind == AIR_JET) {
			if(a->uwLife) --a->uwLife;
			if(!a->uwLife || g->state != GS_PLAY) a->done = 1;
			// radar: straight at the gunship; without, it searches
			tx = c->x;
			ty = c->y;
			if(!ubRadars) {
				tx = (int16_t)(tx + ((int16_t)(a->uwLife & 0x1FF) - 256));
				ty = (int16_t)(ty + ((int16_t)((a->uwLife >> 2) & 0x1FF) - 256));
			}
			ubTurnMask = ubRadars ? 3 : 7;
		}
		else {
			tx = g->carrier.x;
			ty = g->carrier.y;
			ubTurnMask = 7;
			if(!g->carrier.hp) a->done = 1;
		}
		if(a->done) {
			// away: straight on, off the map
			tx = (int16_t)(a->x + g_pHeadX[a->heading] * 4);
			ty = (int16_t)(a->y + g_pHeadY[a->heading] * 4);
		}
		if(!(g->frame & ubTurnMask)) a->heading = turned(a->heading, logicHeadingTo((int16_t)(tx - a->x), (int16_t)(ty - a->y)));
		headingVel(a->heading, a->kind == AIR_JET ? JET_SPEED : BOMBER_SPEED, &a->vx, &a->vy);
		subStep(&a->x, a->vx, &a->fx);
		subStep(&a->y, a->vy, &a->fy);
		if(a->done && offMap(g, a->x, a->y)) {
			a->active = 0;
			continue;
		}
		if(a->timer) --a->timer;
		int16_t dx = (int16_t)(c->x - a->x), dy = (int16_t)(c->y - a->y);
		if(a->kind == AIR_JET) {
			if(g->state == GS_PLAY && !c->landed && c->alt > 12 && dist(dx, dy) < 14) {
				// a collision
				a->active = 0;
				boom(g, a->x, a->y, 1);
				logicHitChopper(g, 30);
				continue;
			}
			if(!a->done && !a->timer && g->state == GS_PLAY && !c->landed && dist(dx, dy) < 110 &&
				((logicHeadingTo(dx, dy) - a->heading + 1) & 15) <= 2) {
				missileFire(g, a->x, a->y);
				a->timer = 90;
			}
		}
		else if(!a->done && dist((int16_t)(g->carrier.x - a->x), (int16_t)(g->carrier.y - a->y)) < 14) {
			carrierHit(g, BOMBER_DAMAGE);
			a->done = 1;
		}
	}
}

// --------------------------------------------------------------- boats ---

// A supply boat comes in from the open sea straight along a pier's axis
static void boatSpawn(tGame *g) {
	tBoat *b = 0;
	for(uint8_t i = 0; i < BOAT_MAX; ++i) if(!g->pBoats[i].active) { b = &g->pBoats[i]; break; }
	if(!b) return;
	// a pier whose factory still stands
	uint8_t ubPiers = 0, pPiers[8];
	for(uint8_t i = 0; i < g->ubStructs && ubPiers < 8; ++i) {
		const tStruct *s = &g->pStructs[i];
		if(s->type != OBJ_PIER) continue;
		// its factory: the link-th factory
		uint8_t n = 0;
		for(uint8_t k = 0; k < g->ubStructs; ++k) {
			const tStruct *f = &g->pStructs[k];
			if(f->type != OBJ_FACTORY) continue;
			if(n++ == s->link) {
				if(f->hp) pPiers[ubPiers++] = i;
				break;
			}
		}
	}
	if(!ubPiers) return;
	uint8_t p = pPiers[pick(g, ubPiers)];
	const tStruct *s = &g->pStructs[p];
	int8_t dx = (int8_t)s->stage, dy = (int8_t)(s->timer & 0xFF);   // (a pier keeps its way out to sea there)
	b->active = 1;
	b->pier = p;
	b->leaving = 0;
	b->hp = BOAT_HP;
	b->fx = b->fy = 0;
	int16_t px = (int16_t)(s->tx * TILE + 8), py = (int16_t)(s->ty * TILE + 8);
	int16_t x = px, y = py;
	for(uint8_t k = 0; k < 40 && !offMap(g, (int16_t)(x + dx * 64), (int16_t)(y + dy * 64)); ++k) {
		x = (int16_t)(x + dx * TILE);
		y = (int16_t)(y + dy * TILE);
	}
	b->x = x;
	b->y = y;
	b->heading = logicHeadingTo((int16_t)(px - x), (int16_t)(py - y));
}

static tStruct *pierFactory(tGame *g, const tStruct *pPier) {
	uint8_t n = 0;
	for(uint8_t k = 0; k < g->ubStructs; ++k) {
		tStruct *f = &g->pStructs[k];
		if(f->type == OBJ_FACTORY && n++ == pPier->link) return f;
	}
	return 0;
}

void logicStructRepair(tGame *g, tStruct *s, uint8_t ubHp);

static void boatsUpdate(tGame *g) {
	for(uint8_t i = 0; i < BOAT_MAX; ++i) {
		tBoat *b = &g->pBoats[i];
		if(!b->active) continue;
		const tStruct *p = &g->pStructs[b->pier];
		int16_t px = (int16_t)(p->tx * TILE + 8), py = (int16_t)(p->ty * TILE + 8);
		if(b->leaving) {
			// back out to sea the way it came
			px = (int16_t)(b->x + g_pHeadX[b->heading] * 4);
			py = (int16_t)(b->y + g_pHeadY[b->heading] * 4);
		}
		if(!(g->frame & 15)) b->heading = logicHeadingTo((int16_t)(px - b->x), (int16_t)(py - b->y));
		headingVel(b->heading, BOAT_SPEED, &b->vx, &b->vy);
		subStep(&b->x, b->vx, &b->fx);
		subStep(&b->y, b->vy, &b->fy);
		if(b->leaving) {
			if(offMap(g, b->x, b->y)) b->active = 0;
			continue;
		}
		if(dist((int16_t)(px - b->x), (int16_t)(py - b->y)) < 10) {
			tStruct *f = pierFactory(g, p);
			if(f && f->hp) logicStructRepair(g, f, BOAT_REPAIR);
			b->leaving = 1;
			b->heading = (uint8_t)((b->heading + 8) & 15);
		}
	}
}

// ------------------------------------------------------------- cruiser ---

static void cruiserUpdate(tGame *g) {
	tCruiser *k = &g->cruiser;
	if(k->sunk) return;
	if(!k->active) {
		if(g->state != GS_PLAY || !g->carrier.hp) return;
		if(++g->uwBuild < logicCruiserBuild(g)) return;
		// it sails in along the carrier's lane, from the far side
		k->active = 1;
		k->hp = CRUISER_HP;
		k->y = g->carrier.y;
		int16_t wMid = (int16_t)(g->map.uwW * TILE / 2);
		k->dir = g->carrier.x < wMid ? -1 : 1;
		k->dir = (int8_t)-k->dir;   // (toward the carrier)
		k->x = k->dir > 0 ? (int16_t)(g->carrier.xMin - 200) : (int16_t)(g->carrier.xMax + 200);
		if(k->x < 48) k->x = 48;
		if(k->x > (int16_t)(g->map.uwW * TILE - 48)) k->x = (int16_t)(g->map.uwW * TILE - 48);
		k->dir = k->x < g->carrier.x ? 1 : -1;
		k->fx = 0;
		k->timer = 60;
		k->ubLevel = 0;
		k->uwStall = 0;
		logicEventAt(g, EV_CRUISER, k->x);
		return;
	}
	// the anti-stall clock: ignore it and it presses harder
	if(g->state == GS_PLAY) {
		if(k->uwStall < 60000) ++k->uwStall;
		if(k->ubLevel < 2 && k->uwStall >= (k->ubLevel ? CRUISER_STALL2 : CRUISER_STALL1)) {
			++k->ubLevel;
			logicEventAt(g, EV_CRUISER_UP, k->x);
		}
		if(k->ubLevel == 2 && k->uwStall >= CRUISER_STALL2) {
			uint16_t t = (uint16_t)(k->uwStall - CRUISER_STALL2);
			while(t >= CRUISER_JETS) t = (uint16_t)(t - CRUISER_JETS);
			if(!t) airSpawnAt(g, AIR_JET, k->x, k->y);   // off its deck, at the gunship
		}
	}
	int16_t dx = (int16_t)(g->carrier.x - k->x);
	if(absw(dx) > CRUISER_RANGE) {
		k->dir = dx > 0 ? 1 : -1;
		k->fx = (uint8_t)(k->fx + (k->ubLevel ? 16 : 8));
		if(k->fx >= 16) {
			k->fx = 0;
			k->x = (int16_t)(k->x + k->dir);
		}
	}
	else if(g->carrier.hp) {
		if(k->timer) --k->timer;
		else {
			shellFire(g, (int16_t)(k->x + k->dir * 30), k->y);
			k->timer = k->ubLevel ? 60 : 120;
		}
	}
}

static void cruiserHit(tGame *g, uint8_t ubDamage) {
	tCruiser *k = &g->cruiser;
	if(!k->active) return;
	k->hp = k->hp > ubDamage ? (uint8_t)(k->hp - ubDamage) : 0;
	k->uwStall = 0;                    // progress: the anti-stall clock starts again
	boom(g, k->x, k->y, ubDamage > 1);
	if(!k->hp) {
		k->active = 0;
		k->sunk = 1;
		boom(g, (int16_t)(k->x - 24), k->y, 1);
		boom(g, (int16_t)(k->x + 24), k->y, 1);
		score(g, CRUISER_SCORE);
		if(g->carrier.hp) g->carrier.hp = CARRIER_HP;   // the reward: the carrier repaired
		logicEventAt(g, EV_CRUISER_SUNK, k->x);
	}
}

// ---------------------------------------------------- the player's shots ---

uint8_t enemiesShot(tGame *g, int16_t x, int16_t y, uint8_t ubAlt) {
	if(ubAlt >= 8) {
		for(uint8_t i = 0; i < AIR_MAX; ++i) {
			tAir *a = &g->pAir[i];
			if(!a->active || absw((int16_t)(a->x - x)) > 10 || absw((int16_t)(a->y - y)) > 10) continue;
			boom(g, x, y, 0);
			if(!--a->hp) {
				a->active = 0;
				boom(g, a->x, a->y, 1);
				score(g, a->kind == AIR_JET ? 200 : 300);
			}
			return 1;
		}
	}
	if(ubAlt <= CANNON_HIT_ALT) {
		int16_t gx = (int16_t)(x + (ubAlt >> 1)), gy = (int16_t)(y + ubAlt);
		for(uint8_t i = 0; i < BOAT_MAX; ++i) {
			tBoat *b = &g->pBoats[i];
			if(!b->active || absw((int16_t)(b->x - gx)) > 10 || absw((int16_t)(b->y - gy)) > 10) continue;
			boom(g, gx, gy, 0);
			if(!--b->hp) {
				b->active = 0;
				boom(g, b->x, b->y, 1);
				score(g, 150);
			}
			return 1;
		}
		tCruiser *k = &g->cruiser;
		if(k->active && absw((int16_t)(k->x - gx)) < 44 && absw((int16_t)(k->y - gy)) < 12) {
			cruiserHit(g, 1);
			return 1;
		}
	}
	return 0;
}

void enemiesBlast(tGame *g, int16_t x, int16_t y) {
	for(uint8_t i = 0; i < BOAT_MAX; ++i) {
		tBoat *b = &g->pBoats[i];
		if(b->active && absw((int16_t)(b->x - x)) < 18 && absw((int16_t)(b->y - y)) < 18) {
			b->active = 0;
			score(g, 150);
		}
	}
	tCruiser *k = &g->cruiser;
	if(k->active && absw((int16_t)(k->x - x)) < 48 + BLAST_R && absw((int16_t)(k->y - y)) < 16 + BLAST_R) {
		cruiserHit(g, BOMB_DAMAGE);
	}
}

// --------------------------------------------------------------- pacing ---

static inline uint8_t inView(const tGame *g, int16_t x, int16_t y) {
	int16_t sx = (int16_t)(x - g->camX), sy = (int16_t)(y - g->camY);
	return sx >= 0 && sx < SCREEN_W && sy >= 0 && sy < PLAY_H;
}

void logicThreat(const tGame *g, uint8_t *pAir, uint8_t *pShots) {
	uint8_t a = 0, s = 0;
	for(uint8_t i = 0; i < AIR_MAX; ++i) a = (uint8_t)(a + (g->pAir[i].active && inView(g, g->pAir[i].x, g->pAir[i].y)));
	for(uint8_t i = 0; i < ESHOT_MAX; ++i) {
		const tEShot *e = &g->pEShots[i];
		s = (uint8_t)(s + (e->active && e->kind != ESHOT_SHELL && inView(g, e->x, e->y)));
	}
	*pAir = a;
	*pShots = s;
}

// ---------------------------------------------------------------- frame ---

void enemiesUpdate(tGame *g) {
	uint8_t ubRadars = radarsUp(g);
	if(g->state == GS_PLAY) {
		if(++g->uwAlertClock >= ALERT_TIME_FRAMES) {
			g->uwAlertClock = 0;
			if(g->ubAlertTime < ALERT_TIME_MAX) ++g->ubAlertTime;
		}
		uint8_t a = logicAlert(g);
		if(!--g->uwJetTimer) {
			airSpawn(g, AIR_JET);
			int16_t t = (int16_t)(JET_EVERY + s_pJetExtra[g->difficulty] + s_pThJet[thIx(g)] - s_pAlertJet[a] + (logicRandom(g) & 255));
			if(!ubRadars) t = (int16_t)(t + JET_EVERY);
			g->uwJetTimer = (uint16_t)(t < 200 ? 200 : t);
		}
		if(!--g->uwBomberTimer) {
			if(g->carrier.hp) airSpawn(g, AIR_BOMBER);
			int16_t t = (int16_t)(BOMBER_EVERY + s_pBomberExtra[g->difficulty] + s_pThBomber[thIx(g)] - s_pAlertBomber[a] + (logicRandom(g) & 511));
			g->uwBomberTimer = (uint16_t)(t < 400 ? 400 : t);
		}
		if(!--g->uwBoatTimer) {
			boatSpawn(g);
			g->uwBoatTimer = (uint16_t)(BOAT_EVERY + s_pThBoat[thIx(g)] - s_pAlertBoat[a] + (logicRandom(g) & 255));
		}
	}
	turretsUpdate(g);
	eshotsUpdate(g);
	airUpdate(g, ubRadars);
	boatsUpdate(g);
	cruiserUpdate(g);
}
