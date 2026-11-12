/**
 * hiscore: a top-N hall of fame for an AGK game (plain C, no Amiga headers,
 * so `agk unit` tests it on the host). techniques/hiscore/TECHNIQUE.md is the
 * spec. Extracted from Rotor Rescue (src/hiscore.c, logic.c) and used by
 * Rotor Raider 1.5.
 *
 *   - the table: initials, score, a game-defined "reached" byte (mission,
 *     theatre, level...) and a game-defined 16-bit "extra" (hostages saved,
 *     factories destroyed...), best first; an equal score ranks below;
 *   - the file: HI_BYTES, big-endian, a 4-byte magic and a 16-bit checksum;
 *     an unpack that fails changes nothing (the game keeps its defaults);
 *   - the initials: a joystick editor with a timeout;
 *   - the table screen's lines: hiRow();
 *   - invincible (test) games and the demo never rank: hiQualify().
 *
 * The disk side (AmigaDOS, write-protect check, flush delay) is
 * hiscore_disk.c, which only the Amiga build compiles.
 */
#ifndef _HISCORE_H_
#define _HISCORE_H_

#include <stdint.h>

#ifndef HI_COUNT
#define HI_COUNT 5                 // entries (-DHI_COUNT=n to change)
#endif
#define HI_NAME 3                  // initials
#define HI_LETTERS " ABCDEFGHIJKLMNOPQRSTUVWXYZ."   // in stick order; the first is a space
#define HI_ENTRY_BYTES 10          // on the disk: initials, reached, extra (2), score (4)
#define HI_BYTES (4 + HI_COUNT * HI_ENTRY_BYTES + 2)   // magic, entries, checksum

typedef struct {
	char name[HI_NAME];
	uint8_t reached;               // game-defined: 1..reachedMax
	uint16_t extra;                // game-defined: shown beside the score
	uint32_t score;
} tHiScore;

typedef struct {
	tHiScore pEntries[HI_COUNT];   // best first
} tHiTable;

_Static_assert(HI_COUNT >= 1 && HI_COUNT <= 9, "hiRow prints the rank as one digit");

/** What a game fixes once (a const struct). */
typedef struct {
	char szMagic[5];               // the file's first 4 bytes ("RRH1"); the 5th is the terminator
	uint8_t ubReachedMax;          // the largest valid `reached` (a damaged file is refused)
	const char *szFile;            // the disk file's name, in the game's directory
} tHiConfig;

// ---------------------------------------------------------------- table ---

/** Copy the game's HI_COUNT default entries (best first) into the table. */
void hiDefaults(tHiTable *pT, const tHiScore *pDefaults);

/** The place ulScore would take (0 = first), or HI_COUNT if it doesn't make
 *  the table. A score equal to one already there ranks below it; 0 never ranks. */
uint8_t hiRank(const tHiTable *pT, uint32_t ulScore);

/** hiRank, but HI_COUNT for a game that never counts: an invincible one (the
 *  game's test mode) or the attract mode's demo. */
uint8_t hiQualify(const tHiTable *pT, uint32_t ulScore, uint8_t isInvincible, uint8_t isDemo);

/** Put a game into the table at its rank (the entries below move down, the
 *  last drops off) with the initials "A  ", to be edited (hiEditUpdate).
 *  Returns the rank, or HI_COUNT (nothing changed) if it doesn't make it. */
uint8_t hiInsert(tHiTable *pT, uint32_t ulScore, uint8_t ubReached, uint16_t uwExtra);

/** The best score (what a HUD's HI shows). */
static inline uint32_t hiTop(const tHiTable *pT) { return pT->pEntries[0].score; }

// ----------------------------------------------------------------- file ---

/** HI_BYTES into pOut. */
void hiPack(const tHiTable *pT, const tHiConfig *pCfg, uint8_t *pOut);

/** 1: pIn (HI_BYTES) is a valid table and is now in pT. 0: wrong magic, a bad
 *  checksum, characters that aren't letters, an entry out of order or with
 *  `reached` 0 or over the maximum - and pT is exactly as it was. */
uint8_t hiUnpack(tHiTable *pT, const tHiConfig *pCfg, const uint8_t *pIn);

// -------------------------------------------------------------- initials ---

#define HI_EDIT_FRAMES 1500        // 30 s; then what's chosen stands
#define HI_EDIT_REPEAT 8           // frames between letters while up/down is held

typedef struct {
	uint8_t ubRank;                // the entry being edited
	uint8_t ubPos;                 // its letter being chosen; HI_NAME when done
	uint8_t ubHold;                // up/down held: frames to the next letter
	uint8_t isFireWas, isLeftWas;  // last frame's fire and left (edges)
	uint16_t uwLeft;               // frames to the timeout
} tHiEdit;

/** Start editing entry ubRank (from hiInsert). Fire held from the end screen
 *  doesn't count as a press. */
void hiEditBegin(tHiEdit *pE, uint8_t ubRank, uint16_t uwFrames);

/** One frame of the stick: dy -1 (up) the next letter, +1 the previous, held:
 *  repeating; fire: the next letter (it starts as the one just chosen); left:
 *  back one. Returns 1 once the name is done (the last letter chosen, or the
 *  timeout: the letters as they are). */
uint8_t hiEditUpdate(tHiEdit *pE, tHiTable *pT, int8_t dx, int8_t dy, uint8_t isFire);

// --------------------------------------------------------------- display ---

#define HI_REACHED_W 6             // the table line's columns
#define HI_EXTRA_W 8
#define HI_ROW_LEN 35              // a line's characters (the buffer holds one more: the terminator)

/** 6 digits of ulValue (zero-padded, no terminator; 999999 at most shown). */
void hiNumber(char *pOut, uint32_t ulValue, uint8_t ubDigits);

/** Entry i's line into szOut (HI_ROW_LEN + 1 bytes): "- 1  ABC  012345  CORAL   14 FACT -"
 *  (the dashes only on the new entry, isNew). szReached and szExtra are the game's
 *  words for that entry's fields, cut or padded to HI_REACHED_W / HI_EXTRA_W. */
void hiRow(char *szOut, const tHiTable *pT, uint8_t i, uint8_t isNew, const char *szReached, const char *szExtra);

// ------------------------------------------------------------ disk status ---

enum { HI_DISK_NONE, HI_DISK_SAVING, HI_DISK_SAVED, HI_DISK_PROTECTED, HI_DISK_FAILED, HI_DISK_LOADED, HI_DISK_DEFAULT };

/** The line to show under the table for a status ("" for none, loaded, default). */
const char *hiDiskText(uint8_t ubStatus);

#ifdef AMIGA
// ------------------------------------------------------------------ disk ---
// hiscore_disk.c: the table as one small file next to the game, through
// AmigaDOS with the OS briefly back (as ACE's disk files do). A write-protected
// disk is found with Info() before anything is written; requesters are off
// (ACE sets pr_WindowPtr to -1), so a missing, full or protected disk is a
// status, never a requester waiting for a click nobody can see.

/** Read the table from the game's directory into pT. HI_DISK_LOADED, or
 *  HI_DISK_DEFAULT (no file, a short one or an invalid one: pT is untouched,
 *  so the game's defaults stay). Call it at startup, before systemUnuse() or
 *  during the splash: it takes a moment on a floppy. */
uint8_t hiDiskLoad(tHiTable *pT, const tHiConfig *pCfg);

/** Write the table. HI_DISK_SAVED, HI_DISK_PROTECTED (write-protected: nothing
 *  tried) or HI_DISK_FAILED. It keeps the OS running 2 s after closing the file so
 *  the file system and trackdisk flush: call it between pictures (show
 *  HI_DISK_SAVING first), then agkPerfSkip(). */
uint8_t hiDiskSave(const tHiTable *pT, const tHiConfig *pCfg);
#endif

#endif // _HISCORE_H_
