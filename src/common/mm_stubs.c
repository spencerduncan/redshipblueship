/**
 * MM Stubs and Aliases for Single Executable Build
 *
 * This file provides:
 * 1. Aliases for MM_ prefixed functions to their non-prefixed versions
 * 2. Stub implementations for enhancement layer functions that were excluded
 */

#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>

/* ==========================================================================
 * MM_ -> base function aliases (libultra/libc functions)
 * These were renamed by the namespace tool but should use OoT's implementations
 * ========================================================================== */

/* Math functions */
float MM_sqrtf(float x) { return sqrtf(x); }

/* String functions */
int MM_sprintf(char *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsprintf(str, format, args);
    va_end(args);
    return ret;
}

/* OS functions - these should use OoT's implementations */
/* Forward declarations from OoT */
extern void osContGetReadData(void *data);
extern int osContInit(void *mq, unsigned char *bitpattern, void *status);
extern int osContStartReadData(void *mq);
extern void osCreateMesgQueue(void *mq, void *msg, int count);
extern uint64_t osGetTime(void);
extern int osMotorInit(void *mq, void *pfs, int channel);
extern int osRecvMesg(void *mq, void *msg, int flag);
extern int osSendMesg(void *mq, void *msg, int flag);
extern void osSetEventMesg(int event, void *mq, void *msg);
extern void osViBlack(int flag);
extern void *osViGetCurrentFramebuffer(void);
extern void *osViGetNextFramebuffer(void);
extern void osViSetMode(void *mode);
extern void osViSetSpecialFeatures(unsigned int func);
extern void osViSetXScale(float scale);
extern void osViSetYScale(float scale);
extern void osViSwapBuffer(void *buffer);

void MM_osContGetReadData(void *data) { osContGetReadData(data); }
int MM_osContInit(void *mq, unsigned char *bitpattern, void *status) { return osContInit(mq, bitpattern, status); }
int MM_osContStartReadData(void *mq) { return osContStartReadData(mq); }
void MM_osCreateMesgQueue(void *mq, void *msg, int count) { osCreateMesgQueue(mq, msg, count); }
uint64_t MM_osGetTime(void) { return osGetTime(); }
int MM_osMotorInit(void *mq, void *pfs, int channel) { return osMotorInit(mq, pfs, channel); }
int MM_osRecvMesg(void *mq, void *msg, int flag) { return osRecvMesg(mq, msg, flag); }
int MM_osSendMesg(void *mq, void *msg, int flag) { return osSendMesg(mq, msg, flag); }
void MM_osSetEventMesg(int event, void *mq, void *msg) { osSetEventMesg(event, mq, msg); }
void MM_osViBlack(int flag) { osViBlack(flag); }
void *MM_osViGetCurrentFramebuffer(void) { return osViGetCurrentFramebuffer(); }
void *MM_osViGetNextFramebuffer(void) { return osViGetNextFramebuffer(); }
void MM_osViSetMode(void *mode) { osViSetMode(mode); }
void MM_osViSetSpecialFeatures(unsigned int func) { osViSetSpecialFeatures(func); }
void MM_osViSetXScale(float scale) { osViSetXScale(scale); }
void MM_osViSetYScale(float scale) { osViSetYScale(scale); }
void MM_osViSwapBuffer(void *buffer) { osViSwapBuffer(buffer); }

/* FaultDrawer - stub implementations (OoT uses different fault handling) */
int MM_FaultDrawer_DrawText(int x, int y, const char* fmt, ...) {
    (void)x; (void)y; (void)fmt;
    return 0;
}

int MM_FaultDrawer_Printf(const char* fmt, ...) {
    (void)fmt;
    return 0;
}

void MM_FaultDrawer_SetCharPad(int xPad, int yPad) { (void)xPad; (void)yPad; }

/* ==========================================================================
 * AudioLoad stubs - these need real implementations later
 * For now, stub them out to allow linking
 * ========================================================================== */

void MM_AudioLoad_Init(void* pool, void* data) { (void)pool; (void)data; }
void MM_AudioLoad_InitAsyncLoads(void) {}
void MM_AudioLoad_InitSampleDmaBuffers(int num) { (void)num; }
void MM_AudioLoad_InitScriptLoads(void) {}
void MM_AudioLoad_InitSlowLoads(void) {}
void MM_AudioLoad_SetDmaHandler(void* handler) { (void)handler; }
void MM_AudioLoad_DecreaseSampleDmaTtls(void) {}
void MM_AudioLoad_ProcessLoads(void) {}
void MM_AudioLoad_ProcessScriptLoads(void) {}
int MM_AudioLoad_IsFontLoadComplete(int fontId) { (void)fontId; return 1; }
int MM_AudioLoad_IsSeqLoadComplete(int seqId) { (void)seqId; return 1; }
void MM_AudioLoad_SetFontLoadStatus(int fontId, int status) { (void)fontId; (void)status; }
void MM_AudioLoad_SetSeqLoadStatus(int seqId, int status) { (void)seqId; (void)status; }
void* MM_AudioLoad_GetFontsForSequence(int seqId, void* out) { (void)seqId; (void)out; return NULL; }
void MM_AudioLoad_DiscardSeqFonts(int seqId) { (void)seqId; }
int MM_AudioLoad_SyncInitSeqPlayer(int player, int seqId, int arg2) { (void)player; (void)seqId; (void)arg2; return 0; }
int MM_AudioLoad_SyncInitSeqPlayerSkipTicks(int player, int seqId, int ticks) { (void)player; (void)seqId; (void)ticks; return 0; }
void* MM_AudioLoad_SyncLoadInstrument(int fontId, int instId, int drumId) { (void)fontId; (void)instId; (void)drumId; return NULL; }
void MM_AudioLoad_AsyncLoadFont(int fontId, int param, void* retMsg, void* msgQueue) { (void)fontId; (void)param; (void)retMsg; (void)msgQueue; }
void MM_AudioLoad_AsyncLoadSampleBank(int bankId, int param, void* retMsg, void* msgQueue) { (void)bankId; (void)param; (void)retMsg; (void)msgQueue; }
void MM_AudioLoad_AsyncLoadSeq(int seqId, int param, void* retMsg, void* msgQueue) { (void)seqId; (void)param; (void)retMsg; (void)msgQueue; }
void MM_AudioLoad_LoadPermanentSamples(void) {}
void MM_AudioLoad_ScriptLoad(int type, int id, void* retMsg, void* msgQueue) { (void)type; (void)id; (void)retMsg; (void)msgQueue; }
void MM_AudioLoad_SyncLoadSeqParts(int seqId, int param, void* dst, void* bankDst) { (void)seqId; (void)param; (void)dst; (void)bankDst; }
void MM_AudioLoad_SlowLoadSample(int fontId, int instId, void* dst) { (void)fontId; (void)instId; (void)dst; }
void MM_AudioLoad_SlowLoadSeq(int seqId, void* dst, void* retMsg) { (void)seqId; (void)dst; (void)retMsg; }

/* ==========================================================================
 * Enhancement layer stubs - these are excluded in single-exe mode
 * ========================================================================== */

/* GameInteractor stubs */
void GameInteractor_ExecuteOnActorDraw(void* actor) { (void)actor; }
void GameInteractor_ExecuteOnGameStateUpdate(void* state) { (void)state; }
void GameInteractor_ExecuteOnGameStateMainFinish(void* state) { (void)state; }
void GameInteractor_ExecuteOnGameStateDrawFinish(void* state) { (void)state; }
void GameInteractor_ExecuteOnPlayDrawWorldEnd(void* play) { (void)play; }
void GameInteractor_ExecuteOnInterfaceDrawStart(void* play) { (void)play; }
void GameInteractor_ExecuteBeforeKaleidoDrawPage(void* state, int page) { (void)state; (void)page; }
void GameInteractor_ExecuteAfterKaleidoDrawPage(void* state, int page) { (void)state; (void)page; }
void GameInteractor_ExecuteOnSaveInit(int fileNum) { (void)fileNum; }
void GameInteractor_ExecuteOnSaveLoad(int fileNum) { (void)fileNum; }
void GameInteractor_ExecuteOnOpenText(int textId) { (void)textId; }
void GameInteractor_ExecuteOnItemGive(int itemId) { (void)itemId; }
int GameInteractor_ShouldItemGive(int itemId) { (void)itemId; return 1; }
int GameInteractor_ShouldActorDraw(void* actor) { (void)actor; return 1; }
void GameInteractor_ExecuteOnCameraChangeModeFlags(void* camera) { (void)camera; }
void GameInteractor_ExecuteOnCameraChangeSettingsFlags(void* camera) { (void)camera; }
void GameInteractor_ExecuteAfterCameraUpdate(void* camera) { (void)camera; }
void GameInteractor_ExecuteOnPassPlayerInputs(void* input) { (void)input; }
void GameInteractor_ExecuteOnPlayerPostLimbDraw(void* player, int limbIndex) { (void)player; (void)limbIndex; }
void GameInteractor_ExecuteOnBossDefeated(int bossId) { (void)bossId; }
void GameInteractor_ExecuteOnBottleContentsUpdate(int slotId) { (void)slotId; }
void GameInteractor_ExecuteOnConsoleLogoUpdate(void) {}
void GameInteractor_ExecuteOnFileSelectSaveLoad(void* state, int fileNum) { (void)state; (void)fileNum; }
void GameInteractor_ExecuteOnGameCompletion(void) {}
void GameInteractor_ExecuteBeforeEndOfCycleSave(void) {}
void GameInteractor_ExecuteAfterEndOfCycleSave(void) {}
void GameInteractor_ExecuteBeforeMoonCrashSaveReset(void) {}
void GameInteractor_ExecuteBeforeInterfaceClockDraw(void) {}
void GameInteractor_ExecuteAfterInterfaceClockDraw(void) {}
int GameInteractor_Dpad(void* input, int dpad) { (void)input; return dpad; }
int GameInteractor_InvertControl(int control) { return control; }
int GameInteractor_RightStickOcarina(void* input) { (void)input; return 0; }

/* HudEditor stubs */
void* hudEditorElements = NULL;
int hudEditorActiveElement = 0;

void HudEditor_SetActiveElement(int element) { (void)element; }
int HudEditor_ShouldOverrideDraw(void) { return 0; }
float HudEditor_GetActiveElementScale(void) { return 1.0f; }
int HudEditor_IsActiveElementHidden(void) { return 0; }
void HudEditor_ModifyDrawValues(float* x, float* y, float* scale) { (void)x; (void)y; (void)scale; }
void HudEditor_ModifyDrawValuesFromBase(float* x, float* y, float* scale, float bx, float by) { (void)x; (void)y; (void)scale; (void)bx; (void)by; }
void HudEditor_ModifyMatrixValues(float* x, float* y, float* scale) { (void)x; (void)y; (void)scale; }
void HudEditor_ModifyRectPosValues(int* x, int* y) { (void)x; (void)y; }
void HudEditor_ModifyRectPosValuesFromBase(int* x, int* y, int bx, int by) { (void)x; (void)y; (void)bx; (void)by; }
void HudEditor_ModifyRectSizeValues(int* w, int* h) { (void)w; (void)h; }
void HudEditor_ModifyTextureStepValues(int* x, int* y) { (void)x; (void)y; }
void HudEditor_ModifyKaleidoEquipAnimValues(float* x, float* y, float* scale) { (void)x; (void)y; (void)scale; }

/* Graphics override stubs */
void gDPSetPrimColorOverride(void* dl, int m, int l, int r, int g, int b, int a) { (void)dl; (void)m; (void)l; (void)r; (void)g; (void)b; (void)a; }
void gDPSetEnvColorOverride(void* dl, int r, int g, int b, int a) { (void)dl; (void)r; (void)g; (void)b; (void)a; }
void Gfx_DrawRect_DropShadowOverride(void* dl, int x, int y, int w, int h) { (void)dl; (void)x; (void)y; (void)w; (void)h; }
void Gfx_DrawTexRectIA8_DropShadowOverride(void* dl) { (void)dl; }
void Gfx_DrawTexRectIA8_DropShadowOffsetOverride(void* dl) { (void)dl; }
void Gfx_DrawTexRectIA16_DropShadowOverride(void* dl) { (void)dl; }

/* FrameInterpolation stubs */
void FrameInterpolation_IgnoreActorMtx(void* actor) { (void)actor; }
void FrameInterpolation_InterpolateWiderAngles(int wider) { (void)wider; }

/* Ship enhancement stubs */
int Ship_GetInterpolationFPS(void) { return 20; }
const char* Ship_GetSceneName(int sceneId) { (void)sceneId; return "Unknown"; }
void Ship_HandleConsoleCrashAsReset(void) {}
void Ship_ExtendedCullingActorRestoreProjectedPos(void* actor) { (void)actor; }

/* Motion blur stub */
int MotionBlur_Override(void* dl) { (void)dl; return 0; }

/* Resource manager functions are provided by BenPort.cpp */

/* SaveManager stubs */
int SaveManager_SysFlashrom_ReadData(void* dst, int page, int count) { (void)dst; (void)page; (void)count; return 0; }
int SaveManager_SysFlashrom_WriteData(void* src, int page, int count) { (void)src; (void)page; (void)count; return 0; }

/* SavingEnhancements stubs */
int SavingEnhancements_GetSaveEntrance(void) { return 0; }
void SavingEnhancements_AdvancePlaytime(void) {}

/* PauseOwlWarp stub */
int PauseOwlWarp_IsOwlWarpEnabled(void) { return 0; }

/* Combo stubs */
int Combo_CheckEntranceSwitch(void) { return 0; }
int Combo_CheckHotSwap(void) { return 0; }

/* OTR stubs */
float OTRConvertHUDXToScreenX(float x) { return x; }
void OTRPlay_InitScene(void* play) { (void)play; }

/* AudioEditor stub */
void* AudioEditor_GetOriginalSeq(int seqId) { (void)seqId; return NULL; }

/* Global variables */
int gAudioCtxInitalized = 0;
int currentActorListIndex = 0;

/* Sequence/Font map stubs */
void* gSequenceMap = NULL;
int gSequenceMapSize = 0;
void* gFontMap = NULL;
int gFontMapSize = 0;

/* ZAPD main stub (not needed at runtime) */
int zapd_main(int argc, char** argv) { (void)argc; (void)argv; return 0; }

/* func_8018FA60 - MM-specific function, stub for now */
void func_8018FA60(void* a, int b, int c) { (void)a; (void)b; (void)c; }
