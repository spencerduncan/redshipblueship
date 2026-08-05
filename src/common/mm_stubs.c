/**
 * MM Stubs and Aliases for Single Executable Build
 *
 * This file provides:
 * 1. Aliases for MM_ prefixed functions to their non-prefixed versions
 * 2. Stub implementations for enhancement layer functions that were excluded
 *
 * Header-blindness rule: this file includes NO real MM headers (it predates
 * RSBS_SINGLE_EXECUTABLE and is built into redship_common, which is not
 * compiled with that define), so every declaration here is an unchecked
 * redeclaration the compiler cannot compare against the real prototype. That
 * has shipped signature-drift bugs before (#372, #379, #424, and the
 * GameInteractor/HudEditor class documented at length below). A stub
 * belongs here ONLY if it genuinely cannot see MM headers -- libultra/libc
 * aliases, FaultDrawer, and other symbols with no real MM-side declaration
 * to check against. Anything that stubs a real MM function (anything
 * declared in an MM header under games/mm/) belongs in a header-checked MM
 * target TU instead, guarded by `#ifdef RSBS_SINGLE_EXECUTABLE` so a
 * standalone 2ship build still gets the real definition. See
 * games/mm/2s2h/mm_save_manager_stubs.c, games/mm/2s2h/mm_gameinteractor_stubs.c
 * and games/mm/2s2h/mm_hudeditor_stubs.c for the pattern.
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

/* GameInteractor stubs (all 13) and HudEditor stubs moved to
 * games/mm/2s2h/mm_gameinteractor_stubs.c and
 * games/mm/2s2h/mm_hudeditor_stubs.c respectively: both stub real MM
 * functions declared in games/mm/2s2h/GameInteractor/GameInteractor.h and
 * games/mm/2s2h/BenGui/HudEditor.h, so per the header-blindness rule above
 * they belong in a header-checked MM TU, not here. See those two files for
 * the fault history this comment used to carry (#372/#379/#392/#424/#438/
 * #442/#514 and the mm_stubs.c drift class in general) and for why the
 * "Should" family, ExecuteOnActorDraw/Init/OpenText, ExecuteOnGameStateUpdate/
 * DrawFinish, the Kaleido draw pair, OnSaveInit/OnSaveLoad, the end-of-cycle
 * pair, OnFileSelectSaveLoad, OnGameCompletion, BeforeMoonCrashSaveReset, and
 * InvertControl/Dpad/RightStickOcarina are NOT stubbed anywhere: they all
 * have real, header-checked dispatch elsewhere now. */

/* Graphics override + CosmeticEditor wrappers: implemented for real in
 * games/mm/2s2h/CosmeticGfxSingleExe.cpp, compiled against the declarations
 * in 2s2h/BenGui/CosmeticEditor.h. Do NOT re-stub them here: the wrappers
 * RETURN the advanced display-list pointer, and the void placeholder stubs
 * that used to live in this file made MM's HUD draw consume garbage as its
 * gfx write pointer (WRITE access violation at 0xA7 in
 * MM_Interface_DrawItemButtons on the first HUD-visible MM frame — caught
 * by int-gameplay-roundtrip, locked ROM-free by the cosmetic-gfx-stub
 * test). */

/* The unprefixed FrameInterpolation_IgnoreActorMtx / _InterpolateWiderAngles
 * stubs that used to live here are gone (#379). Every MM call site is rebound
 * to MM_FrameInterpolation_* by games/mm/include/mm_frame_interpolation_prefix.h
 * (force-reached through gfx.h and FrameInterpolation.h), and SoH declares
 * neither name (games/oot/soh/frame_interpolation.h) — so nothing in the link
 * referenced them. They were also the wrong shape: `void(void*)` and
 * `void(int)` against real no-argument functions
 * (2s2h/Enhancements/FrameInterpolation/FrameInterpolation.cpp:513,521), the
 * same stub-signature-drift class as MotionBlur_Override below. Do NOT
 * re-add them: an unprefixed reference appearing again is a missing-prefix bug
 * that must fail the link, not bind to a no-op. */

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
