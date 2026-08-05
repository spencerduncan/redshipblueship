/**
 * MM GameInteractor dormant-dispatch stubs for the single-executable build
 * (#438 tracking, relocation only).
 *
 * These 13 GameInteractor_Execute* entry points have no linked provider in
 * single-exe mode: 2s2h/GameInteractor/GameInteractor.cpp (the real MM
 * implementation) is filtered out of the single-exe link (games/mm/CMakeLists.txt,
 * "GameInteractor (use OoT's)"), and none of them collides with an OoT-side
 * extern "C" wrapper the way the "Should" family and OnActorInit/OnActorDraw/
 * OnOpenText used to (see the historical notes below). They previously lived
 * as untyped redeclarations in src/common/mm_stubs.c, which includes no real
 * headers and so could not catch signature drift against
 * 2s2h/GameInteractor/GameInteractor.h -- the same class of hazard that #372,
 * #379 and #424 each shipped as a live bug.
 *
 * Moved here, header-checked, by relocation only: no dispatch is wired.
 * Every body below is the same no-op src/common/mm_stubs.c always ran, now
 * compiled against the real prototypes (2s2h/GameInteractor/GameInteractor.h,
 * matching 2s2h/GameInteractor/GameInteractor.cpp's real definitions) so the
 * compiler rejects any future drift instead of leaving an ABI mismatch to be
 * discovered at runtime. It lives in the MM target rather than in
 * src/common/mm_stubs.c for the same reason as
 * games/mm/2s2h/mm_save_manager_stubs.c: mm_stubs.c is built into
 * redship_common, which is NOT compiled with RSBS_SINGLE_EXECUTABLE, and
 * GameInteractor.h needs MM's z64.h surface (Actor/Camera/Player/PauseContext/
 * SaveContext/u8/s16/s32) to declare these at all.
 *
 * Guarded to single-exe: a standalone 2ship build compiles the real
 * GameInteractor.cpp, which owns these symbols there.
 *
 * ==========================================================================
 * Fault history (carried over verbatim from src/common/mm_stubs.c; none of
 * it concerns the 13 stubs below, but it documents why several sibling
 * GameInteractor_Execute* names are NOT stubbed here or anywhere -- they now
 * have real, header-checked dispatch elsewhere, and re-adding a stub for any
 * of them would silently sever that dispatch again).
 * ==========================================================================
 *
 * GameInteractor_ExecuteOnActorDraw / ExecuteOnActorInit / ExecuteOnOpenText
 * moved to real, header-checked dispatch in
 * games/mm/2s2h/GameExports_SingleExe.cpp (#438), reached through the
 * single-exe macro rebind at the bottom of MM's GameInteractor.h. The old
 * mm_stubs.c stub was the reason the draw pair looked wired while doing
 * nothing: 21 TUs register ShouldActorInit and 24 register OnOpenText
 * through the COND_* macros, all of which park in S2H::GameHooks, and none
 * of which that no-op ever consulted. Re-stubbing any of the three would
 * silently sever that dispatch again: chest models and every rando text
 * override go back to vanilla with no diagnostic.
 *
 * GameInteractor_ExecuteOnGameStateUpdate / ExecuteOnGameStateDrawFinish
 * moved to real, header-checked dispatch in
 * games/mm/2s2h/GameExports_SingleExe.cpp (#442): MM's own frame loop
 * (games/mm/src/code/game.c MM_GameState_Update) already calls both at the
 * right points every frame -- the "pump" upstream 2S2H used -- so once
 * SavingEnhancements.cpp's raw registrations for these two hook types moved
 * onto S2H::GameHooks, leaving these as no-ops would have kept autosave
 * (OnGameStateUpdate: HandleAutoSave) and its owl-save icon
 * (OnGameStateDrawFinish: DrawAutosaveIcon) permanently dead even though
 * registration itself no longer corrupts memory.
 *
 * GameInteractor_ExecuteBeforeKaleidoDrawPage / ExecuteAfterKaleidoDrawPage
 * moved to real, header-checked dispatch in
 * games/mm/2s2h/GameExports_SingleExe.cpp (#438), wired as a PAIR on
 * purpose, matching how z_kaleido_scope_NES.c brackets every page draw. The
 * After half had a live registrant the whole time: KaleidoItemPage.cpp's
 * COND_ID_HOOK(PAUSE_ITEM) draws the trade-slot cycling arrows and adjacent-
 * item previews. Both stubs also carried the #372/#424 signature-drift
 * hazard ((void*, int) against the real (PauseContext*, u16)), retired with
 * them.
 *
 * GameInteractor_ExecuteOnSaveInit / GameInteractor_ExecuteOnSaveLoad moved
 * to real, header-checked dispatch in games/mm/2s2h/GameExports_SingleExe.cpp
 * (Lane C1, #392): they now Execute the MM-owned S2H::GameHooks registries
 * (OnSaveInit -> Rando::MiscBehavior::OnFileCreate at MM_Sram_InitSave,
 * OnSaveLoad -> Rando's OnSaveLoadHandler at the file-select/opening loads).
 *
 * GameInteractor_ExecuteOnOpenText USED to resolve, from MM call sites, to
 * OoT's wrapper in games/oot/soh/Enhancements/game-interactor/
 * GameInteractor_Hooks.cpp (signature (uint16_t* textId, bool*
 * loadFromMessageTable), #228) -- which no-ops while MM is the active game,
 * so MM text boxes fired no hooks at all. OoT's own z_message_PAL.c still
 * reaches that wrapper; MM's z_message.c is rebound to
 * MM_GameHooks_ExecuteOnOpenText (#438) instead.
 *
 * GameInteractor_ShouldItemGive / GameInteractor_ShouldActorDraw stubs
 * retired (#392 VB follow-up): MM call sites now rebind to the header-checked
 * MM_GameHooks_ExecuteShouldItemGive / MM_GameHooks_ExecuteShouldActorDraw in
 * games/mm/2s2h/GameExports_SingleExe.cpp. The old stubs carried the
 * signature-drift hazard class of #372/#424 (int(int) / int(void*) against
 * the real bool(u8) / bool(Actor*)).
 *
 * GameInteractor_ExecuteOnPassPlayerInputs moved to real, header-checked
 * dispatch in games/mm/2s2h/GameExports_SingleExe.cpp (#442): MM's real
 * z_player.c call site already pumps this every gameplay frame; the old stub
 * was keeping SavingEnhancements.cpp's post-migration OnPassPlayerInputs
 * registration (cutscene-skip-on-load's gameplay-started detector) a
 * permanent no-op.
 *
 * GameInteractor_ExecuteOnFileSelectSaveLoad moved to real, header-checked
 * dispatch in games/mm/2s2h/GameExports_SingleExe.cpp (#438). The old stub
 * was both dead dispatch -- FileSelect.cpp's registrant is the sole
 * isRando[] writer -- and the worst signature drift in the old file: (void*,
 * int) against the real (s16, bool, SaveContext*).
 *
 * GameInteractor_ExecuteOnGameCompletion moved to real, header-checked
 * dispatch in games/mm/2s2h/GameExports_SingleExe.cpp (#438). Its registrant
 * (RegisterSavingEnhancements' fileCompletedAt stamp) went live with #520.
 *
 * GameInteractor_ExecuteBeforeEndOfCycleSave / ExecuteAfterEndOfCycleSave
 * moved to real, header-checked dispatch in
 * games/mm/2s2h/GameExports_SingleExe.cpp (#514). #442 left this pair
 * stubbed deliberately, to be wired as a pair rather than half-wired; #514
 * wired both. These two were the most expensive no-ops in the old file: both
 * call sites in games/mm/src/code/z_sram_NES.c (Sram_SaveEndOfCycle, entered
 * by Song of Time and "Dawn of the New Day") are live and unguarded, so the
 * vanilla three-day wipe ran with no snapshot taken and no restore performed.
 *
 * GameInteractor_ExecuteBeforeMoonCrashSaveReset moved to real, header-checked
 * dispatch in games/mm/2s2h/GameExports_SingleExe.cpp (#442): MM's real
 * z_sram_NES.c call site already pumps this at the moon-crash reset point;
 * the old stub was keeping SavingEnhancements.cpp's post-migration
 * registration (owl-save deletion on moon crash) a permanent no-op.
 *
 * GameInteractor_InvertControl, GameInteractor_Dpad, and
 * GameInteractor_RightStickOcarina moved to real, header-checked definitions
 * in games/mm/2s2h/GameExports_SingleExe.cpp (#372): the untyped stubs
 * drifted from MM's GameInteractor.h. InvertControl returned the ENUM
 * ORDINAL as a +-1 multiplier; Dpad returned the button combo
 * unconditionally, forcing CVar-gated enhancements permanently ON;
 * RightStickOcarina happened to return the right default but was one field
 * away from the same fate.
 *
 * The "Should" family (GameInteractor_Should, ShouldActorInit,
 * ShouldActorUpdate, ShouldActorDraw, ShouldItemGive) is no longer stubbed
 * anywhere (#392 VB follow-up): the single-exe macro rebind at the bottom of
 * MM's GameInteractor.h renames every MM call site to the MM-owned
 * MM_GameHooks_Execute* dispatchers in games/mm/2s2h/GameExports_SingleExe.cpp.
 * Re-adding a stub for any of them would silently sever that dispatch.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include "2s2h/GameInteractor/GameInteractor.h"

void GameInteractor_ExecuteOnGameStateMainFinish(void) {}
void GameInteractor_ExecuteOnPlayDrawWorldEnd(void) {}
void GameInteractor_ExecuteOnInterfaceDrawStart(void) {}

void GameInteractor_ExecuteOnItemGive(u8 item) {
    (void)item;
}

void GameInteractor_ExecuteOnCameraChangeModeFlags(Camera* camera) {
    (void)camera;
}
void GameInteractor_ExecuteOnCameraChangeSettingsFlags(Camera* camera) {
    (void)camera;
}
void GameInteractor_ExecuteAfterCameraUpdate(Camera* camera) {
    (void)camera;
}

void GameInteractor_ExecuteOnPlayerPostLimbDraw(Player* player, s32 limbIndex) {
    (void)player;
    (void)limbIndex;
}
void GameInteractor_ExecuteOnBossDefeated(s16 actorId) {
    (void)actorId;
}
void GameInteractor_ExecuteOnBottleContentsUpdate(u8 item) {
    (void)item;
}
void GameInteractor_ExecuteOnConsoleLogoUpdate(void) {}

void GameInteractor_ExecuteBeforeInterfaceClockDraw(void) {}
void GameInteractor_ExecuteAfterInterfaceClockDraw(void) {}

#endif // RSBS_SINGLE_EXECUTABLE
