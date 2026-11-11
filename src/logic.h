/**
 * ROTOR RAIDER - game rules, plain portable C (by Paul Ross Seibert).
 * (c) 2026 Desert Palms Software.
 *
 * Nothing in logic.c may include ACE or Amiga headers: it is compiled for the
 * Amiga *and* for the host so `agk unit` can test it in milliseconds.
 * Deterministic (a seeded LFSR, no timers), 16-bit maths in per-frame code.
 *
 * The world is a theatre: a map of 16x16 tiles (disk/theatreN.bin), x right,
 * y down, in pixels. Headings: 16 of them, 0 = north, clockwise.
 */
#ifndef _LOGIC_H_
#define _LOGIC_H_

#include <stdint.h>

// a 16 x 16 -> 32 bit multiply: the 68000's muls.w (int maths would call a
// slow library routine), plain C on the host for the unit tests
static inline int32_t mulw(int16_t a, int16_t b) {
#if defined(__mc68000__) || defined(__m68k__)
	int32_t r = a;
	__asm__("muls.w %1,%0" : "+d"(r) : "d"(b));
	return r;
#else
	return (int32_t)a * b;
#endif
}

// a 32 / 16 -> 16 bit signed divide: the 68000's divs.w (the quotient must
// fit 16 bits), plain C on the host
static inline int16_t divw(int32_t a, int16_t b) {
#if defined(__mc68000__) || defined(__m68k__)
	int32_t r = a;
	__asm__("divs.w %1,%0" : "+d"(r) : "d"(b));
	return (int16_t)r;
#else
	return (int16_t)(a / b);
#endif
}

// ------------------------------------------------------------- the world ---
#define SCREEN_W 320
#define PLAY_H 216             // the play area; the HUD (40 lines) is below it
#define TILE 16
#define MAP_W_MAX 160
#define MAP_H_MAX 128

// Tile numbers (art/tools/tiles.py draws them in this order)
#define TILE_COAST0 0          // 0-15: coast by corner mask; 0 open sea, 15 plain land
#define TILE_SEA0 16
#define TILE_LAND0 20
#define TILE_LAND_PLAIN 15
#define TILE_FACTORY 28        // 3x3: intact; +9 damaged; +18 destroyed
#define TILE_TURRET 55         // +1 its crater
#define TILE_RADAR 57          // +1 its crater
#define TILE_SHIPYARD 59       // 4x2
#define TILE_AIRSTRIP 67       // 5x1, a helipad first
#define TILE_CRATER 76

typedef struct {
	uint8_t *pTiles;           // uwW * uwH, row by row (structures change them)
	uint16_t uwW, uwH;         // in tiles
} tMap;

// --------------------------------------------------------------- theatre ---
// disk/theatreN.bin (art/tools/theatres.py): a header, the tiles, objects,
// then the island bands' colours. Parsed by theatre.c.
enum { OBJ_NONE, OBJ_CARRIER, OBJ_FACTORY, OBJ_TURRET, OBJ_RADAR, OBJ_SHIPYARD,
	OBJ_AIRSTRIP, OBJ_PIER, OBJ_SPAWN };

typedef struct {
	uint8_t type, tx, ty, a, b, c;
} tTheatreObj;

#define BANDS_MAX 4
typedef struct {
	uint8_t ubRow;             // first tile row
	uint16_t pColors[8];       // terrain pens 0-7 (0 = the sea, COLOR00)
} tBand;

typedef struct {
	tMap map;
	const tTheatreObj *pObjs;
	uint16_t uwObjs;
	tBand pBands[BANDS_MAX];
	uint8_t ubBands;
} tTheatre;

#define THEATRE_HEADER 12
/** pData: the whole file (writable: the tiles are the game's map). 0 if it
 *  isn't a theatre. */
uint8_t theatreParse(uint8_t *pData, uint32_t ulSize, tTheatre *pOut);

// ------------------------------------------------------------- the input ---
typedef struct {
	int8_t dx, dy;             // -1, 0, 1
	uint8_t fire, fire2;       // held
} tInput;

enum { STEER_CLASSIC, STEER_ARCADE };

// --------------------------------------------------------------- objects ---
typedef struct {
	int16_t x, y;              // centre, world px
	int16_t vx, vy;            // 1/16 px a frame
	uint8_t fx, fy;            // sub-pixel accumulators (1/16)
	uint8_t heading;           // 0..15, 0 = north, clockwise
	uint8_t turnTimer;         // frames to the next heading step
	uint8_t alt;               // 0 landed .. ALT_CRUISE
	uint8_t landed;
	uint8_t landTimer;         // frames hovering slow and still over a deck
	uint8_t damage;            // 0..DAMAGE_MAX: at the max it goes down
	uint8_t bombs;             // 0..BOMBS_MAX
	uint8_t serviceTimer;      // repairs and reloads on deck
	uint8_t crashed;           // going down, then wreckage
} tChopper;

typedef struct {
	int16_t x, y;              // centre, world px
	int8_t dir;                // 1 east, -1 west
	int8_t moved;              // px it moved this frame (riders move with it)
	uint8_t fx;
	int16_t xMin, xMax;        // its patrol lane
	uint8_t hp;
} tCarrier;

typedef struct {
	uint16_t timer;            // repairs, rebuilding, firing
	uint8_t type;              // OBJ_FACTORY, OBJ_TURRET, ...
	uint8_t tx, ty;            // top-left tile
	uint8_t hp;                // 0: destroyed
	uint8_t stage;             // the tiles shown (factories: 0 intact, 1 damaged, 2 destroyed)
	uint8_t link;              // piers: the factory they supply
} tStruct;                     // 8 bytes: a power of two, so a loop over them needs no divide
#define STRUCT_MAX 48

enum { SHOT_CANNON, SHOT_BOMB };
typedef struct {
	int16_t x, y;              // air position; the ground under it is (x + alt / 2, y + alt)
	int16_t vx, vy;            // 1/16 px a frame
	uint8_t fx, fy;
	uint8_t active, kind, alt;
} tShot;
#define SHOT_MAX 4
#define BOMB_FALL_MAX 2

typedef struct {
	int16_t x, y;              // centre
	uint8_t active, frame, timer, big;
} tBoom;
#define BOOM_MAX 8

#define DIRTY_MAX 24           // tiles the map changed (the scroller redraws them)

// ---------------------------------------------------------------- enemies ---
enum { ESHOT_FLAK, ESHOT_MISSILE, ESHOT_SHELL };
typedef struct {
	int16_t x, y;              // flak bursts (shells land) where it is when life runs out
	int16_t vx, vy;            // 1/16 px a frame
	uint8_t fx, fy;
	uint8_t active, kind, life, heading;
	uint8_t pad[2];
} tEShot;                      // 16 bytes (loops over a power of two need no divide)
#define ESHOT_MAX 8

enum { AIR_JET, AIR_BOMBER };
typedef struct {
	int16_t x, y;
	int16_t vx, vy;
	uint8_t fx, fy;
	uint8_t active, kind, heading, hp;
	uint8_t timer;             // turning, firing
	uint8_t done;              // leaving (bombers: bombs dropped)
	uint16_t uwLife;           // frames before a jet gives up and leaves
	uint8_t pad[14];
} tAir;                        // 32 bytes
#define AIR_MAX 4

typedef struct {
	int16_t x, y;
	int16_t vx, vy;
	uint8_t fx, fy;
	uint8_t active, heading, hp;
	uint8_t pier;              // index of the pier structure it sails for
	uint8_t leaving;
} tBoat;
#define BOAT_MAX 3

typedef struct {
	int16_t x, y;
	int8_t dir;
	uint8_t fx;
	uint8_t active, hp;
	uint8_t timer;             // between salvoes
	uint8_t sunk;              // gone for good
} tCruiser;

enum { GS_TITLE, GS_PLAY, GS_LOST, GS_CLEAR, GS_OVER, GS_VICTORY };
enum { MSG_NONE, MSG_THEATRE, MSG_FACTORY, MSG_CRUISER, MSG_SUNK, MSG_CRUISER_SUNK, MSG_RELOAD, MSG_REPAIR };
extern const char *const g_pTheatreName[3];   // (THEATRES)

typedef struct {
	uint8_t state;
	uint16_t frame;
	uint16_t lfsr;
	uint8_t steer;             // STEER_*
	uint8_t difficulty;
	tMap map;
	tChopper chop;
	tCarrier carrier;
	tStruct pStructs[STRUCT_MAX];
	uint8_t ubStructs;
	uint8_t ubTurrets;
	uint8_t pTurrets[STRUCT_MAX];   // the turrets' indices in pStructs (their guns are drawn each picture)
	uint8_t factoriesLeft;
	tShot pShots[SHOT_MAX];        // the cannon's
	tShot pBombs[BOMB_FALL_MAX];
	tBoom pBooms[BOOM_MAX];
	tEShot pEShots[ESHOT_MAX];
	tAir pAir[AIR_MAX];
	tBoat pBoats[BOAT_MAX];
	tCruiser cruiser;
	uint16_t uwBuild;              // the shipyard's cruiser: 0..CRUISER_BUILD
	uint16_t uwJetTimer, uwBomberTimer, uwBoatTimer;
	uint16_t uwAlertClock;         // frames toward the next time step of the alert
	uint8_t ubAlertTime;           // alert from time (0..ALERT_TIME_MAX)
	uint8_t ubFactories;           // at the start
	uint8_t lives;
	uint16_t stateTimer;
	uint8_t theatre;               // 0..THEATRES-1
	uint8_t pSpawnX[8], pSpawnY[8], ubSpawns;   // tiles where planes come from
	uint8_t wantTheatre;           // main.c loads this theatre (0xFF: none)
	uint8_t msg;                   // MSG_*: a few seconds of text over the play
	uint16_t msgTimer;
	uint8_t titleArmed;            // fire must be let go before the title takes it
	int16_t titleX0, titleY0;      // where the title's pan starts (the camera then)
	uint16_t titleFrame;
	uint8_t fireHold;              // frames fire has been held
	uint8_t prevFire, prevFire2, isTwoButton;
	uint32_t score, hiScore;
	uint8_t pDirty[DIRTY_MAX][2];  // tx, ty
	uint8_t ubDirty;
	int16_t camX, camY;        // the play area's top-left, world px
	uint16_t events;           // EV_* of the latest frame
	int16_t pEvX[16];          // where (world x) the latest event of each kind happened (by bit)
} tGame;

// events bits
#define EV_LANDED    0x0001
#define EV_TAKEOFF   0x0002
#define EV_CANNON    0x0004
#define EV_BOMB      0x0008
#define EV_BOOM      0x0010
#define EV_BIGBOOM   0x0020
#define EV_FACTORY   0x0040   // a factory destroyed
#define EV_FLAK      0x0080
#define EV_MISSILE   0x0100
#define EV_HIT       0x0200   // the gunship took damage
#define EV_CRASH     0x0400   // ... and went down
#define EV_CARRIER   0x0800   // the carrier took damage
#define EV_CRUISER   0x1000   // the cruiser set sail
#define EV_STATE     0x2000
#define EV_SUNK      0x4000   // the carrier sank
void logicEventAt(tGame *g, uint16_t uwEv, int16_t x);   // events |= uwEv, which happened at world x
int16_t logicEventX(const tGame *g, uint16_t uwEv);       // ... where it was (one bit)

// Stereo: Paula's channels 0 and 3 are on the left, 1 and 2 on the right. The
// effects' channels (sound.toml sfx_channels) are 3 and 1: an effect plays on
// the side of the screen it happens on; within SFX_MIDDLE px of the middle,
// on whichever ptplayer finds free.
#define SFX_LEFT 3
#define SFX_RIGHT 1
#define SFX_EITHER 0xFF
#define SFX_MIDDLE 40
uint8_t logicSfxChannel(const tGame *g, uint16_t uwEv);

// --------------------------------------------------------------- tuning ---
#define V_MAX 40               // 2.5 px a frame
#define V_ACCEL 2              // arcade: toward the stick's velocity
#define V_DRAG 1
#define V_BRAKE 3
// Classic steering's momentum: thrust pushes along the nose, the gunship
// glides on (turning doesn't change where it's going), the brake stops it
#define THRUST_SHIFT 4         // thrust closes 1/16 of the gap to full speed along the nose a frame
#define GLIDE_SHIFT 5          // at speed it loses 1/32 of it a frame...
#define GLIDE_TAIL 3           // ... and slow, 1 every 4 frames (frame & this): ~2.5 s to rest
#define BRAKE_SHIFT 3
#define V_REVERSE 13           // classic: down held once stopped backs off, at a third of full speed
#define REVERSE_SHIFT 4        // ... reached like thrust (1/16 of the gap a frame)
#define TURN_FRAMES 4          // frames per heading step (16 steps a turn)
#define ALT_CRUISE 24
#define LAND_SPEED 4           // slower than this (1/16 px a frame) to land
#define LAND_DELAY 25          // frames hovering still over a deck before it descends
#define DAMAGE_MAX 100
#define BOMBS_MAX 8
#define REPAIR_FRAMES 10       // on deck: 1% of damage repaired every this many frames
#define RELOAD_FRAMES 40       // ... and a bomb loaded
#define CARRIER_SPEED 4        // 1/16 px a frame
#define DECK_HALF_W 36         // where the gunship can land, around the carrier's centre
#define DECK_HALF_H 9
#define CARRIER_HP 100
#define CANNON_SPEED 80        // 5 px a frame
#define CANNON_DROP 2          // altitude lost a frame: the gun points down at the ground ahead
#define CANNON_HIT_ALT 6       // low enough to hit things on the ground
#define CANNON_REPEAT 7        // two-button pads: frames between shots while fire is held
#define BOMB_HOLD 15           // one button: frames fire is held for a bomb
#define BOMB_FORWARD 16        // a bomb leaves at the gunship's speed plus this (1/16 px a frame)
#define BOMB_MIN_ALT 8         // too low to drop one
#define BLAST_R 14             // a bomb's reach, px
#define FACTORY_HP 24          // bomb damage 8: three bombs
#define BOMB_DAMAGE 8
#define TURRET_HP 3
#define RADAR_HP 4
#define FACTORY_REPAIR 300     // frames to repair a point of a damaged factory
#define LIVES_START 4
#define THEATRES 3
#define TURRET_RANGE 120       // px, ground distance to the gunship's ground point
#define TURRET_TRACK 200       // px: a turret's gun follows the gunship from this far
#define TURRET_FLASH 0x80      // a turret's link: its gun's aim (0-7, of 8) | this while it fires
#define TURRET_TELL 0x40       // ... | this while it winds up to fire: the barrels glow, held still
#define TURRET_TELL_NORMAL 16  // frames of the tell before every round (by difficulty: 24 .. 8)
#define TURRET_COOL 110        // frames between rounds, the tell included (difficulty and alert take some off)
#define FLAK_SPEED 24          // 1.5 px a frame (3 before 1.4: playbook F2 wants 1-1.5)
#define SHELL_SPEED 48         // the cruiser's shells at the carrier: 3 px a frame
#define FLAK_R 14              // a burst this close hurts
#define FLAK_DAMAGE 8
#define MISSILE_SPEED 44
#define MISSILE_LIFE 110
#define MISSILE_DAMAGE 18
#define MISSILE_ALERT 3        // from this alert level turrets fire missiles
#define JET_SPEED 56           // 3.5 px a frame
#define JET_HP 2
#define JET_EVERY 900          // frames between jets (radar and alert make it less)
#define BOMBER_SPEED 24
#define BOMBER_HP 4
#define BOMBER_EVERY 1800
#define BOMBER_DAMAGE 12       // to the carrier
#define BOAT_SPEED 10
#define BOAT_HP 3
#define BOAT_EVERY 700
#define BOAT_REPAIR 6          // factory hp a delivery puts back
#define CRUISER_BUILD 9000     // frames to build it (3 minutes)
#define CRUISER_SETBACK 1500   // a bomb on the shipyard
#define CRUISER_HP 40
#define CRUISER_RANGE 140
#define CRUISER_DAMAGE 15
#define ALERT_TIME_FRAMES 6000 // the alert rises a step every 2 minutes...
#define ALERT_TIME_MAX 2       // ... twice at most
#define CRASH_FRAMES 100
#define LOST_FRAMES 150
#define CLEAR_FRAMES 400
#define OVER_FRAMES 350
#define CAM_LEAD 0             // look-ahead in px: velocity (1/16 px a frame) >> this
#define CAM_MAX_STEP 8         // the scroller draws one tile row/column a frame

extern const int8_t g_pHeadX[16], g_pHeadY[16];   // unit vectors * 64

// --------------------------------------------------------------- drawing ---
// BOBs on PF1 (sizes must match the art: main.c checks at startup)
enum { BOB_SHADOW, BOB_CARRIER, BOB_SHOT, BOB_BOMB, BOB_BOOM, BOB_PUFF, BOB_RETICLE,
	BOB_JET, BOB_BOMBER, BOB_BOAT, BOB_CRUISER, BOB_FLAK, BOB_MISSILE, BOB_LOGO, BOB_POINTER, BOB_GUN, BOB_TYPES };
extern const uint16_t g_pBobW[BOB_TYPES];
extern const uint8_t g_pBobH[BOB_TYPES], g_pBobFrames[BOB_TYPES];

typedef struct {
	int16_t x, y;              // screen px, top-left
	uint8_t bob, frame;
} tDraw;
#define DRAW_MAX 72
uint8_t logicDraw(const tGame *g, tDraw *pOut);   // back to front
uint8_t logicChopperFrame(const tGame *g);         // heading * 2 + rotor phase

// ------------------------------------------------------------ functions ---
void logicInit(tGame *g, uint8_t *pTiles, uint16_t uwW, uint16_t uwH);
void logicLoadTheatre(tGame *g, const tTheatre *pTh);   // structures, carrier, start on deck
void logicReticle(const tGame *g, int16_t *pX, int16_t *pY);   // where a bomb would land
uint8_t logicAlert(const tGame *g);        // 0..5: how hard the enemy is fighting back
const char *logicRank(uint32_t ulScore);
// Each theatre is harder than the last (Coral Reach is 1.0's balance)
uint8_t logicFactoryHp(const tGame *g);    // FACTORY_HP and up: 3, 4, 5 bombs
uint16_t logicCruiserBuild(const tGame *g);
uint8_t logicMissileAlert(const tGame *g);
uint8_t logicTurretTell(const tGame *g);   // frames an AA gun winds up before it fires
void logicSetDifficulty(tGame *g, uint8_t ubLevel);   // 0 invincible .. 5
void logicStartGame(tGame *g);             // from the title: theatre 0 (main.c loads it)
void logicHitChopper(tGame *g, uint8_t ubDamage);
void logicStructRepair(tGame *g, tStruct *s, uint8_t ubHp);
void logicRespawn(tGame *g);               // on the carrier's deck, or the airstrip's helipad
uint8_t logicTestJump(tGame *g);           // GODMODE only: hover by the next standing factory
// Home: the carrier, or the airstrip's helipad once it's sunk. Its heading
// (of 16) and rough distance from the gunship; 0 if there's nowhere.
uint8_t logicHomeBearing(const tGame *g, uint8_t *pHeading, int16_t *pDist);
#define DAMAGE_WARN 70         // from here the HUD says go and repair
int16_t logicDist(int16_t dx, int16_t dy);   // the longer side plus half the shorter
// enemies.c
void enemiesUpdate(tGame *g);
void enemiesReset(tGame *g);
uint8_t enemiesShot(tGame *g, int16_t x, int16_t y, uint8_t ubAlt);   // a cannon round: 1 if it hit
void enemiesBlast(tGame *g, int16_t x, int16_t y);                    // a bomb
void logicUpdate(tGame *g, const tInput *pIn);
void logicChopperUpdate(tGame *g, const tInput *pIn);
void logicCameraUpdate(tGame *g);
void logicCarrierPlace(tGame *g, int16_t x, int16_t y, int16_t xMin, int16_t xMax);
uint8_t logicOverDeck(const tGame *g);
uint16_t logicRandom(tGame *g);

uint8_t logicTileAt(const tMap *pMap, int16_t x, int16_t y);   // world px; outside: open sea
uint8_t logicHeadingFor(int8_t dx, int8_t dy);                 // stick -> heading (arcade)
#define FRAME8(h) ((uint8_t)((((h) + 1) >> 1) & 7))   // a heading of 16 -> the nearest of 8
uint8_t logicHeadingTo(int16_t dx, int16_t dy);                // a heading toward (dx, dy), of 16
int8_t logicTurnToward(uint8_t ubFrom, uint8_t ubTo);          // -1, 0, 1: the shorter way

// The PF2 scroller's maths (scroll.c): fine scroll for the left edge at x,
// with the fetch starting one word early (DDFSTRT 0x30)
uint16_t logicScrollDelay(uint16_t uwX);
int16_t logicScrollByteOffset(uint16_t uwX);

#endif // _LOGIC_H_
