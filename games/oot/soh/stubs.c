#include <libultraship/libultra.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "z64.h"
#include "OTRGlobals.h"
//#include <math.h>

u32 osResetType;
u32 osTvType = OS_TV_NTSC;
// u32 osTvType = OS_TV_PAL;
OSViMode OoT_osViModeNtscLan1;
OSViMode OoT_osViModeMpalLan1;
OSViMode OoT_osViModeFpalLan1;
OSViMode OoT_osViModePalLan1;
// AudioContext gAudioContext;
// unk_D_8016E750 D_8016E750[4];
u8 gLetterTLUT[4][32];
u8 gFontFF[999];
DmaEntry gDmaDataTable[0x60C];
// u8 D_80133418;
u16 gAudioSEFlagSwapSource[64];
u16 gAudioSEFlagSwapTarget[64];
u8 gAudioSEFlagSwapMode[64];

// Zbuffer and Color framebuffer
u16 OoT_D_0E000000[SCREEN_WIDTH * SCREEN_HEIGHT];
u16 OoT_D_0F000000[SCREEN_WIDTH * SCREEN_HEIGHT];

u8 osAppNmiBuffer[2048];

f32 qNaN0x10000 = 0x7F810000;

// void gSPTextureRectangle(Gfx* pkt, s32 xl, s32 yl, s32 xh, s32 yh, u32 tile, u32 s, s32 t, u32 dsdx, u32 dtdy)
//{
//	__gSPTextureRectangle(pkt, xl, yl, xh, yh, tile, s, t, dsdx, dtdy);
// }

/* NOTE (#385): every stub below with a return type returns an explicit value.
 * Falling off the end of a non-void function is UB — in practice the caller
 * reads whatever happened to be in the return register — and this file had 23
 * such bodies. One of them, __osGetActiveQueue, fed a *garbage pointer* to the
 * crash handler's thread walk (games/oot/src/code/fault.c:537), so the fault
 * handler was itself liable to fault. soh/stubs.c is compiled with
 * -Werror=return-type / /we4716 (games/oot/CMakeLists.txt) so the class cannot
 * come back, mirroring the /we4013 precedent that retired implicit
 * declarations. Values are the real libultra success/absent codes, never
 * whatever is cheapest to write. */

OSId OoT_osGetThreadId(OSThread* thread) {
    /* The port runs no libultra threads; 0 is the id of the (absent) one. */
    return 0;
}

OSPri OoT_osGetThreadPri(OSThread* thread) {
    /* OS_PRIORITY_IDLE. Not the tail priority (-1): callers compare against
     * that to detect the end of a thread list, never to describe a thread. */
    return OS_PRIORITY_IDLE;
}

void OoT_osSetThreadPri(OSThread* thread, OSPri pri) {
}

void OoT_osCreatePiManager(OSPri pri, OSMesgQueue* cmdQ, OSMesg* cmdBuf, s32 cmdMsgCnt) {
}

/* Controller Pak (PFS) family. There is no memory pak on a PC, so every entry
 * point reports PFS_ERR_NOPACK — the code libultra returns when nothing is
 * plugged in, and the one callers already handle. Deliberately NOT 0: success
 * would tell the game a pak exists and send it on to read/write it. This also
 * preserves the pre-#385 de-facto behaviour, where the garbage in the return
 * register was almost always non-zero and so read as "some error". */

s32 OoT_osPfsFreeBlocks(OSPfs* pfs, s32* leftoverBytes) {
    return PFS_ERR_NOPACK;
}

s32 OoT_osEPiWriteIo(OSPiHandle* handle, u32 devAddr, u32 data) {
    /* PI writes are emulated away; report the write as accepted. */
    return 0;
}

s32 OoT_osPfsReadWriteFile(OSPfs* pfs, s32 fileNo, u8 flag, s32 offset, ptrdiff_t size, u8* data) {
    return PFS_ERR_NOPACK;
}

s32 OoT_osPfsDeleteFile(OSPfs* pfs, u16 companyCode, u32 gameCode, u8* gameName, u8* extName) {
    return PFS_ERR_NOPACK;
}

s32 OoT_osPfsFileState(OSPfs* pfs, s32 fileNo, OSPfsState* state) {
    return PFS_ERR_NOPACK;
}

s32 OoT_osPfsInitPak(OSMesgQueue* mq, OSPfs* pfs, s32 channel) {
    return PFS_ERR_NOPACK;
}

s32 __osPfsCheckRamArea(OSPfs* pfs) {
    return PFS_ERR_NOPACK;
}

s32 OoT_osPfsChecker(OSPfs* pfs) {
    return PFS_ERR_NOPACK;
}

s32 OoT_osPfsFindFile(OSPfs* pfs, u16 companyCode, u32 gameCode, u8* gameName, u8* extName, s32* fileNo) {
    return PFS_ERR_NOPACK;
}

s32 OoT_osPfsAllocateFile(OSPfs* pfs, u16 companyCode, u32 gameCode, u8* gameName, u8* extName, s32 length,
                          s32* fileNo) {
    return PFS_ERR_NOPACK;
}

OSIntMask osSetIntMask(OSIntMask a) {
    return 0;
}

s32 OoT_osAfterPreNMI(void) {
    return 0;
}

s32 osProbeRumblePak(OSMesgQueue* ctrlrqueue, OSPfs* pfs, u32 channel) {
    /* Rumble is driven by libultraship/SDL, not by a probed Rumble Pak. Report
     * "no pack" so the N64 probe path stays out of it; osSetRumble below is the
     * entry point that actually works. */
    return PFS_ERR_NOPACK;
}

s32 osSetRumble(OSPfs* pfs, u32 vibrate) {
    return 0;
}

void osCreateThread(OSThread* thread, OSId id, void (*entry)(void*), void* arg, void* sp, OSPri pri) {
}

void OoT_osStartThread(OSThread* thread) {
}

void OoT_osStopThread(OSThread* thread) {
}

void OoT_osDestroyThread(OSThread* thread) {
}

/* osWritebackDCache / osInvalICache removed — see the note by osInvalDCache
 * below. libultraship provides identical no-ops for the whole family. */

s32 OoT_osContStartQuery(OSMesgQueue* mq) {
    /* 0 == query issued, matching games/oot/src/libultra/io/contquery.c. The
     * caller in padsetup.c is `if (osContStartQuery(mq) != 0) return -1;`, so
     * the old garbage return could abort controller setup at random. */
    return 0;
}

void OoT_osContGetQuery(OSContStatus* data) {
}

u32 __osGetFpcCsr() {
    return 0;
}

void __osSetFpcCsr(u32 a0) {
}

s32 __osDisableInt(void) {
    /* There are no RCP interrupts to mask; 0 is the "previous mask" that the
     * paired no-op __osRestoreInt below will be handed back. */
    return 0;
}

void __osRestoreInt(s32 a0) {
}

/* __osGetActiveQueue removed (#385) — the real implementation now resolves from
 * rsbs/src/libultra/os/getactivequeue.c, whose __osActiveQueue storage lives in
 * rsbs/src/libultra/os/threadqueue.c. It was never a duplicate-symbol error
 * because __osActiveQueue had no definition anywhere in the link, which made
 * getactivequeue.o unpullable and let this file's empty body win by default.
 * That body returned the return register, and fault.c walked it as a thread
 * list. The rsbs version returns a real tail-sentinel head, so the walk
 * terminates immediately instead of dereferencing garbage. */

OSThread* __osGetCurrFaultedThread(void) {
    /* No libultra exception handler runs, so there is never a faulted thread on
     * record. NULL is the documented "none" and both fault handlers check for
     * it — games/mm/src/boot/fault.c:1051 falls back to Fault_FindFaultedThread
     * on NULL, which is exactly the intended path here. */
    return NULL;
}

u32 osMemSize = 1024 * 1024 * 1024;

void Audio_osInvalDCache(void* buf, s32 size) {
}

void Audio_osWritebackDCache(void* mem, s32 size) {
}

s32 OoT_osAiSetFrequency(u32 freq) {
    // this is based off the math from the original method
    /*

    s32 OoT_osAiSetFrequency(u32 frequency) {
        u8 bitrate;
        f32 dacRateF = ((f32)OoT_osViClock / frequency) + 0.5f;
        u32 dacRate = dacRateF;

        if (dacRate < 132) {
            return -1;
        }

        bitrate = (dacRate / 66);
        if (bitrate > 16) {
            bitrate = 16;
        }

        HW_REG(AI_DACRATE_REG, u32) = dacRate - 1;
        HW_REG(AI_BITRATE_REG, u32) = bitrate - 1;
        return OoT_osViClock / (s32)dacRate;
    }

    */

    // bitrate is unused

    // OoT_osViClock comes from
    // #define VI_NTSC_CLOCK 48681812 /* Hz = 48.681812 MHz */
    // s32 OoT_osViClock = VI_NTSC_CLOCK;

    // frequency was originally 32000

    // given all of that, dacRate is
    // (u32)(((f32)48681812 / 32000) + 0.5f)
    // which evaluates to 1521 (which is > 132)

    // this leaves us with a final calculation of
    // 48681812 / 1521
    // which evaluates to 32006

    return 32006;
}

/* RSBS: the osWritebackDCache / osInvalICache / osInvalDCache /
 * osWritebackDCacheAll family is deliberately NOT defined here. libultraship
 * already defines all four as empty no-ops
 * (libultraship/src/libultraship/libultra/os_cache.cpp), byte-for-byte
 * equivalent to what this file used to declare, so these were duplicate
 * definitions of a symbol two archives both export.
 *
 * They were latent until phase 2: archive members are pulled in lazily, so
 * the collision only became a link error once the new single-exe TUs dragged
 * both soh_port's stubs.c.o and libultraship's os_cache.cpp.o into the same
 * link. MSVC never complained because the Windows link is /FORCE:MULTIPLE by
 * design; GNU ld rejected it, so build-windows passed while build-linux
 * failed on the same commit.
 *
 * Note the rest of this file prefixes its port shims OoT_ precisely to avoid
 * this class of collision (OoT_osDestroyThread, OoT_osContStartQuery, ...).
 * These four were the ones that got missed. Both games' unprefixed callers —
 * including MM's games/mm/src/audio/lib/dcache.c:7,14 — now resolve to
 * libultraship's no-ops, which is the intended platform-layer behavior. */

void Audio_SetBGM(u32 bgmId) {
}

s32 OoT_osContSetCh(u8 ch) {
    /* 0 == channel count accepted, per libultra. */
    return 0;
}

u32 OoT_osDpGetStatus(void) {
    /* No RDP status register to read; all status bits clear. */
    return 0;
}

void OoT_osDpSetStatus(u32 status) {
}

u32 __osSpGetStatus() {
    /* No RSP status register to read; all status bits clear. Callers poll this
     * for DMA_BUSY / IO_BUSY, so 0 ("idle") is the value that lets them
     * proceed — garbage could have read as permanently busy. */
    return 0;
}

void __osSpSetStatus(u32 status) {
}

OSPiHandle* OoT_osDriveRomInit() {
    /* There is no cart PI handle in the port. NULL is the libultra "no device"
     * return; the old garbage pointer would have been dereferenced as one. */
    return NULL;
}

void __osInitialize_common(void) {
}

void __osInitialize_autodetect(void) {
}

void __osExceptionPreamble() {
}

void __osCleanupThread(void) {
}

s32 _Printf(PrintCallback a, void* arg, const char* fmt, va_list ap) {
    unsigned char buffer[4096];
    int written;
    size_t len;

    /* #385: this one had a body, so it survived the empty-body sweep, but it
     * still fell off the end — and _Printf's return value is not decorative.
     * games/oot/src/libultra/libc/sprintf.c returns it verbatim as sprintf's
     * character count, so every sprintf() in the port was handing its caller
     * the return register. libultra contracts _Printf to return the number of
     * characters emitted, or -1 on failure. */
    written = vsnprintf((char*)buffer, sizeof(buffer), fmt, ap);
    if (written < 0) {
        return -1;
    }

    /* vsnprintf reports what it WOULD have written; report what actually
     * landed in the buffer, so a truncating format does not claim more
     * characters than the callback ever saw. */
    len = strlen((const char*)buffer);
    a(arg, buffer, (s32)len);
    return (s32)len;
}

void OoT_osSpTaskLoad(OSTask* task) {
}

void OoT_osSpTaskStartGo(OSTask* task) {
}

void osSetUpMempakWrite(s32 channel, OSPifRam* buf) {
}

u32 OoT_osGetMemSize(void) {
    return 1024 * 1024 * 1024;
}

s32 OoT_osEPiReadIo(OSPiHandle* handle, u32 devAddr, u32* data) {
    return 0;
}

void OoT_osSpTaskYield(void) {
}

s32 OoT_osStopTimer(OSTimer* timer) {
    /* 0 == the timer was not on the active list, which is always true here:
     * OoT_osSetTimer is not wired up either. */
    return 0;
}

OSYieldResult OoT_osSpTaskYielded(OSTask* task) {
    /* TEMPORARY: deliberately emptied body (dropped `return 0;`) to prove the
     * #401 -Werror=return-type / /we4716 /we4715 escalation on soh/stubs.c
     * actually fails the build. Will be reverted immediately after the red
     * run is observed. See #392 / #401 / #421. */
}

void OoT_osViExtendVStart(u32 arg0) {
}
