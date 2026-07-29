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
 * second set of MM references (ExecuteOnFlagSet, ExecuteOnActorUpdate, ...)
 * resolves to OoT's extern "C" wrappers in
 * games/oot/soh/Enhancements/game-interactor/GameInteractor_Hooks.cpp instead
 * — that is NOT a valid provider: MM's GIVanillaBehavior ordinals alias OoT's
 * (MM VB_SETUP_TRANSITION == OoT VB_PLAY_RAINBOW_BRIDGE_CS == 206), so routing
 * MM calls into OoT's hook registry runs OoT handlers against MM state. Those
 * wrappers therefore gate on Context_GetCurrentGame() and return vanilla
 * behavior while MM is active, making them equivalent to the stubs below.
 *
 * The "Should" family (GameInteractor_Should, ShouldActorInit,
 * ShouldActorUpdate, ShouldActorDraw, ShouldItemGive) is no longer in either
 * bucket (#392 VB follow-up): the single-exe macro rebind at the bottom of
 * MM's GameInteractor.h renames every MM call site to the MM-owned
 * MM_GameHooks_Execute* dispatchers in games/mm/2s2h/GameExports_SingleExe.cpp,
 * which consult only the S2H::GameHooks registries — rando/enhancement VB
 * overrides fire on MM frames without MM ids ever reaching OoT's tables.
 * Re-adding Should stubs here would silently sever that dispatch. */
/* GameInteractor_ExecuteOnActorDraw / ExecuteOnActorInit / ExecuteOnOpenText
 * moved to real, header-checked dispatch in
 * games/mm/2s2h/GameExports_SingleExe.cpp (#438), reached through the
 * single-exe macro rebind at the bottom of MM's GameInteractor.h. This stub
 * was the reason the draw pair looked wired while doing nothing: 21 TUs
 * register ShouldActorInit and 24 register OnOpenText through the COND_*
 * macros, all of which park in S2H::GameHooks, and none of which this no-op
 * ever consulted. Re-stubbing any of the three here would silently sever that
 * dispatch again — chest models and every rando text override go back to
 * vanilla with no diagnostic. */
/* GameInteractor_ExecuteOnGameStateUpdate / ExecuteOnGameStateDrawFinish moved
 * to real, header-checked dispatch in games/mm/2s2h/GameExports_SingleExe.cpp
 * (#442): MM's own frame loop (games/mm/src/code/game.c MM_GameState_Update)
 * already calls both at the right points every frame — the "pump" upstream
 * 2S2H used — so once SavingEnhancements.cpp's raw registrations for these
 * two hook types moved onto S2H::GameHooks, leaving these as no-ops would
 * have kept autosave (OnGameStateUpdate: HandleAutoSave) and its owl-save
 * icon (OnGameStateDrawFinish: DrawAutosaveIcon) permanently dead even though
 * registration itself no longer corrupts memory. Re-stubbing them here would
 * silently sever that dispatch again. */
void GameInteractor_ExecuteOnGameStateMainFinish(void* state) { (void)state; }
void GameInteractor_ExecuteOnPlayDrawWorldEnd(void* play) { (void)play; }
void GameInteractor_ExecuteOnInterfaceDrawStart(void* play) { (void)play; }
/* GameInteractor_ExecuteBeforeKaleidoDrawPage / ExecuteAfterKaleidoDrawPage
 * moved to real, header-checked dispatch in
 * games/mm/2s2h/GameExports_SingleExe.cpp (#438), reached through the
 * single-exe macro rebind at the bottom of MM's GameInteractor.h — wired as a
 * PAIR on purpose, matching how z_kaleido_scope_NES.c brackets every page
 * draw (the same both-or-neither reasoning the EndOfCycleSave pair below
 * records). The After half had a live registrant the whole time:
 * KaleidoItemPage.cpp's COND_ID_HOOK(PAUSE_ITEM) draws the trade-slot cycling
 * arrows and adjacent-item previews, which these no-ops kept invisible while
 * the LIVE VB_KALEIDO_DISPLAY_ITEM_TEXT override still suppressed the vanilla
 * item text — strictly worse than vanilla. Both stubs also carried the
 * #372/#424 signature-drift hazard ((void*, int) against the real
 * (PauseContext*, u16)), retired with them. Re-stubbing either here would
 * silently sever the dispatch again. (MM-only symbols — OoT defines no
 * twins.) */
/* GameInteractor_ExecuteOnSaveInit / GameInteractor_ExecuteOnSaveLoad moved to
 * real, header-checked dispatch in games/mm/2s2h/GameExports_SingleExe.cpp
 * (Lane C1, #392): they now Execute the MM-owned S2H::GameHooks registries
 * (OnSaveInit -> Rando::MiscBehavior::OnFileCreate at MM_Sram_InitSave,
 * OnSaveLoad -> Rando's OnSaveLoadHandler at the file-select/opening loads).
 * Re-stubbing them here would silently sever MM rando generation. */
/* GameInteractor_ExecuteOnOpenText USED to resolve, from MM call sites, to
 * OoT's wrapper in
 * games/oot/soh/Enhancements/game-interactor/GameInteractor_Hooks.cpp
 * (signature (uint16_t* textId, bool* loadFromMessageTable), #228) — which
 * no-ops while MM is the active game, so MM text boxes fired no hooks at all.
 * OoT's own z_message_PAL.c still reaches that wrapper; MM's z_message.c is
 * rebound to MM_GameHooks_ExecuteOnOpenText (#438), see the block above. */
void GameInteractor_ExecuteOnItemGive(int itemId) { (void)itemId; }
/* GameInteractor_ShouldItemGive / GameInteractor_ShouldActorDraw stubs
 * retired (#392 VB follow-up): MM call sites now rebind to the header-checked
 * MM_GameHooks_ExecuteShouldItemGive / MM_GameHooks_ExecuteShouldActorDraw in
 * games/mm/2s2h/GameExports_SingleExe.cpp (see the Should-family note above).
 * The stubs here carried the signature-drift hazard class of #372/#424
 * (int(int) / int(void*) against the real bool(u8) / bool(Actor*)). */
void GameInteractor_ExecuteOnCameraChangeModeFlags(void* camera) { (void)camera; }
void GameInteractor_ExecuteOnCameraChangeSettingsFlags(void* camera) { (void)camera; }
void GameInteractor_ExecuteAfterCameraUpdate(void* camera) { (void)camera; }
/* GameInteractor_ExecuteOnPassPlayerInputs moved to real, header-checked
 * dispatch in games/mm/2s2h/GameExports_SingleExe.cpp (#442): MM's real
 * z_player.c call site already pumps this every gameplay frame; the stub was
 * keeping SavingEnhancements.cpp's post-migration OnPassPlayerInputs
 * registration (cutscene-skip-on-load's gameplay-started detector) a
 * permanent no-op. Re-stubbing here would silently sever it again. */
void GameInteractor_ExecuteOnPlayerPostLimbDraw(void* player, int limbIndex) { (void)player; (void)limbIndex; }
void GameInteractor_ExecuteOnBossDefeated(int bossId) { (void)bossId; }
void GameInteractor_ExecuteOnBottleContentsUpdate(int slotId) { (void)slotId; }
void GameInteractor_ExecuteOnConsoleLogoUpdate(void) {}
/* GameInteractor_ExecuteOnFileSelectSaveLoad moved to real, header-checked
 * dispatch in games/mm/2s2h/GameExports_SingleExe.cpp (#438), reached through
 * the single-exe macro rebind at the bottom of MM's GameInteractor.h. The stub
 * was both dead dispatch — FileSelect.cpp's registrant is the sole isRando[]
 * writer, so a randomizer file on MM's file-select list rendered exactly like
 * a vanilla one — and the worst signature drift in this file: (void*, int)
 * against the real (s16, bool, SaveContext*), so even a future caller-side
 * fix would have marshalled garbage. Re-stubbing it here would silently sever
 * the dispatch again. (MM-only symbol — OoT defines no twin.) */
/* GameInteractor_ExecuteOnGameCompletion moved to real, header-checked dispatch
 * in games/mm/2s2h/GameExports_SingleExe.cpp (#438), reached through the
 * single-exe macro rebind at the bottom of MM's GameInteractor.h. Its registrant
 * (RegisterSavingEnhancements' fileCompletedAt stamp) went live with #520, so a
 * no-op here would silently drop the game-completion stamp again. Re-stubbing it
 * would sever that dispatch. (MM-only symbol — OoT defines no twin, so deleting
 * the stub cannot orphan an OoT caller.) */
/* GameInteractor_ExecuteBeforeEndOfCycleSave / ExecuteAfterEndOfCycleSave moved
 * to real, header-checked dispatch in
 * games/mm/2s2h/GameExports_SingleExe.cpp (#514), reached through the
 * single-exe macro rebind at the bottom of MM's GameInteractor.h. #442 left
 * this pair stubbed deliberately, to be wired as a pair rather than half-wired;
 * #514 wires both, so that deferral is spent and the "stay stubbed on purpose"
 * note it carried is gone with it.
 *
 * These two were the most expensive no-ops in this file. Both call sites in
 * games/mm/src/code/z_sram_NES.c (Sram_SaveEndOfCycle, entered by Song of Time
 * and "Dawn of the New Day") are live and unguarded, so the vanilla three-day
 * wipe ran with no snapshot taken and no restore performed:
 * Rando::MiscBehavior::AfterEndOfCycleSave — dungeon/boss keys, stray fairies,
 * skulltula tokens, frog flags, the three trade slots, and the per-check
 * cycleObtained reset — was registered and unreachable, and because the checks
 * stay flagged obtained none of what the wipe took was re-collectable.
 * Re-stubbing either name here silently restores that: routine play quietly
 * eats randomizer progress with no diagnostic, and the loss only surfaces
 * cycles later when a check refuses to re-offer an item the player had. */
/* GameInteractor_ExecuteBeforeMoonCrashSaveReset moved to real, header-checked
 * dispatch in games/mm/2s2h/GameExports_SingleExe.cpp (#442): MM's real
 * z_sram_NES.c call site already pumps this at the moon-crash reset point;
 * the stub was keeping SavingEnhancements.cpp's post-migration registration
 * (owl-save deletion on moon crash) a permanent no-op. */
void GameInteractor_ExecuteBeforeInterfaceClockDraw(void) {}
void GameInteractor_ExecuteAfterInterfaceClockDraw(void) {}
/* GameInteractor_InvertControl, GameInteractor_Dpad, and
 * GameInteractor_RightStickOcarina moved to real, header-checked definitions
 * in games/mm/2s2h/GameExports_SingleExe.cpp (#372): the untyped stubs here
 * drifted from MM's GameInteractor.h. InvertControl returned the ENUM
 * ORDINAL as a ±1 multiplier (stick_x *= 2 on every Lib_GetControlStickData
 * movement frame); Dpad returned the button combo unconditionally, forcing
 * the CVar-gated D-pad-equip and D-pad-ocarina enhancements permanently ON;
 * RightStickOcarina happened to return the right default but was one field
 * away from the same fate. */

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
/* The Ship_GetSceneName stub that used to live here is gone (Lane C0, #392):
 * 2s2h/ShipUtils.cpp is compiled in single-exe builds now and carries the
 * real scene-name table — the stub's "Unknown" would otherwise shadow it in
 * every spoiler/tracker string, and the two strong definitions cannot
 * coexist on Linux. */
void Ship_HandleConsoleCrashAsReset(void) {}

/* The Ship_ExtendedCullingActorRestoreProjectedPos stub that used to live here
 * is gone (#382). It was declared `void (void*)` while every call site passes
 * (PlayState*, Actor*) — the same stub-signature-drift class as
 * OTRConvertHUDXToScreenX, the cosmetic gfx overrides and the
 * MotionBlur/SavingEnhancements family. Worse, OoT's real body was commented
 * out, so this no-op was the ONLY definition in the link and the
 * projected-position restore did nothing for BOTH games.
 *
 * MM now has MM_Ship_ExtendedCullingActorRestoreProjectedPos
 * (2s2h/GameExports_SingleExe.cpp, reached via include/mm_ship_utils_prefix.h)
 * and OoT has a real Ship_ExtendedCullingActorRestoreProjectedPos
 * (soh/ShipUtils.cpp) built on its own projection helper. Locked by the
 * mm-culling-binding test. */

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

/* The SaveManager_SysFlashrom_Read/WriteData stubs that used to live here are
 * gone (#487). They were the same untyped-stub class as MotionBlur_Override
 * above: `int(void*, int, int)` against real prototypes of
 * `s32(void*, u32, u32)` and `void(u8*, u32, u32)`
 * (games/mm/2s2h/SaveManager/SaveManager.h), and the read stub returned 0 --
 * "success" -- while writing nothing into the caller's buffer, so MM committed
 * zeroed buffers over gSaveContext. Header-checked implementations now live in
 * games/mm/2s2h/mm_save_manager_stubs.c, which is compiled into the MM target
 * and can therefore both see SaveManager.h's C branch and be guarded on
 * RSBS_SINGLE_EXECUTABLE (redship_common, which builds this file, is not
 * compiled with that define). */

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

/* The currentActorListIndex stub that used to live here is gone (Lane C0,
 * #392): MM's ObjectExtension/ActorListIndex TUs are compiled now (the
 * randomizer's ActorBehavior layer needs the real extension store), renamed
 * to the MM_ / namespace-S2H surface via their own headers. The stub here
 * was an `int` backing MM's `extern s16` declaration — a live
 * signature-drift fault class on top of shadowing the real definition. */

/* The zapd_main stub that used to live here is gone (issue #325): it shadowed
 * the real ZAPDLib entry points in the link and silently broke in-app ROM
 * extraction. Extraction now spawns the bundled standalone ZAPD executable
 * (games/oot/soh/Extractor/Extract.cpp), so nothing references zapd_main in
 * single-exe builds. */
