/**
 * attract: the idle cycle of an arcade game, and demo playback (plain C, no
 * Amiga headers, so `agk unit` tests it on the host).
 * techniques/attract/TECHNIQUE.md is the spec. Extracted from Rotor Rescue's
 * attract loop and used by Rotor Raider 1.5.
 *
 *   title -> hall of fame -> demo -> (credits) -> title
 *
 * attractUpdate() is the cycle's clock: left alone on the title for uwIdle
 * frames it moves to the next page; each page lasts its time (the demo until
 * its recorded inputs run out or the player is lost); fire ends any page.
 * The game owns what each page looks like and what the demo does.
 *
 * The DEMO is a recorded run of joystick inputs (tDemo): the game puts itself
 * in a FIXED state (the level loaded fresh, the random seed and the frame
 * counter set), marks itself as a demo (no score counts, a DEMO marker), and
 * feeds demoNext()'s bits to its input reader instead of the joystick. The
 * game is deterministic, so it flies the same every time; a host unit test
 * replays it to prove it (tools/scen2demo.py makes the table from a scenario).
 */
#ifndef _ATTRACT_H_
#define _ATTRACT_H_

#include <stdint.h>

enum { ATTRACT_TITLE, ATTRACT_SCORES, ATTRACT_DEMO, ATTRACT_CREDITS };

typedef struct {
	uint16_t uwIdle;       // frames idle on the title before the hall of fame (600: 12 s)
	uint16_t uwScores;     // frames the hall of fame shows (400)
	uint16_t uwCredits;    // frames the credits page shows after the demo; 0: the game has none
} tAttractCfg;

typedef struct {
	const tAttractCfg *pCfg;
	uint8_t ubPage;        // ATTRACT_*
	uint16_t uwTimer;      // frames on this page (the demo: not counted)
	uint8_t isArmed;       // fire has been let go on this page: the next press is a fresh one
} tAttract;

/** What attractUpdate says happened. */
enum { ATTRACT_NOTHING, ATTRACT_PAGE, ATTRACT_START };

/** On the title, idle. */
void attractInit(tAttract *pA, const tAttractCfg *pCfg);

/** One frame while the game is in the cycle (on the title or any page).
 *  isFire: the fire button is held. isTouched: any other input this frame (the
 *  stick moved, a choice was made): it keeps the title from going idle.
 *  ATTRACT_START: fire pressed on the title (it was let go first): start a game.
 *  ATTRACT_PAGE: the page changed - read pA->ubPage (and start the demo, show
 *  the table...). A fire press on any page but the title returns to the title
 *  (a PAGE change); fire must then be let go and pressed again to start. */
uint8_t attractUpdate(tAttract *pA, uint8_t isFire, uint8_t isTouched);

/** The demo is over (its inputs ran out, the gunship was lost, the level was
 *  cleared): the credits page, or the title. */
void attractDemoDone(tAttract *pA);

/** Back to the title with the idle clock at 0 and fire not armed: after a game, or on boot. */
void attractReset(tAttract *pA);

// ------------------------------------------------------------------ demo ---

// Input bits of a recorded frame
#define DEMO_L 1
#define DEMO_R 2
#define DEMO_U 4
#define DEMO_D 8
#define DEMO_F 16              // the fire button
#define DEMO_F2 32             // the second button

/** A recording: runs of {frames, bits}, frames 1-255 (the tool splits longer ones). */
typedef struct {
	const uint8_t (*pRuns)[2];
	uint16_t uwRuns;
	uint16_t uwRun;            // the run being played
	uint8_t ubLeft;            // its frames left
} tDemo;

/** Start playing from the first frame. */
void demoStart(tDemo *pD, const uint8_t (*pRuns)[2], uint16_t uwRuns);

/** The next frame's input bits into *pBits; 0 once the recording is over (the game ends its demo). */
uint8_t demoNext(tDemo *pD, uint8_t *pBits);

/** The recording's length in frames. */
uint32_t demoFrames(const uint8_t (*pRuns)[2], uint16_t uwRuns);

#endif // _ATTRACT_H_
