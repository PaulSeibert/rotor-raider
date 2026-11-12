// ROTOR RAIDER - game rules (portable C, tested on the host by agk unit).
// (c) 2026 Desert Palms Software. Game by Paul Ross Seibert.

#include "logic.h"

const uint16_t g_pBobW[BOB_TYPES] = {[BOB_SHADOW] = 32, [BOB_CARRIER] = 96, [BOB_SHOT] = 16,
	[BOB_BOMB] = 16, [BOB_BOOM] = 32, [BOB_PUFF] = 16, [BOB_RETICLE] = 16, [BOB_JET] = 32,
	[BOB_BOMBER] = 32, [BOB_BOAT] = 16, [BOB_CRUISER] = 96, [BOB_FLAK] = 16, [BOB_MISSILE] = 16, [BOB_LOGO] = 160, [BOB_POINTER] = 16, [BOB_GUN] = 16};
const uint8_t g_pBobH[BOB_TYPES] = {[BOB_SHADOW] = 32, [BOB_CARRIER] = 32, [BOB_SHOT] = 4,
	[BOB_BOMB] = 8, [BOB_BOOM] = 32, [BOB_PUFF] = 16, [BOB_RETICLE] = 16, [BOB_JET] = 32,
	[BOB_BOMBER] = 32, [BOB_BOAT] = 16, [BOB_CRUISER] = 32, [BOB_FLAK] = 4, [BOB_MISSILE] = 16, [BOB_LOGO] = 146, [BOB_POINTER] = 16, [BOB_GUN] = 16};
const uint8_t g_pBobFrames[BOB_TYPES] = {[BOB_SHADOW] = 16, [BOB_CARRIER] = 4, [BOB_SHOT] = 1,
	[BOB_BOMB] = 1, [BOB_BOOM] = 6, [BOB_PUFF] = 4, [BOB_RETICLE] = 2, [BOB_JET] = 16,   // (8 headings, then the same 8 flashing: the missile tell)

	[BOB_BOMBER] = 8, [BOB_BOAT] = 8, [BOB_CRUISER] = 2, [BOB_FLAK] = 1, [BOB_MISSILE] = 8, [BOB_LOGO] = 1, [BOB_POINTER] = 8, [BOB_GUN] = 24};

const char *const g_pTheatreName[THEATRES] = {"CORAL REACH", "EMBER FLATS", "FROSTLINE"};

const char *logicRank(uint32_t ulScore) {
	return ulScore >= 60000 ? "SKY MARSHAL" : ulScore >= 40000 ? "WING COMMANDER" :
		ulScore >= 25000 ? "SQUADRON LEADER" : ulScore >= 12000 ? "FLIGHT LIEUTENANT" :
		ulScore >= 5000 ? "PILOT OFFICER" : "CADET";
}

// Per theatre: Coral Reach, Ember Flats, Frostline
static const uint8_t s_pFactoryHp[THEATRES] = {FACTORY_HP, FACTORY_HP + BOMB_DAMAGE, FACTORY_HP + 2 * BOMB_DAMAGE};
static const uint16_t s_pCruiserBuild[THEATRES] = {CRUISER_BUILD, 7500, 6000};
static const uint8_t s_pMissileAlert[THEATRES] = {MISSILE_ALERT, 2, 1};

static inline uint8_t theatreIx(const tGame *g) { return g->theatre < THEATRES ? g->theatre : THEATRES - 1; }
uint8_t logicFactoryHp(const tGame *g) { return s_pFactoryHp[theatreIx(g)]; }
uint16_t logicCruiserBuild(const tGame *g) { return s_pCruiserBuild[theatreIx(g)]; }
uint8_t logicMissileAlert(const tGame *g) { return s_pMissileAlert[theatreIx(g)]; }

void logicEventAt(tGame *g, uint32_t ulEv, int16_t x) {
	g->events |= ulEv;
	uint8_t b = 0;
	while(!(ulEv & 1)) { ulEv >>= 1; ++b; }
	g->pEvX[b] = x;
}

int16_t logicEventX(const tGame *g, uint32_t ulEv) {
	uint8_t b = 0;
	while(ulEv && !(ulEv & 1)) { ulEv >>= 1; ++b; }
	return g->pEvX[b & 31];
}

uint8_t logicSfxChannel(const tGame *g, uint32_t ulEv) {
	int16_t sx = (int16_t)(logicEventX(g, ulEv) - g->camX - SCREEN_W / 2);
	return sx < -SFX_MIDDLE ? SFX_LEFT : sx > SFX_MIDDLE ? SFX_RIGHT : SFX_EITHER;
}

static void say(tGame *g, uint8_t ubMsg, uint16_t uwFrames) {
	g->msg = ubMsg;
	g->msgTimer = uwFrames;
}

// Unit vectors * 64 for the 16 headings (0 = north, clockwise)
const int8_t g_pHeadX[16] = {0, 24, 45, 59, 64, 59, 45, 24, 0, -24, -45, -59, -64, -59, -45, -24};
const int8_t g_pHeadY[16] = {-64, -59, -45, -24, 0, 24, 45, 59, 64, 59, 45, 24, 0, -24, -45, -59};

static inline int16_t clampw(int16_t v, int16_t lo, int16_t hi) {
	return v < lo ? lo : v > hi ? hi : v;
}

uint16_t logicRandom(tGame *g) {
	uint16_t lsb = g->lfsr & 1;
	g->lfsr >>= 1;
	if(lsb) g->lfsr ^= 0xB400;
	return g->lfsr;
}

// position += velocity (1/16 px), keeping the fraction
static inline void subStep(int16_t *pPos, int16_t v, uint8_t *pFrac) {
	int16_t f = (int16_t)(*pFrac + v);
	*pPos = (int16_t)(*pPos + (f >> 4));
	*pFrac = (uint8_t)(f & 15);
}

// ------------------------------------------------------------- setup ---

void logicInit(tGame *g, uint8_t *pTiles, uint16_t uwW, uint16_t uwH) {
	uint8_t *p = (uint8_t *)g;
	for(uint16_t i = 0; i < sizeof(*g); ++i) p[i] = 0;
	g->lfsr = 0xACE1;
	g->state = GS_TITLE;
	g->map.pTiles = pTiles;
	g->map.uwW = uwW;
	g->map.uwH = uwH;
	g->chop.x = (int16_t)(uwW * TILE / 2);
	g->chop.y = (int16_t)(uwH * TILE / 2);
	g->chop.alt = ALT_CRUISE;
	g->chop.bombs = BOMBS_MAX;
	g->carrier.hp = CARRIER_HP;
	g->camX = (int16_t)(g->chop.x - SCREEN_W / 2);
	g->camY = (int16_t)(g->chop.y - PLAY_H / 2);
	g->difficulty = 3;
	g->hiScore = 10000;
	g->lives = LIVES_START;
	g->wantTheatre = 0xFF;
}

void logicSetDifficulty(tGame *g, uint8_t ubLevel) {
	if(ubLevel <= 5) g->difficulty = ubLevel;
}

static void setState(tGame *g, uint8_t ubState, uint16_t uwTimer) {
	if(ubState == GS_TITLE) {
		g->titleX0 = g->camX;
		g->titleY0 = (int16_t)(g->camY > 140 ? g->camY - 140 : 0);   // (the carrier off the bottom: under the text)
		g->titleFrame = 0;
	}
	g->state = ubState;
	g->stateTimer = uwTimer;
	g->events |= EV_STATE;
}

void logicStartGame(tGame *g) {
	g->lives = LIVES_START;
	g->score = 0;
	g->theatre = 0;
	g->wantTheatre = 0;            // main.c loads it, then logicLoadTheatre
	setState(g, GS_PLAY, 0);
}

// ------------------------------------------------------------- the map ---

uint8_t logicTileAt(const tMap *pMap, int16_t x, int16_t y) {
	if(x < 0 || y < 0) return 0;
	uint16_t tx = (uint16_t)x >> 4, ty = (uint16_t)y >> 4;
	if(tx >= pMap->uwW || ty >= pMap->uwH) return 0;
	return pMap->pTiles[ty * pMap->uwW + tx];
}

// ------------------------------------------------------------ structures ---

static void structSize(uint8_t ubType, uint8_t *pW, uint8_t *pH) {
	*pW = ubType == OBJ_FACTORY ? 3 : ubType == OBJ_SHIPYARD ? 4 : ubType == OBJ_AIRSTRIP ? 5 : 1;
	*pH = ubType == OBJ_FACTORY ? 3 : ubType == OBJ_SHIPYARD ? 2 : 1;
}

static void markDirty(tGame *g, uint8_t tx, uint8_t ty) {
	if(g->ubDirty < DIRTY_MAX) {
		g->pDirty[g->ubDirty][0] = tx;
		g->pDirty[g->ubDirty][1] = ty;
		++g->ubDirty;
	}
}

// The tiles a structure shows for its stage
static void structTiles(tGame *g, const tStruct *s) {
	uint8_t w, h, ubFirst;
	structSize(s->type, &w, &h);
	switch(s->type) {
		case OBJ_FACTORY: ubFirst = (uint8_t)(TILE_FACTORY + 9 * s->stage); break;
		case OBJ_TURRET: ubFirst = (uint8_t)(TILE_TURRET + (s->hp ? 0 : 1)); break;
		case OBJ_RADAR: ubFirst = (uint8_t)(TILE_RADAR + (s->hp ? 0 : 1)); break;
		case OBJ_SHIPYARD: ubFirst = TILE_SHIPYARD; break;
		case OBJ_AIRSTRIP: ubFirst = TILE_AIRSTRIP; break;
		default: return;
	}
	uint16_t uwRow = (uint16_t)mulw((int16_t)s->ty, (int16_t)g->map.uwW);
	for(uint8_t y = 0; y < h; ++y, uwRow = (uint16_t)(uwRow + g->map.uwW)) {
		for(uint8_t x = 0; x < w; ++x) {
			g->map.pTiles[uwRow + s->tx + x] = ubFirst++;
			markDirty(g, (uint8_t)(s->tx + x), (uint8_t)(s->ty + y));
		}
	}
}

void logicLoadTheatre(tGame *g, const tTheatre *pTh) {
	g->ubStructs = 0;
	g->ubTurrets = 0;
	g->factoriesLeft = 0;
	g->ubSpawns = 0;
	for(uint16_t i = 0; i < pTh->uwObjs; ++i) {
		const tTheatreObj *o = &pTh->pObjs[i];
		if(o->type == OBJ_CARRIER) {
			logicCarrierPlace(g, (int16_t)(o->tx * TILE + 8), (int16_t)(o->ty * TILE + 8),
				(int16_t)(o->a * TILE), (int16_t)(o->b * TILE));
			continue;
		}
		if(o->type == OBJ_SPAWN) {
			if(g->ubSpawns < 8) {
				g->pSpawnX[g->ubSpawns] = o->tx;
				g->pSpawnY[g->ubSpawns] = o->ty;
				++g->ubSpawns;
			}
			continue;
		}
		if(g->ubStructs >= STRUCT_MAX) continue;
		if(o->type == OBJ_TURRET) g->pTurrets[g->ubTurrets++] = g->ubStructs;
		tStruct *s = &g->pStructs[g->ubStructs++];
		s->type = o->type;
		s->tx = o->tx;
		s->ty = o->ty;
		s->stage = 0;
		s->timer = 0;
		s->link = o->type == OBJ_TURRET ? (uint8_t)((o->tx ^ o->ty) & 7) : o->a;   // turrets: the gun's aim
		s->hp = o->type == OBJ_FACTORY ? logicFactoryHp(g) : o->type == OBJ_TURRET ? TURRET_HP :
			o->type == OBJ_RADAR ? RADAR_HP : 1;
		if(o->type == OBJ_PIER) {
			// a pier keeps its way out to sea (dx, dy as signed bytes) in stage and timer
			s->stage = o->b;
			s->timer = o->c;
		}
		if(o->type == OBJ_FACTORY) ++g->factoriesLeft;
		structTiles(g, s);      // intact again (a theatre played before has craters)
	}
	// the turrets top to bottom: logicDraw looks only at the rows on screen
	for(uint8_t i = 1; i < g->ubTurrets; ++i) {
		uint8_t t = g->pTurrets[i], k = i;
		while(k && g->pStructs[g->pTurrets[k - 1]].ty > g->pStructs[t].ty) {
			g->pTurrets[k] = g->pTurrets[k - 1];
			--k;
		}
		g->pTurrets[k] = t;
	}
	g->ubDirty = 0;
	g->ubFactories = g->factoriesLeft;
	g->wantTheatre = 0xFF;
	enemiesReset(g);
	say(g, MSG_THEATRE, 220);
	logicRespawn(g);
}

void logicRespawn(tGame *g) {
	// the gunship on the carrier's helipad (or the airstrip's), ready to go
	tChopper *c = &g->chop;
	if(g->carrier.hp) {
		c->x = (int16_t)(g->carrier.x - 18);
		c->y = g->carrier.y;
	}
	else {
		for(uint8_t i = 0; i < g->ubStructs; ++i) {
			if(g->pStructs[i].type == OBJ_AIRSTRIP) {
				c->x = (int16_t)(g->pStructs[i].tx * TILE + 8);
				c->y = (int16_t)(g->pStructs[i].ty * TILE + 8);
			}
		}
	}
	c->vx = c->vy = 0;
	c->fx = c->fy = 0;
	c->alt = 0;
	c->landed = 1;
	c->landTimer = LAND_DELAY;
	c->heading = 4;
	c->damage = 0;
	c->bombs = BOMBS_MAX;
	c->crashed = 0;
	c->turnTimer = 0;
	g->camX = (int16_t)(c->x - SCREEN_W / 2);
	g->camY = (int16_t)(c->y - PLAY_H / 2);
	if(g->camX < 0) g->camX = 0;
	if(g->camY < 0) g->camY = 0;
	for(uint8_t i = 0; i < SHOT_MAX; ++i) g->pShots[i].active = 0;
	for(uint8_t i = 0; i < BOMB_FALL_MAX; ++i) g->pBombs[i].active = 0;
	for(uint8_t i = 0; i < BOOM_MAX; ++i) g->pBooms[i].active = 0;
}

// --------------------------------------------------------------- weapons ---

static void addScore(tGame *g, uint16_t uwPts) {
	g->score += uwPts;
	if(g->score > g->hiScore && g->difficulty) g->hiScore = g->score;
}

static void boomAt(tGame *g, int16_t x, int16_t y, uint8_t isBig) {
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

// The structure whose footprint holds world point (x, y), or 0
static tStruct *structAt(tGame *g, int16_t x, int16_t y, int16_t r) {
	for(uint8_t i = 0; i < g->ubStructs; ++i) {
		tStruct *s = &g->pStructs[i];
		int16_t x0 = (int16_t)(s->tx * TILE), y0 = (int16_t)(s->ty * TILE);
		// (most are far away: no structure is over 5 tiles wide or 3 high)
		if(x + r < x0 || x - r >= x0 + 5 * TILE || y + r < y0 || y - r >= y0 + 3 * TILE) continue;
		uint8_t w, h;
		structSize(s->type, &w, &h);
		if(x + r >= x0 && x - r < x0 + w * TILE && y + r >= y0 && y - r < y0 + h * TILE) return s;
	}
	return 0;
}

void logicStructRepair(tGame *g, tStruct *s, uint8_t ubHp) {
	if(s->type != OBJ_FACTORY || !s->hp) return;
	uint8_t ubMax = logicFactoryHp(g);
	s->hp = s->hp + ubHp >= ubMax ? ubMax : (uint8_t)(s->hp + ubHp);
	if(s->hp == ubMax && s->stage) {
		s->stage = 0;
		structTiles(g, s);
	}
}

static void structDamage(tGame *g, tStruct *s, uint8_t ubDmg) {
	if(!s->hp) return;
	if(s->type == OBJ_SHIPYARD) {
		// the cruiser's building is set back (once it has sailed, nothing)
		if(!g->cruiser.active && !g->cruiser.sunk) {
			g->uwBuild = g->uwBuild > CRUISER_SETBACK ? (uint16_t)(g->uwBuild - CRUISER_SETBACK) : 0;
			addScore(g, 50);
		}
		return;
	}
	if(s->type == OBJ_AIRSTRIP || s->type == OBJ_PIER) return;
	int16_t cx, cy;
	uint8_t w, h;
	structSize(s->type, &w, &h);
	cx = (int16_t)(s->tx * TILE + w * 8);
	cy = (int16_t)(s->ty * TILE + h * 8);
	s->hp = s->hp > ubDmg ? (uint8_t)(s->hp - ubDmg) : 0;
	if(s->type != OBJ_TURRET || !(s->link & TURRET_TELL)) s->timer = 0;   // (a hit never shortens a tell)
	if(s->hp) {
		if(s->type == OBJ_FACTORY && s->stage == 0) {
			s->stage = 1;
			structTiles(g, s);
		}
		return;
	}
	if(s->type == OBJ_FACTORY) {
		s->stage = 2;
		--g->factoriesLeft;
		logicEventAt(g, EV_FACTORY, cx);
		addScore(g, 1000);
	}
	else addScore(g, s->type == OBJ_RADAR ? 250 : 100);
	structTiles(g, s);
	boomAt(g, cx, cy, 1);
}

static void cannonFire(tGame *g) {
	const tChopper *c = &g->chop;
	for(uint8_t i = 0; i < SHOT_MAX; ++i) {
		tShot *s = &g->pShots[i];
		if(s->active) continue;
		s->active = 1;
		s->kind = SHOT_CANNON;
		s->x = c->x;
		s->y = c->y;
		s->fx = s->fy = 0;
		s->vx = (int16_t)(c->vx + ((g_pHeadX[c->heading] * CANNON_SPEED + 32) >> 6));
		s->vy = (int16_t)(c->vy + ((g_pHeadY[c->heading] * CANNON_SPEED + 32) >> 6));
		s->alt = c->alt;
		logicEventAt(g, EV_CANNON, c->x);
		return;
	}
}

static inline int16_t bombVx(const tChopper *c) {
	return (int16_t)(c->vx + ((g_pHeadX[c->heading] * BOMB_FORWARD + 32) >> 6));
}
static inline int16_t bombVy(const tChopper *c) {
	return (int16_t)(c->vy + ((g_pHeadY[c->heading] * BOMB_FORWARD + 32) >> 6));
}

void logicReticle(const tGame *g, int16_t *pX, int16_t *pY) {
	const tChopper *c = &g->chop;
	*pX = (int16_t)(c->x + ((bombVx(c) * c->alt) >> 4));
	*pY = (int16_t)(c->y + ((bombVy(c) * c->alt) >> 4));
}

static void bombDrop(tGame *g) {
	tChopper *c = &g->chop;
	if(!c->bombs || c->landed || c->alt < BOMB_MIN_ALT) return;
	for(uint8_t i = 0; i < BOMB_FALL_MAX; ++i) {
		tShot *s = &g->pBombs[i];
		if(s->active) continue;
		s->active = 1;
		s->kind = SHOT_BOMB;
		s->x = c->x;
		s->y = c->y;
		s->fx = s->fy = 0;
		s->vx = bombVx(c);
		s->vy = bombVy(c);
		s->alt = c->alt;
		--c->bombs;
		logicEventAt(g, EV_BOMB, c->x);
		if(!c->bombs) say(g, MSG_RELOAD, 250);
		return;
	}
}

static void triggers(tGame *g, const tInput *pIn) {
	const tChopper *c = &g->chop;
	uint8_t isArmed = !c->landed && c->alt >= BOMB_MIN_ALT;
	if(pIn->fire2 && !g->prevFire2) {
		g->isTwoButton = 1;
		if(isArmed) bombDrop(g);
	}
	if(pIn->fire && !g->prevFire) {
		g->fireHold = 0;
		if(isArmed) cannonFire(g);
	}
	else if(pIn->fire && g->fireHold < 255) {
		++g->fireHold;
		if(g->isTwoButton) {
			if(g->fireHold == CANNON_REPEAT) {
				g->fireHold = 0;
				if(isArmed) cannonFire(g);
			}
		}
		else if(g->fireHold == BOMB_HOLD - 1 && isArmed) bombDrop(g);
	}
	g->prevFire = pIn->fire;
	g->prevFire2 = pIn->fire2;
}

__attribute__((noinline)) static void shotsUpdate(tGame *g) {
	for(uint8_t i = 0; i < SHOT_MAX; ++i) {
		tShot *s = &g->pShots[i];
		if(!s->active) continue;
		subStep(&s->x, s->vx, &s->fx);
		subStep(&s->y, s->vy, &s->fy);
		s->alt = s->alt > CANNON_DROP ? (uint8_t)(s->alt - CANNON_DROP) : 0;
		int16_t gx = (int16_t)(s->x + (s->alt >> 1)), gy = (int16_t)(s->y + s->alt);
		if(enemiesShot(g, s->x, s->y, s->alt)) {
			s->active = 0;
			continue;
		}
		if(s->alt <= CANNON_HIT_ALT) {
			tStruct *t = structAt(g, gx, gy, 0);
			if(t && t->hp && (t->type == OBJ_TURRET || t->type == OBJ_RADAR)) {
				s->active = 0;
				boomAt(g, gx, gy, 0);
				structDamage(g, t, 1);
				continue;
			}
		}
		if(!s->alt) {
			s->active = 0;
			boomAt(g, gx, gy, 0);
		}
	}
}

__attribute__((noinline)) static void bombsUpdate(tGame *g) {
	for(uint8_t i = 0; i < BOMB_FALL_MAX; ++i) {
		tShot *s = &g->pBombs[i];
		if(!s->active) continue;
		subStep(&s->x, s->vx, &s->fx);
		subStep(&s->y, s->vy, &s->fy);
		if(--s->alt) continue;
		s->active = 0;
		boomAt(g, s->x, s->y, 1);
		enemiesBlast(g, s->x, s->y);
		// everything in reach: a factory takes BOMB_DAMAGE, smaller things go
		for(uint8_t k = 0; k < g->ubStructs; ++k) {
			tStruct *t = &g->pStructs[k];
			uint8_t w, h;
			structSize(t->type, &w, &h);
			int16_t x0 = (int16_t)(t->tx * TILE), y0 = (int16_t)(t->ty * TILE);
			if(s->x + BLAST_R >= x0 && s->x - BLAST_R < x0 + w * TILE &&
				s->y + BLAST_R >= y0 && s->y - BLAST_R < y0 + h * TILE) {
				structDamage(g, t, t->type == OBJ_FACTORY ? BOMB_DAMAGE : 255);
			}
		}
	}
}

// Damaged factories mend: every 4th frame, 4 frames' worth
__attribute__((noinline)) static void structsUpdate(tGame *g) {
	if(g->frame & 3) return;
	for(uint8_t i = 0; i < g->ubStructs; ++i) {
		tStruct *s = &g->pStructs[i];
		if(s->type != OBJ_FACTORY || !s->hp || s->hp >= logicFactoryHp(g)) continue;
		s->timer = (uint16_t)(s->timer + 4);
		if(s->timer < FACTORY_REPAIR) continue;
		s->timer = 0;
		logicStructRepair(g, s, 1);
	}
}

static void boomsUpdate(tGame *g) {
	for(uint8_t i = 0; i < BOOM_MAX; ++i) {
		tBoom *b = &g->pBooms[i];
		if(!b->active) continue;
		if(++b->timer < 4) continue;
		b->timer = 0;
		if(++b->frame >= (b->big ? 6 : 4)) b->active = 0;
	}
}

// ---------------------------------------------------------------- flight ---

uint8_t logicHeadingFor(int8_t dx, int8_t dy) {
	static const uint8_t pHead[3][3] = {
		{14, 0, 2},    // dy = -1: dx -1, 0, 1
		{12, 0, 4},    // dy = 0 (0, 0 isn't used)
		{10, 8, 6},    // dy = 1
	};
	return pHead[dy + 1][dx + 1];
}

int8_t logicTurnToward(uint8_t ubFrom, uint8_t ubTo) {
	uint8_t d = (uint8_t)((ubTo - ubFrom) & 15);
	return d == 0 ? 0 : d <= 8 ? 1 : -1;
}

// v moves toward the target by up to ubStep
static inline int16_t approach(int16_t v, int16_t target, uint8_t ubStep) {
	if(v < target) return v + ubStep > target ? target : (int16_t)(v + ubStep);
	if(v > target) return v - ubStep < target ? target : (int16_t)(v - ubStep);
	return v;
}

uint8_t logicOverDeck(const tGame *g) {
	int16_t dx = (int16_t)(g->chop.x - g->carrier.x), dy = (int16_t)(g->chop.y - g->carrier.y);
	if(g->carrier.hp && dx >= -DECK_HALF_W && dx <= DECK_HALF_W && dy >= -DECK_HALF_H && dy <= DECK_HALF_H) return 1;
	// the airstrip's helipad (its first tile)
	for(uint8_t i = 0; i < g->ubStructs; ++i) {
		const tStruct *s = &g->pStructs[i];
		if(s->type != OBJ_AIRSTRIP) continue;
		dx = (int16_t)(g->chop.x - (s->tx * TILE + 8));
		dy = (int16_t)(g->chop.y - (s->ty * TILE + 8));
		if(dx >= -10 && dx <= 10 && dy >= -10 && dy <= 10) return 2;
	}
	return 0;
}

// Landing, sitting on deck (riding along, repairs, reloads), taking off.
// Returns 1 while the gunship is on or going down to a deck: it doesn't fly.
static uint8_t chopperDeck(tGame *g, uint8_t isPush) {
	tChopper *c = &g->chop;
	if(isPush) {
		c->landTimer = 0;
		if(c->landed || c->alt < ALT_CRUISE) {
			if(c->landed) logicEventAt(g, EV_TAKEOFF, c->x);
			c->landed = 0;
			++c->alt;
		}
		return 0;
	}
	uint8_t isSlow = c->vx <= LAND_SPEED && c->vx >= -LAND_SPEED && c->vy <= LAND_SPEED && c->vy >= -LAND_SPEED;
	if(!c->landed && !(isSlow && logicOverDeck(g))) {
		c->landTimer = 0;
		if(c->alt < ALT_CRUISE) ++c->alt;     // drifted off: back up
		return 0;
	}
	if(c->landTimer < LAND_DELAY) {
		++c->landTimer;
		return 0;
	}
	// going down with the ship, or on it
	c->vx = c->vy = 0;
	if(logicOverDeck(g) == 1) c->x = (int16_t)(c->x + g->carrier.moved);   // (riding the carrier)
	if(c->alt) {
		if(!--c->alt) {
			c->landed = 1;
			logicEventAt(g, EV_LANDED, c->x);
		}
		return 1;
	}
	if(++c->serviceTimer >= RELOAD_FRAMES) c->serviceTimer = 0;
	if(c->damage && (c->serviceTimer == 0 || c->serviceTimer == REPAIR_FRAMES ||
		c->serviceTimer == 2 * REPAIR_FRAMES || c->serviceTimer == 3 * REPAIR_FRAMES)) --c->damage;
	if(!c->serviceTimer && c->bombs < BOMBS_MAX) ++c->bombs;
	return 1;
}

// v / 2^n, rounded toward zero (the same both ways)
static inline int16_t shrs(int16_t v, uint8_t n) {
	return v < 0 ? (int16_t)-((-v) >> n) : (int16_t)(v >> n);
}

static inline int16_t absw16(int16_t v) { return v < 0 ? (int16_t)-v : v; }

// toward zero by up to d (d >= 0)
static inline int16_t toward0(int16_t v, int16_t d) {
	if(v > 0) return v > d ? (int16_t)(v - d) : 0;
	if(v < 0) return -v > d ? (int16_t)(v + d) : 0;
	return 0;
}

// a step of 1/2^n of the way from v to t, at least 1
static inline int16_t pull(int16_t v, int16_t t, uint8_t n) {
	int16_t d = (int16_t)(t - v), s = shrs(d, n);
	if(!s && d) s = d > 0 ? 1 : -1;
	return (int16_t)(v + s);
}

// Classic steering, with momentum: thrust pulls the velocity a sixteenth of
// the way a frame toward full speed along the nose, so turning under thrust
// carves a wide arc; off the stick the gunship glides on, slowing gently
// (turning then doesn't change where it's going); down brakes, and held on
// once the gunship is stopped it backs off slowly, tail first (Raid on
// Bungeling Bay's stick: up flies, down slows then reverses, off hovers).
static void classicMomentum(tGame *g, uint8_t isThrust, uint8_t isBrake) {
	tChopper *c = &g->chop;
	if(isThrust) {
		// (rounded to nearest, so west is as fast as east)
		int16_t tvx = (int16_t)((g_pHeadX[c->heading] * V_MAX + 32) >> 6);
		int16_t tvy = (int16_t)((g_pHeadY[c->heading] * V_MAX + 32) >> 6);
		c->vx = pull(c->vx, tvx, THRUST_SHIFT);
		c->vy = pull(c->vy, tvy, THRUST_SHIFT);
		return;
	}
	if(isBrake) {
		// still moving forward (along the nose): brake; stopped or going
		// backward: toward reverse speed, tail first
		int32_t lAlong = (int32_t)c->vx * g_pHeadX[c->heading] + (int32_t)c->vy * g_pHeadY[c->heading];
		if(lAlong > 0 || ((c->vx || c->vy) && lAlong == 0)) {
			c->vx = toward0(c->vx, (int16_t)((absw16(c->vx) >> BRAKE_SHIFT) + 1));
			c->vy = toward0(c->vy, (int16_t)((absw16(c->vy) >> BRAKE_SHIFT) + 1));
			return;
		}
		int16_t tvx = (int16_t)-((g_pHeadX[c->heading] * V_REVERSE + 32) >> 6);
		int16_t tvy = (int16_t)-((g_pHeadY[c->heading] * V_REVERSE + 32) >> 6);
		c->vx = pull(c->vx, tvx, REVERSE_SHIFT);
		c->vy = pull(c->vy, tvy, REVERSE_SHIFT);
		return;
	}
	// the glide: a share of the speed, at least 1 every few frames
	int16_t wTail = !(g->frame & GLIDE_TAIL);
	int16_t dx = (int16_t)(absw16(c->vx) >> GLIDE_SHIFT), dy = (int16_t)(absw16(c->vy) >> GLIDE_SHIFT);
	c->vx = toward0(c->vx, dx ? dx : wTail);
	c->vy = toward0(c->vy, dy ? dy : wTail);
}

void logicChopperUpdate(tGame *g, const tInput *pIn) {
	tChopper *c = &g->chop;
	int8_t wTurn = 0;
	uint8_t isThrust = 0, isBrake = 0;
	if(g->steer == STEER_CLASSIC) {
		wTurn = pIn->dx;
		isThrust = pIn->dy < 0;
		isBrake = pIn->dy > 0;
	}
	else if(pIn->dx || pIn->dy) {
		wTurn = logicTurnToward(c->heading, logicHeadingFor(pIn->dx, pIn->dy));
		isThrust = 1;
	}
	if(chopperDeck(g, isThrust)) {
		if(g->steer == STEER_CLASSIC && wTurn && c->landed) {
			// turning on deck
			if(c->turnTimer) --c->turnTimer;
			if(!c->turnTimer) {
				c->heading = (uint8_t)((c->heading + wTurn) & 15);
				c->turnTimer = TURN_FRAMES;
			}
		}
		return;
	}
	if(c->turnTimer) --c->turnTimer;
	if(wTurn && !c->turnTimer) {
		c->heading = (uint8_t)((c->heading + wTurn) & 15);
		c->turnTimer = TURN_FRAMES;
	}
	if(!wTurn) c->turnTimer = 0;   // the next push turns at once

	if(g->steer == STEER_CLASSIC) classicMomentum(g, isThrust, isBrake);
	else {
		int16_t tvx = 0, tvy = 0;
		uint8_t ubStep = V_DRAG;
		if(isThrust) {
			// (rounded to nearest, so west is as fast as east)
			tvx = (int16_t)((g_pHeadX[c->heading] * V_MAX + 32) >> 6);
			tvy = (int16_t)((g_pHeadY[c->heading] * V_MAX + 32) >> 6);
			ubStep = V_ACCEL;
		}
		c->vx = approach(c->vx, tvx, ubStep);
		c->vy = approach(c->vy, tvy, ubStep);
	}
	subStep(&c->x, c->vx, &c->fx);
	subStep(&c->y, c->vy, &c->fy);
	int16_t wMaxX = (int16_t)(g->map.uwW * TILE - 16), wMaxY = (int16_t)(g->map.uwH * TILE - 16);
	if(c->x < 16 || c->x > wMaxX) { c->x = clampw(c->x, 16, wMaxX); c->vx = 0; }
	if(c->y < 16 || c->y > wMaxY) { c->y = clampw(c->y, 16, wMaxY); c->vy = 0; }
}

// --------------------------------------------------------------- carrier ---

void logicCarrierPlace(tGame *g, int16_t x, int16_t y, int16_t xMin, int16_t xMax) {
	tCarrier *k = &g->carrier;
	k->x = x;
	k->y = y;
	k->dir = 1;
	k->fx = 0;
	k->xMin = xMin;
	k->xMax = xMax;
	k->hp = CARRIER_HP;
}

static void carrierUpdate(tGame *g) {
	tCarrier *k = &g->carrier;
	k->moved = 0;
	if(!k->hp) return;
	// a quarter of a pixel a frame: fx counts the 1/16 px
	k->fx = (uint8_t)(k->fx + CARRIER_SPEED);
	if(k->fx >= 16) {
		k->fx = 0;
		k->moved = k->dir;
		k->x = (int16_t)(k->x + k->dir);
		if(k->x >= k->xMax) k->dir = -1;
		else if(k->x <= k->xMin) k->dir = 1;
	}
}

// For testing (GODMODE only): the gunship hovers just south of the next
// factory still standing, where a bomb lands on it. 1 if it jumped.
uint8_t logicTestJump(tGame *g) {
	if(g->difficulty != 0 || g->state != GS_PLAY) return 0;
	for(uint8_t i = 0; i < g->ubStructs; ++i) {
		const tStruct *s = &g->pStructs[i];
		if(s->type != OBJ_FACTORY || !s->hp) continue;
		tChopper *c = &g->chop;
		c->x = (int16_t)(s->tx * TILE + 24);
		c->y = (int16_t)(s->ty * TILE + 24 + 24);
		c->vx = c->vy = 0;
		c->heading = 0;
		c->landed = 0;
		c->alt = ALT_CRUISE;
		c->bombs = BOMBS_MAX;
		g->camX = (int16_t)(c->x - SCREEN_W / 2);
		g->camY = (int16_t)(c->y - PLAY_H / 2);
		return 1;
	}
	return 0;
}

// ------------------------------------------------------------------ home ---

static uint8_t homeAt(const tGame *g, int16_t *pX, int16_t *pY) {
	if(g->carrier.hp) {
		*pX = g->carrier.x;
		*pY = g->carrier.y;
		return 1;
	}
	for(uint8_t i = 0; i < g->ubStructs; ++i) {
		if(g->pStructs[i].type == OBJ_AIRSTRIP) {
			*pX = (int16_t)(g->pStructs[i].tx * TILE + 8);
			*pY = (int16_t)(g->pStructs[i].ty * TILE + 8);
			return 1;
		}
	}
	return 0;
}

uint8_t logicHomeBearing(const tGame *g, uint8_t *pHeading, int16_t *pDist) {
	int16_t x, y;
	if(!homeAt(g, &x, &y)) return 0;
	int16_t dx = (int16_t)(x - g->chop.x), dy = (int16_t)(y - g->chop.y);
	*pHeading = logicHeadingTo(dx, dy);
	*pDist = logicDist(dx, dy);
	return 1;
}

// ------------------------------------------------------------ the gunship ---

void logicHitChopper(tGame *g, uint8_t ubDamage) {
	tChopper *c = &g->chop;
	if(g->state != GS_PLAY || c->crashed) return;
	logicEventAt(g, EV_HIT, c->x);
	if(g->difficulty == 0) return;        // invincible: it shows, nothing more
	uint8_t ubBefore = c->damage;
	c->damage = c->damage + ubDamage >= DAMAGE_MAX ? DAMAGE_MAX : (uint8_t)(c->damage + ubDamage);
	if(ubBefore < DAMAGE_WARN && c->damage >= DAMAGE_WARN && c->damage < DAMAGE_MAX) say(g, MSG_REPAIR, 250);
	if(c->damage < DAMAGE_MAX) return;
	c->crashed = 1;
	c->landed = 0;
	if(!c->alt) c->alt = 1;
	if(g->lives) --g->lives;
	logicEventAt(g, EV_CRASH, c->x);
	setState(g, GS_LOST, LOST_FRAMES);
}

// going down: spinning, sinking; then an explosion and wreckage
static void crashUpdate(tGame *g) {
	tChopper *c = &g->chop;
	if(!(g->frame & 1)) c->heading = (uint8_t)((c->heading + 1) & 15);
	c->vx = (int16_t)(c->vx - (c->vx >> 3));
	c->vy = (int16_t)(c->vy - (c->vy >> 3));
	subStep(&c->x, c->vx, &c->fx);
	subStep(&c->y, c->vy, &c->fy);
	if(c->alt && !(g->frame & 1) && !--c->alt) {
		boomAt(g, c->x, c->y, 1);
		boomAt(g, (int16_t)(c->x + 6), (int16_t)(c->y - 4), 1);
	}
}

// ---------------------------------------------------------------- camera ---

static inline int16_t camStep(int16_t d) {
	int16_t s = (int16_t)(d >> 3);
	if(!s) s = d > 0 ? 1 : d < 0 ? -1 : 0;
	return clampw(s, -CAM_MAX_STEP, CAM_MAX_STEP);
}

void logicCameraUpdate(tGame *g) {
	const tChopper *c = &g->chop;
	int16_t tx = (int16_t)(c->x - SCREEN_W / 2 + (c->vx >> CAM_LEAD));
	int16_t ty = (int16_t)(c->y - PLAY_H / 2 + (c->vy >> CAM_LEAD));
	if(g->state == GS_TITLE) {
		// a slow pan from where the camera was: east and back, a pixel every
		// other frame (it never jumps: the scroller draws a tile row a frame)
		uint16_t uwT = (uint16_t)(++g->titleFrame >> 1);
		uint16_t uwSpan = 600;
		while(uwT >= 2 * uwSpan) uwT = (uint16_t)(uwT - 2 * uwSpan);
		tx = (int16_t)(g->titleX0 + (uwT < uwSpan ? uwT : 2 * uwSpan - uwT));
		ty = g->titleY0;
	}
	tx = clampw(tx, 0, (int16_t)(g->map.uwW * TILE - SCREEN_W));
	ty = clampw(ty, 0, (int16_t)(g->map.uwH * TILE - PLAY_H));
	g->camX = (int16_t)(g->camX + camStep((int16_t)(tx - g->camX)));
	g->camY = (int16_t)(g->camY + camStep((int16_t)(ty - g->camY)));
}

// ------------------------------------------------------------- the frame ---

void logicUpdate(tGame *g, const tInput *pIn) {
	++g->frame;
	g->events = 0;
	carrierUpdate(g);
	if(g->stateTimer) --g->stateTimer;
	switch(g->state) {
		case GS_TITLE:
			// left/right: the steering; fire (let go first): start
			if(pIn->dx && !g->prevFire2) g->steer = pIn->dx < 0 ? STEER_CLASSIC : STEER_ARCADE;
			if(!pIn->fire) g->titleArmed = 1;
			else if(g->titleArmed) {
				g->titleArmed = 0;
				logicStartGame(g);
			}
			g->prevFire = pIn->fire;
			break;
		case GS_PLAY:
			logicChopperUpdate(g, pIn);
			triggers(g, pIn);
			if(!g->factoriesLeft && g->ubFactories && g->wantTheatre == 0xFF) {
				addScore(g, (uint16_t)(2000 + 500 * g->lives));
				setState(g, GS_CLEAR, CLEAR_FRAMES);
			}
			break;
		case GS_LOST:
			crashUpdate(g);
			if(!g->stateTimer) {
				if(g->lives) {
					logicRespawn(g);
					setState(g, GS_PLAY, 0);
				}
				else setState(g, GS_OVER, OVER_FRAMES);
			}
			break;
		case GS_CLEAR:
			if(!g->stateTimer) {
				if(g->theatre + 1 < THEATRES) {
					++g->theatre;
					g->wantTheatre = g->theatre;   // main.c loads it
					setState(g, GS_PLAY, 0);
				}
				else setState(g, GS_VICTORY, OVER_FRAMES);
			}
			break;
		case GS_OVER:
		case GS_VICTORY:
			if(!g->stateTimer) {
				g->titleArmed = 0;
				setState(g, GS_TITLE, 0);
			}
			break;
	}
	enemiesUpdate(g);
	if(g->events & EV_FACTORY) say(g, MSG_FACTORY, 120);
	if(g->events & EV_CRUISER) say(g, MSG_CRUISER, 200);
	if(g->events & EV_CRUISER_UP) say(g, MSG_CRUISER_UP, 200);
	if(g->events & EV_CRUISER_SUNK) say(g, MSG_CRUISER_SUNK, 250);
	if(g->events & EV_SUNK) say(g, MSG_SUNK, 220);
	if(g->msgTimer && !--g->msgTimer) g->msg = MSG_NONE;
	shotsUpdate(g);
	bombsUpdate(g);
	structsUpdate(g);
	boomsUpdate(g);
	logicCameraUpdate(g);
}

// --------------------------------------------------------------- drawing ---

// a heading of 16 to the nearest of 8 drawn frames

static inline uint8_t onScreen(int16_t sx, int16_t sy, uint16_t w, uint8_t h) {
	return sx > -(int16_t)w && sx < SCREEN_W && sy > -(int16_t)h && sy < PLAY_H;
}

static inline __attribute__((always_inline)) uint8_t drawAdd(tDraw *pOut, uint8_t n, int16_t sx, int16_t sy, uint8_t bob, uint8_t frame) {
	if(n >= DRAW_MAX || !onScreen(sx, sy, g_pBobW[bob], g_pBobH[bob])) return n;
	pOut[n].x = sx;
	pOut[n].y = sy;
	pOut[n].bob = bob;
	pOut[n].frame = frame;
	return (uint8_t)(n + 1);
}

uint8_t logicDraw(const tGame *g, tDraw *pOut) {
	uint8_t n = 0;
	int16_t cx = g->camX, cy = g->camY;
	const tCarrier *k = &g->carrier;
	// the turrets' guns, on their tiles: aimed, glowing as they wind up (the
	// tell, frames 16-23) and flashing as they fire (8-15)
	// (sorted top to bottom: from the first row on screen to the last)
	int16_t wRow0 = (int16_t)((cy - 15) >> 4), wRow1 = (int16_t)((cy + PLAY_H - 1) >> 4);
	for(uint8_t i = 0; i < g->ubTurrets; ++i) {
		const tStruct *s = &g->pStructs[g->pTurrets[i]];
		if((int16_t)s->ty < wRow0) continue;
		if((int16_t)s->ty > wRow1) break;
		if(!s->hp) continue;
		uint8_t f = (uint8_t)((s->link & 7) | (s->link & TURRET_FLASH ? 8 : s->link & TURRET_TELL ? 16 : 0));
		n = drawAdd(pOut, n, (int16_t)((s->tx << 4) - cx), (int16_t)((s->ty << 4) - cy), BOB_GUN, f);
	}
	if(k->hp) {
		uint8_t f = (uint8_t)((k->dir < 0 ? 2 : 0) + ((g->frame >> 3) & 1));
		n = drawAdd(pOut, n, (int16_t)(k->x - 48 - cx), (int16_t)(k->y - 16 - cy), BOB_CARRIER, f);
	}
	// ships
	const tCruiser *pCr = &g->cruiser;
	if(pCr->active) n = drawAdd(pOut, n, (int16_t)(pCr->x - 48 - cx), (int16_t)(pCr->y - 16 - cy), BOB_CRUISER, pCr->dir < 0 ? 1 : 0);
	for(uint8_t i = 0; i < BOAT_MAX; ++i) {
		const tBoat *b = &g->pBoats[i];
		if(b->active) n = drawAdd(pOut, n, (int16_t)(b->x - 8 - cx), (int16_t)(b->y - 8 - cy), BOB_BOAT, FRAME8(b->heading));
	}
	// the gunship's shadow: the sun is to the top-left, so it falls down-right
	const tChopper *c = &g->chop;
	if((g->state == GS_PLAY || g->state == GS_LOST) && c->alt) {
		int16_t sx = (int16_t)(c->x - 16 + 2 + (c->alt >> 1) - cx), sy = (int16_t)(c->y - 16 + 2 + c->alt - cy);
		n = drawAdd(pOut, n, sx, sy, BOB_SHADOW, c->heading);
	}
	if(g->state == GS_PLAY) {
		if(c->bombs && !c->landed && c->alt >= BOMB_MIN_ALT) {
			int16_t rx, ry;
			logicReticle(g, &rx, &ry);
			n = drawAdd(pOut, n, (int16_t)(rx - 8 - cx), (int16_t)(ry - 8 - cy), BOB_RETICLE, (uint8_t)((g->frame >> 3) & 1));
		}
	}
	for(uint8_t i = 0; i < BOMB_FALL_MAX; ++i) {
		const tShot *s = &g->pBombs[i];
		if(s->active) n = drawAdd(pOut, n, (int16_t)(s->x - 4 - cx), (int16_t)(s->y - 4 - cy), BOB_BOMB, 0);
	}
	for(uint8_t i = 0; i < SHOT_MAX; ++i) {
		const tShot *s = &g->pShots[i];
		if(s->active) n = drawAdd(pOut, n, (int16_t)(s->x - 2 - cx), (int16_t)(s->y - 2 - cy), BOB_SHOT, 0);
	}
	for(uint8_t i = 0; i < ESHOT_MAX; ++i) {
		const tEShot *e = &g->pEShots[i];
		if(!e->active) continue;
		if(e->kind == ESHOT_MISSILE) n = drawAdd(pOut, n, (int16_t)(e->x - 8 - cx), (int16_t)(e->y - 8 - cy), BOB_MISSILE, FRAME8(e->heading));
		else n = drawAdd(pOut, n, (int16_t)(e->x - 2 - cx), (int16_t)(e->y - 2 - cy), BOB_FLAK, 0);
	}
	for(uint8_t i = 0; i < AIR_MAX; ++i) {
		const tAir *a = &g->pAir[i];
		if(a->active) n = drawAdd(pOut, n, (int16_t)(a->x - 16 - cx), (int16_t)(a->y - 16 - cy),
			a->kind == AIR_JET ? BOB_JET : BOB_BOMBER,
			(uint8_t)(FRAME8(a->heading) + (a->kind == AIR_JET && a->ubTell ? 8 : 0)));   // 8-15: the tell's flash
	}
	// home's pointer at the edge of the play area, when it's off screen
	int16_t hx, hy;
	if(g->state == GS_PLAY && homeAt(g, &hx, &hy)) {
		int16_t sx = (int16_t)(hx - cx), sy = (int16_t)(hy - cy);
		if(sx < -16 || sx > SCREEN_W + 16 || sy < -16 || sy > PLAY_H + 16) {
			uint8_t isUrgent = !c->bombs || c->damage >= DAMAGE_WARN;
			if(!isUrgent || (g->frame & 8)) {
				// along the line from the middle of the play area, to a box 12 px in
				int16_t dx = (int16_t)(sx - SCREEN_W / 2), dy = (int16_t)(sy - PLAY_H / 2);
				int16_t ax = dx < 0 ? (int16_t)-dx : dx, ay = dy < 0 ? (int16_t)-dy : dy;
				int16_t px, py;
				const int16_t wHw = SCREEN_W / 2 - 12, wHh = PLAY_H / 2 - 12;
				if(mulw(ax, wHh) > mulw(ay, wHw)) {
					px = (int16_t)(SCREEN_W / 2 + (dx < 0 ? -wHw : wHw));
					py = (int16_t)(PLAY_H / 2 + divw(mulw(dy, wHw), ax));
				}
				else {
					py = (int16_t)(PLAY_H / 2 + (dy < 0 ? -wHh : wHh));
					px = (int16_t)(SCREEN_W / 2 + divw(mulw(dx, wHh), ay));
				}
				n = drawAdd(pOut, n, (int16_t)(px - 8), (int16_t)(py - 8), BOB_POINTER, FRAME8(logicHeadingTo(dx, dy)));
			}
		}
	}
	for(uint8_t i = 0; i < BOOM_MAX; ++i) {
		const tBoom *b = &g->pBooms[i];
		if(!b->active) continue;
		if(b->big) n = drawAdd(pOut, n, (int16_t)(b->x - 16 - cx), (int16_t)(b->y - 16 - cy), BOB_BOOM, b->frame);
		else n = drawAdd(pOut, n, (int16_t)(b->x - 8 - cx), (int16_t)(b->y - 8 - cy), BOB_PUFF, b->frame);
	}
	return n;
}

uint8_t logicChopperFrame(const tGame *g) {
	const tChopper *c = &g->chop;
	// the rotor turns slower on deck
	uint8_t ubPhase = c->landed ? (uint8_t)((g->frame >> 3) & 1) : (uint8_t)((g->frame >> 1) & 1);
	return (uint8_t)((c->heading << 1) + ubPhase);
}

// -------------------------------------------------------------- scrolling ---

uint16_t logicScrollDelay(uint16_t uwX) {
	// The fetch starts one word early (DDFSTRT 0x30), shifted right by the
	// delay: pixel x is at the left edge with a delay of 16 - x % 16.
	return (uint16_t)((16 - (uwX & 15)) & 15);
}

int16_t logicScrollByteOffset(uint16_t uwX) {
	return (int16_t)((((int16_t)uwX - 1) >> 4) * 2);
}
