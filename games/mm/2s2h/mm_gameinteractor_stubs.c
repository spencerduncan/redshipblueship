/**
 * MM GameInteractor dispatch tombstone for the single-executable build (#438).
 *
 * THIS FILE NOW DEFINES NOTHING, and that is the point of it. It exists as the
 * fault history for every GameInteractor_Execute* entry point that once
 * resolved to a no-op here (or, before #620, to an untyped redeclaration in
 * src/common/mm_stubs.c), together with the reason each one stopped. Re-adding
 * a stub for ANY name recorded below would silently sever a real dispatch, and
 * the record is what makes that visible to whoever is tempted.
 *
 * WHY THE STUBS EXISTED. games/mm/2s2h/GameInteractor/GameInteractor.cpp -- the
 * real MM implementation -- is filtered out of the single-exe link
 * (games/mm/CMakeLists.txt, "GameInteractor (use OoT's)"), so MM's C call sites
 * had no definition for the MM-only Execute* names. A no-op made them link. It
 * also made them permanently dead: the single-exe COND_HOOK / COND_ID_HOOK
 * redirection parks every MM registration in the MM-owned S2H::GameHooks
 * registry, which no stub ever consulted.
 *
 * WHY THE STUBS ARE GONE. Each name now has a real, header-checked dispatcher
 * in games/mm/2s2h/GameExports_SingleExe.cpp, reached through the macro rebind
 * at the bottom of 2s2h/GameInteractor/GameInteractor.h. OoT defines none of
 * the MM-only names, so with no stub to fall back on a dropped bridge or a
 * dropped rebind is a LINK error instead of another silent no-op -- the
 * Before/AfterEndOfCycleSave property, and the single most useful thing this
 * file's retirement buys.
 *
 * The header include below is kept deliberately: it costs one compile and it
 * means that if someone ever does re-add a definition here, it is compiled
 * against the real prototypes (2s2h/GameInteractor/GameInteractor.h, matching
 * GameInteractor.cpp's real definitions) rather than drifting silently. Several
 * of the names retired below shipped with a drifted signature at some point --
 * AfterKaleidoDrawPage (void*, int) against (PauseContext*, u16),
 * OnFileSelectSaveLoad (void*, int) against (s16, bool, SaveContext*),
 * OnBossDefeated int against s16, OnBottleContentsUpdate int against u8 -- the
 * hazard class #372, #379 and #424 each shipped as a live bug.
 *
 * The file lives in the MM target rather than in src/common/mm_stubs.c for the
 * same reason as games/mm/2s2h/mm_save_manager_stubs.c: mm_stubs.c is built
 * into redship_common, which is NOT compiled with RSBS_SINGLE_EXECUTABLE, and
 * GameInteractor.h needs MM's z64.h surface (Actor/Camera/Player/PauseContext/
 * SaveContext/u8/s16/s32) to declare these at all.
 *
 * Guarded to single-exe: a standalone 2ship build compiles the real
 * GameInteractor.cpp, which owns these symbols there.
 *
 * ==========================================================================
 * FAULT HISTORY -- every Execute* name that was ever stubbed, and what wiring
 * it cost or bought. None of these is stubbed anywhere any more; re-adding one
 * re-creates the bug named beside it, with no diagnostic.
 * ==========================================================================
 *
 * THE #438 REMAINDER (this file's last ten, plus the two OoT-gated names and
 * the two wrong-registry ones that were never stubbed). Wired together because
 * they shared one blocker: their registrant bodies dereference live play state
 * with no null check, which is the #516 SIGSEGV class, so the guards had to
 * land in the same change as the dispatch rather than ahead of it. They did.
 *
 *   AfterRoomSceneCommands  Never stubbed -- it had a real, linked dispatcher
 *     that walked the WRONG registry (GameInteractor::Instance's upstream
 *     container, not S2H::GameHooks). Fixed in place, and FIRST of the
 *     remainder, because it is the only one that is DESTRUCTIVE rather than
 *     inert on un-elision: JPGrottos.cpp's four already-live ShouldActorInit
 *     legs delete Deku Palace's vanilla grottos and torches while gated on a
 *     flag only the dead AfterRoomSceneCommands registrant sets. OnRoomInit was
 *     swapped alongside it, same registry, no registrants yet.
 *   OnGameStateMainStart    Never stubbed -- MM's dispatcher walked only the
 *     raw extern "C" vector the CI integration blocks use, so the COND_HOOK
 *     registrants (AmmoBuyback's B/C-Up mask over the buyback quantity prompt,
 *     EasyFrameAdvance, PauseBufferWindow) were never drained. The S2H leg was
 *     added beside the raw walk, which stays: it is the only hook surface a C TU
 *     can reach.
 *   OnPlayDestroy / OnSeqPlayerInit   Never stubbed either, and the #512/#515
 *     shape: OoT DEFINES both names as active-game-gated wrappers, so MM's call
 *     sites (z_play.c, audio/lib/load.c) bound an unconditional return for the
 *     whole MM session. Now rebound MM-side. Cost while dead: Better Song of
 *     Double Time never committed its chosen day/time at teardown, Play as
 *     Kafei never re-applied on scene destroy, and the sequence-name toast
 *     worked in OoT and went silent at the Clock Tower crossing (shared CVar,
 *     OoT's registrant whole-archived, MM's elided).
 *   OnInterfaceDrawStart    Stubbed here. Enemy health bars recorded max health
 *     through their live OnActorInit sibling and drew nothing; Ammo Buyback's
 *     digit readout was the other casualty. Guard landed in
 *     Enhancements/Items/AmmoBuyback.cpp (DrawAmmoSelectionDigits read
 *     MM_gPlayState->msgCtx unguarded; EnemyHealthBars.cpp was already
 *     guarded).
 *   OnPlayerPostLimbDraw    Stubbed here. Persistent Bunny Hood drew no hood,
 *     Bow Reticle no reticle, Hyrule Warriors Styled Link neither mask. All
 *     three registrants dereferenced MM_gPlayState->viewProjectionMtxF or
 *     ->state.gfxCtx unguarded; all three are guarded now. The id leg keys on
 *     limbIndex, which all four of those registrations use -- an Execute-only
 *     bridge would have been the plausible half-fix.
 *   Before/AfterInterfaceClockDraw  Stubbed here, wired as a PAIR for the
 *     reason the EndOfCycleSave pair records: Before hijacks the save's day and
 *     time so the clock renders the scrub target and After puts them back, so
 *     half a fix leaves the player's real time overwritten. Guard landed in
 *     Enhancements/Songs/BetterSongOfDoubleTime.cpp (UpdateDayTexture took
 *     MM_gPlayState unguarded).
 *   OnCameraChangeModeFlags / OnCameraChangeSettingsFlags / AfterCameraUpdate
 *     Stubbed here. The camera trio guards the CAMERA at the bridge rather
 *     than in the registrants, because the bridge itself keys its id leg on
 *     camera->uid the way the excluded upstream twins do. Free look's latch was
 *     never cleared on non-Z camera-mode transitions; the other two had no
 *     registrant at all (OnCameraChangeSettingsFlags still has none).
 *   OnConsoleLogoUpdate     Stubbed here, and the one whose call site
 *     (z_title.c ConsoleLogo_Main) is deliberately unreachable on a cross-game
 *     switch: the arrival fast-forward returns above it, which is correct,
 *     because SkipToFileSelect's registrant would MM_Sram_InitNewSave over the
 *     save the switch is carrying. Reached on a cold `redship --game mm` boot
 *     and the debug MapSelect route. Guard landed in
 *     Enhancements/Cutscenes/SkipToFileSelect.cpp (it casts MM_gGameState with
 *     no null check).
 *   OnPlayDrawWorldEnd      Stubbed here. Premise correction against #438: it
 *     has NO compiled registrant in any single-exe build rather than an elided
 *     one -- 2s2h/NameTag/*.cpp and 2s2h/DeveloperTools/*.cpp are both excluded
 *     outright, and both register through the upstream GameInteractor::Instance
 *     members anyway. Wired regardless, because its z_play.c call site is live
 *     and a real dispatcher makes the next registrant work.
 *   OnGameStateMainFinish   Stubbed here, no registrants in the tree. Wired for
 *     the same reason: game.c's call site is live, so the alternative to a
 *     dispatcher is a stub that quietly eats the first registrant somebody adds.
 *
 * GameInteractor_ExecuteOnItemGive / ExecuteOnBottleContentsUpdate /
 * ExecuteOnBossDefeated moved to real dispatch (#438's item/progression
 * tranche). That tranche is where the criterion above was written down: it went
 * AHEAD of its registrants on purpose, because its sole registrant
 * (Enhancements/Trackers/TimeSplits/TimeSplitsActions.cpp) reads MM_splitList
 * and gSaveContext and nothing else, while every other candidate needed guards
 * first. Covered by check 12 of games/mm/2s2h/mm_hook_dispatch_test.cpp.
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

/* No definitions. See the file comment: every name that used to live here has
 * real dispatch in games/mm/2s2h/GameExports_SingleExe.cpp, and the absence of
 * a fallback definition is what turns a dropped bridge into a link error. */

#endif // RSBS_SINGLE_EXECUTABLE
