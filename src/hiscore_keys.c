// hiscore_keys.c: the keyboard side of the initials entry (Amiga only). It turns
// ACE's key states into the one raw keycode per frame that hiEditKey wants.
// See techniques/hiscore/TECHNIQUE.md.
#include <ace/managers/key.h>
#include "hiscore.h"

#define HI_KEYS_SCANNED 0x68   // 0x00-0x67: every key, the modifiers too (any key restarts the timeout)

uint8_t hiKeyPoll(void) {
	for(UBYTE k = 0; k < HI_KEYS_SCANNED; ++k) {
		if(keyUse(k)) return k;   // (ACTIVE -> USED: seen once per press; a key held is not typed again)
	}
	return HI_KEY_NONE;
}

void hiKeyFlush(void) {
	for(UBYTE k = 0; k < HI_KEYS_SCANNED; ++k) keyUse(k);
}
