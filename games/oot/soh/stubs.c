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

OSId OoT_osGetThreadId(OSThread* thread) {
}

OSPri OoT_osGetThreadPri(OSThread* thread) {
}

void OoT_osSetThreadPri(OSThread* thread, OSPri pri) {
}

void OoT_osCreatePiManager(OSPri pri, OSMesgQueue* cmdQ, OSMesg* cmdBuf, s32 cmdMsgCnt) {
}

s32 OoT_osPfsFreeBlocks(OSPfs* pfs, s32* leftoverBytes) {
}

s32 OoT_osEPiWriteIo(OSPiHandle* handle, u32 devAddr, u32 data) {
}

s32 OoT_osPfsReadWriteFile(OSPfs* pfs, s32 fileNo, u8 flag, s32 offset, ptrdiff_t size, u8* data) {
}

s32 OoT_osPfsDeleteFile(OSPfs* pfs, u16 companyCode, u32 gameCode, u8* gameName, u8* extName) {
}

s32 OoT_osPfsFileState(OSPfs* pfs, s32 fileNo, OSPfsState* state) {
}

s32 OoT_osPfsInitPak(OSMesgQueue* mq, OSPfs* pfs, s32 channel) {
}

s32 __osPfsCheckRamArea(OSPfs* pfs) {
}

s32 OoT_osPfsChecker(OSPfs* pfs) {
}

s32 OoT_osPfsFindFile(OSPfs* pfs, u16 companyCode, u32 gameCode, u8* gameName, u8* extName, s32* fileNo) {
}

s32 OoT_osPfsAllocateFile(OSPfs* pfs, u16 companyCode, u32 gameCode, u8* gameName, u8* extName, s32 length, s32* fileNo) {
}

OSIntMask osSetIntMask(OSIntMask a) {
    return 0;
}

s32 OoT_osAfterPreNMI(void) {
    return 0;
}

s32 osProbeRumblePak(OSMesgQueue* ctrlrqueue, OSPfs* pfs, u32 channel) {
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

void osWritebackDCache(void* vaddr, s32 nbytes) {
}

void osInvalICache(void* vaddr, s32 nbytes) {
}

s32 OoT_osContStartQuery(OSMesgQueue* mq) {
}

void OoT_osContGetQuery(OSContStatus* data) {
}

u32 __osGetFpcCsr() {
    return 0;
}

void __osSetFpcCsr(u32 a0) {
}

s32 __osDisableInt(void) {
}

void __osRestoreInt(s32 a0) {
}

OSThread* __osGetActiveQueue(void) {
}

OSThread* __osGetCurrFaultedThread(void) {
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

void osInvalDCache(void* vaddr, s32 nbytes) {
}

void osWritebackDCacheAll(void) {
}

void Audio_SetBGM(u32 bgmId) {
}

s32 OoT_osContSetCh(u8 ch) {
}

u32 OoT_osDpGetStatus(void) {
}

void OoT_osDpSetStatus(u32 status) {
}

u32 __osSpGetStatus() {
}

void __osSpSetStatus(u32 status) {
}

OSPiHandle* OoT_osDriveRomInit() {
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

    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    a(arg, buffer, strlen(buffer));
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
}

OSYieldResult OoT_osSpTaskYielded(OSTask* task) {
}

void OoT_osViExtendVStart(u32 arg0) {
}
