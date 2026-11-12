// hiscore_disk.c: the hall of fame on the floppy (Amiga only; see hiscore.h).
// From Rotor Rescue's src/hiscore.c.

#include <proto/dos.h>
#include <dos/dos.h>
#include <ace/managers/system.h>
#include "hiscore.h"

static struct InfoData s_sInfo __attribute__((aligned(4)));   // (a BPTR target: longword-aligned)
static uint8_t s_pBuf[HI_BYTES];

static void osOn(void) {
	systemUse();
	systemReleaseBlitterToOs();   // (trackdisk needs the blitter on Kickstart 1.3)
}

static void osOff(void) {
	systemGetBlitterFromOs();
	systemUnuse();
}

__attribute__((optimize("Os"))) uint8_t hiDiskLoad(tHiTable *pT, const tHiConfig *pCfg) {
	uint8_t ubResult = HI_DISK_DEFAULT;
	osOn();
	BPTR fh = Open((CONST_STRPTR)pCfg->szFile, MODE_OLDFILE);
	if(fh) {
		LONG lRead = Read(fh, s_pBuf, HI_BYTES);
		Close(fh);
		if(lRead == HI_BYTES && hiUnpack(pT, pCfg, s_pBuf)) ubResult = HI_DISK_LOADED;
	}
	osOff();
	return ubResult;
}

__attribute__((optimize("Os"))) uint8_t hiDiskSave(const tHiTable *pT, const tHiConfig *pCfg) {
	uint8_t ubResult = HI_DISK_FAILED;
	hiPack(pT, pCfg, s_pBuf);
	osOn();
	BPTR bpDir = Lock((CONST_STRPTR)"", ACCESS_READ);   // the current directory: the game's
	if(bpDir) {
		if(Info(bpDir, &s_sInfo) && s_sInfo.id_DiskState == ID_WRITE_PROTECTED) ubResult = HI_DISK_PROTECTED;
		UnLock(bpDir);
	}
	if(ubResult != HI_DISK_PROTECTED) {
		BPTR fh = Open((CONST_STRPTR)pCfg->szFile, MODE_NEWFILE);
		if(fh) {
			LONG lWritten = Write(fh, s_pBuf, HI_BYTES);
			Close(fh);
			if(lWritten == HI_BYTES) ubResult = HI_DISK_SAVED;
			// the file system writes its buffers and the drive its track a moment
			// after the last access: keep the OS running until it has
			Delay(2 * 50);
		}
		else if(IoErr() == ERROR_DISK_WRITE_PROTECTED) ubResult = HI_DISK_PROTECTED;
	}
	osOff();
	return ubResult;
}
