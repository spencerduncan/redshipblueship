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

#include <libultraship/color.h>

/* ==========================================================================
 * MM_ -> base function aliases (libultra/libc functions)
 * These were renamed by the namespace tool but should use OoT's implementations
 * ========================================================================== */

/* Math functions - sqrtf was renamed by namespace tool, redirect to libc */
float MM_sqrtf(float x) { return sqrtf(x); }

/* String functions */
int MM_sprintf(char *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsprintf(str, format, args);
    va_end(args);
    return ret;
}

/* OS function aliases: MM_ prefixed calls redirect to shared libultraship
 * implementations. The namespace tool renamed MM's calls from os* to MM_os*,
 * but the actual implementations live in libultraship (shared). These aliases
 * bridge the gap.
 *
 * Note: osVi* functions are NOT aliased here because MM has its own
 * implementations in src/libultra/io/ that were renamed to MM_osVi*. */
extern void osContGetReadData(void *data);
extern int osContInit(void *mq, unsigned char *bitpattern, void *status);
extern int osContStartReadData(void *mq);
extern void osCreateMesgQueue(void *mq, void *msg, int count);
extern uint64_t osGetTime(void);
extern int osMotorInit(void *mq, void *pfs, int channel);
extern int osRecvMesg(void *mq, void *msg, int flag);
extern int osSendMesg(void *mq, void *msg, int flag);
extern void osSetEventMesg(int event, void *mq, void *msg);

void MM_osContGetReadData(void *data) { osContGetReadData(data); }
int MM_osContInit(void *mq, unsigned char *bitpattern, void *status) { return osContInit(mq, bitpattern, status); }
int MM_osContStartReadData(void *mq) { return osContStartReadData(mq); }
void MM_osCreateMesgQueue(void *mq, void *msg, int count) { osCreateMesgQueue(mq, msg, count); }
uint64_t MM_osGetTime(void) { return osGetTime(); }
int MM_osMotorInit(void *mq, void *pfs, int channel) { return osMotorInit(mq, pfs, channel); }
int MM_osRecvMesg(void *mq, void *msg, int flag) { return osRecvMesg(mq, msg, flag); }
int MM_osSendMesg(void *mq, void *msg, int flag) { return osSendMesg(mq, msg, flag); }
void MM_osSetEventMesg(int event, void *mq, void *msg) { osSetEventMesg(event, mq, msg); }

/* osCartRomInit - MM's libultra/io is excluded; audio load.c calls this but
 * it's a no-op in the port (ROM cart access is replaced by archive loading) */
void* MM_osCartRomInit(void) { return NULL; }

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
 * Enhancement layer stubs - these are excluded in single-exe mode
 * ========================================================================== */

/* GameInteractor stubs.
 *
 * These cover MM's GameInteractor_* references that no linked TU defines. A
 * second set of MM references (GameInteractor_Should, ExecuteOnFlagSet,
 * ExecuteOnActorUpdate, ...) resolves to OoT's extern "C" wrappers in
 * games/oot/soh/Enhancements/game-interactor/GameInteractor_Hooks.cpp instead
 * — that is NOT a valid provider: MM's GIVanillaBehavior ordinals alias OoT's
 * (MM VB_SETUP_TRANSITION == OoT VB_PLAY_RAINBOW_BRIDGE_CS == 206), so routing
 * MM calls into OoT's hook registry runs OoT handlers against MM state. Those
 * wrappers therefore gate on Context_GetCurrentGame() and return vanilla
 * behavior while MM is active, making them equivalent to the stubs below. */
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
/* GameInteractor_ExecuteOnOpenText resolves to OoT's wrapper in
 * games/oot/soh/Enhancements/game-interactor/GameInteractor_Hooks.cpp
 * (signature (uint16_t* textId, bool* loadFromMessageTable), #228). That
 * wrapper no-ops while MM is the active game (see the header comment above),
 * so MM text boxes never fire OoT text hooks. */
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

/* CosmeticEditor stub: returns the input color unchanged since the
 * editor UI is excluded in single-exe builds. Uses the shared Color_RGBA8
 * typedef from <libultraship/color.h> so the stub ABI matches the declaration
 * in games/mm/2s2h/BenGui/CosmeticEditor.h exactly. */
Color_RGBA8 CosmeticEditor_GetChangedColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t elementId) {
    (void)elementId;
    Color_RGBA8 c = { r, g, b, a };
    return c;
}

/* FrameInterpolation stubs */
void FrameInterpolation_IgnoreActorMtx(void* actor) { (void)actor; }
void FrameInterpolation_InterpolateWiderAngles(int wider) { (void)wider; }

/* Ship enhancement stubs */
/* Ship_GetInterpolationFPS is now defined for real in games/oot/soh/OTRGlobals.cpp
 * (extern "C") as part of the scrolling-texture-interpolation port (#234). */
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

/* Combo_CheckEntranceSwitch and Combo_CheckHotSwap are now in
 * GameExports_SingleExe.cpp (they need real cross-game logic) */

/* OTR stubs */
float OTRConvertHUDXToScreenX(float x) { return x; }
/* The OTRPlay_InitScene no-op stub that used to live here is gone (issue #344):
 * MM's real scene-init glue is now compiled as MM_OTRPlay_InitScene in
 * games/mm/2s2h/z_play_2SH.cpp. */

/* AudioEditor stub */
void* AudioEditor_GetOriginalSeq(int seqId) { (void)seqId; return NULL; }

/* Global variables */
int currentActorListIndex = 0;

/* The zapd_main stub that used to live here is gone (issue #325): it shadowed
 * the real ZAPDLib entry points in the link and silently broke in-app ROM
 * extraction. Extraction now spawns the bundled standalone ZAPD executable
 * (games/oot/soh/Extractor/Extract.cpp), so nothing references zapd_main in
 * single-exe builds. */
