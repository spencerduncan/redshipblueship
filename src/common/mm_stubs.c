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

/* Graphics override + CosmeticEditor wrappers: implemented for real in
 * games/mm/2s2h/CosmeticGfxSingleExe.cpp, compiled against the declarations
 * in 2s2h/BenGui/CosmeticEditor.h. Do NOT re-stub them here: the wrappers
 * RETURN the advanced display-list pointer, and the void placeholder stubs
 * that used to live in this file made MM's HUD draw consume garbage as its
 * gfx write pointer (WRITE access violation at 0xA7 in
 * MM_Interface_DrawItemButtons on the first HUD-visible MM frame — caught
 * by int-gameplay-roundtrip, locked ROM-free by the cosmetic-gfx-stub
 * test). */

/* FrameInterpolation stubs */
void FrameInterpolation_IgnoreActorMtx(void* actor) { (void)actor; }
void FrameInterpolation_InterpolateWiderAngles(int wider) { (void)wider; }

/* Ship enhancement stubs */
/* Ship_GetInterpolationFPS is now defined for real in games/oot/soh/OTRGlobals.cpp
 * (extern "C") as part of the scrolling-texture-interpolation port (#234). */
const char* Ship_GetSceneName(int sceneId) { (void)sceneId; return "Unknown"; }
void Ship_HandleConsoleCrashAsReset(void) {}
void Ship_ExtendedCullingActorRestoreProjectedPos(void* actor) { (void)actor; }

/* The MotionBlur_Override / SavingEnhancements_* / PauseOwlWarp_* stubs that
 * used to live here are gone. Their real implementations
 * (2s2h/Enhancements/Graphics/MotionBlur.cpp,
 * Enhancements/Saving/SavingEnhancements.cpp,
 * Enhancements/Songs/PauseOwlWarp.cpp) are compiled into 2ship_enh and ARE
 * part of the single-exe link, so these were duplicate definitions.
 *
 * MSVC hid that: the Windows link uses /FORCE:MULTIPLE (CMakeLists.txt:274)
 * by design, because the two ports legitimately share symbol names. GNU ld
 * has no such flag here and is therefore the STRICTER gate — it rejected the
 * duplicates outright ("multiple definition of `MotionBlur_Override'"), which
 * is how this surfaced: build-windows passed and build-linux failed on the
 * same commit. That asymmetry is a feature, not a nuisance; do not paper over
 * it with --allow-multiple-definition.
 *
 * Deleting the stubs means the real enhancements now run instead of no-ops.
 * That is the intended behavior and is default-inert: MotionBlur_Override
 * returns early unless the MotionBlur.Mode CVar is set, matching what the
 * stub did. The stub was also the wrong shape — `int(void*)` against a real
 * `void(u8*, s32*)` (Graphics.h:11), the same signature-drift class tracked
 * in #379; the sole caller (games/mm/src/code/z_play.c:107) was always
 * written against the real two-out-param signature. */

/* Resource manager functions are provided by BenPort.cpp */

/* SaveManager stubs */
int SaveManager_SysFlashrom_ReadData(void* dst, int page, int count) { (void)dst; (void)page; (void)count; return 0; }
int SaveManager_SysFlashrom_WriteData(void* src, int page, int count) { (void)src; (void)page; (void)count; return 0; }

/* Combo_CheckEntranceSwitch and Combo_CheckHotSwap are now in
 * GameExports_SingleExe.cpp (they need real cross-game logic) */

/* OTR stubs */
/* OTRConvertHUDXToScreenX used to be stubbed here as float(float) while the
 * real signature (2s2h/BenPort.h, and every caller in z_parameter.c) is
 * int32_t(int32_t) — a silent ABI mismatch that collapsed MM's A-button
 * viewport and the three-day-clock scissors. It now has a header-checked
 * implementation in games/mm/2s2h/BenPortHudSingleExe.cpp. */
/* The OTRPlay_InitScene no-op stub that used to live here is gone (issue #344):
 * MM's real scene-init glue is now compiled as MM_OTRPlay_InitScene in
 * games/mm/2s2h/z_play_2SH.cpp. */

/* The AudioEditor_GetOriginalSeq stub that used to live here is gone: MM's
 * audio-editor entry points are MM_-prefixed now (games/mm/include/
 * mm_audio_prefix.h) and identity-stubbed with header-checked u16(u16)
 * signatures in games/mm/2s2h/GameExports_SingleExe.cpp. The untyped stub
 * here returned 0 for every seqId, funneling MM's seq-load-status writes
 * into slot 0. */

/* Global variables */
int currentActorListIndex = 0;

/* The zapd_main stub that used to live here is gone (issue #325): it shadowed
 * the real ZAPDLib entry points in the link and silently broke in-app ROM
 * extraction. Extraction now spawns the bundled standalone ZAPD executable
 * (games/oot/soh/Extractor/Extract.cpp), so nothing references zapd_main in
 * single-exe builds. */
