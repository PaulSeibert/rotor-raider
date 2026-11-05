// ROTOR RAIDER - the Desert Palms Software logo at boot.
// (c) 2026 Desert Palms Software. Game by Paul Ross Seibert. (From Rotor Rescue.)
#ifndef _SPLASH_H_
#define _SPLASH_H_

#include <ace/types.h>

/** Loads the logo from the floppy (before systemUnuse) and writes its copper
 *  lists. pBlankChip: a few zero bytes in chip RAM (blank sprites).
 *  Returns 1 if the logo will show. */
UBYTE splashCreate(UBYTE ubViewPosY, const void *pBlankChip);

/** Points the copper at the logo (after viewLoad / systemUnuse). */
void splashShow(void);

/** One frame of the logo: fades in, holds, fades out (fire skips ahead).
 *  Returns 0 once it's over (black): then the game's copper list takes over. */
UBYTE splashProcess(UBYTE isSkip);

/** Frees the logo once the game's copper list is showing. Safe to call twice. */
void splashDestroy(void);

#endif // _SPLASH_H_
