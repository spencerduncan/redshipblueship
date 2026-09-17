/**
 * @file mm_hook_dispatch_test.cpp
 * ROM-free, display-free lock for MM's single-exe hook dispatch (#511, #438).
 * CTest label "redship", row MMHookDispatch in CMake/SingleExecutable.cmake,
 * dispatch "mm-hook-dispatch" in src/common/test_runner.cpp.
 *
 * What was broken: in single-exe the COND_HOOK / COND_ID_HOOK macros are
 * rebound (2s2h/GameInteractor/GameInteractor.h) to park every registration in
 * the MM-owned S2H::GameHooks registry, but dispatch for four hook types still
 * went out through the upstream GameInteractor_Execute* names, which resolve to
 * OoT's active-game-gated wrappers or to src/common/mm_stubs.c no-ops. Neither
 * consults S2H::GameHooks, so 21 TUs' ShouldActorInit registrants and 24 TUs'
 * OnOpenText registrants were REGISTERED AND NEVER RUN.
 *
 * The operator-visible shape: MM chest items were randomized (the give runs
 * through VB_GIVE_ITEM_FROM_CHEST, which #392 already bridged) while the chest
 * model and every rando text override stayed vanilla, because those ride
 * ShouldActorInit and OnOpenText.
 *
 * #514 added check 6 for the same class's worst instance: Before/
 * AfterEndOfCycleSave, the pair bracketing Sram_SaveEndOfCycle. There the
 * dormancy was not cosmetic -- the vanilla three-day wipe ran with the rando
 * restore dead, so a Song of Time reset took dungeon keys, stray fairies,
 * tokens and trade slots for good.
 *
 * #515 added check 7 for the pair the audit almost missed entirely:
 * OnActorKill / OnActorDestroy. They have NO mm_stubs.c entry, so there was
 * nothing to notice -- the names simply bound OoT's gated wrappers. With
 * OnActorKill dead the 18 DROP_TYPE_KILL enemies dropped vanilla collectibles
 * and every grass check (~230, ObjGrass.cpp being their sole id writer) was
 * uncollectable with no on-screen tell, so a seed could be unwinnable.
 *
 * The invariant this locks: a hook registered on the MM-owned registry is
 * actually reached by the dispatcher MM's call sites resolve to. Each check
 * registers through the SAME macro production code uses, then drives the
 * dispatcher through the SAME name the call site spells -- so it fails both if
 * a bridge is deleted and if the rebind #define is dropped, which are the two
 * ways this regresses.
 *
 * NON-VACUITY. Each check asserts its counter is zero before dispatch and
 * non-zero after, so a hook that never registered cannot pass, and the id-keyed
 * legs use a distinct id per check so a stray unkeyed registrant cannot satisfy
 * them. Every check drives its dispatcher through the UPSTREAM spelling, which
 * is what also makes the rebind #defines load-bearing here: drop one and the
 * call binds OoT's active-game-gated wrapper, which links fine and does
 * nothing. Verified red before green: reverting either the GameInteractor.h
 * rebind block or the GameExports_SingleExe.cpp bridges fails
 * FAIL(1)/FAIL(3)/FAIL(4)/FAIL(5)/FAIL(7). Check 6 (#514) is stricter by
 * accident of its symbols: its upstream names are MM-only and their mm_stubs.c
 * no-ops were deleted with the fix, so reverting its rebind is a LINK error
 * rather than a silent pass. It is also asserted as a pair on purpose -- wiring
 * only the Before half is a snapshot nobody reads, indistinguishable from the
 * bug in player-visible state, so FAIL(6) must be reachable from half a fix.
 *
 * Check 7 (#515) is the OPPOSITE extreme and the reason the "drive it through
 * the upstream spelling" rule is not optional: OoT defines both of its names,
 * so a dropped rebind links silently and does nothing. It asserts the unkeyed
 * and id-keyed Kill legs SEPARATELY, because an Execute-only bridge is the
 * plausible half-fix -- it revives the enemy drops and leaves every grass check
 * dead -- and it asserts Destroy in the same block, because Destroy is what
 * frees the entries Kill creates.
 *
 * Checks 9-11 (#438 pause-menu/file-select batch) close out the hooks with a
 * live 2ship_rando registrant and dead dispatch. OnKaleidoUpdate (9) is the
 * check-7 shape with a twist: OoT's identically-spelled definition is 0-ARG
 * against MM's 1-arg call, and C linkage encodes no arity, so the wrong bind
 * was silent -- a dropped rebind still links and FAIL(9)s here. The
 * KaleidoDrawPage pair (10) is asserted in one block like check 6, because
 * z_kaleido_scope_NES.c brackets every page draw with both halves; both its
 * dispatchers need an id-keyed leg (KaleidoItemPage keys After on PAUSE_ITEM,
 * PersistentMasks keys Before on PAUSE_MASK), so an Execute-only bridge is
 * the plausible half-fix and each ForID leg is asserted separately.
 * OnFileSelectSaveLoad (11) additionally asserts argument fidelity, because
 * its retired stub was the worst signature drift in mm_stubs.c ((void*, int)
 * against (s16, bool, SaveContext*)) and the registrant indexes isRando[] off
 * fileNum/isOwlSave -- a bridge that dispatches but marshals wrong would
 * corrupt that array while passing a run-count-only check.
 *
 * Checks 13-18 close #438 out: the remainder, which is every hook type that was
 * still registered-and-undispatched after the item/progression trio. Three
 * shapes, and the shape decides what a regression looks like.
 *   13 is the void() batch (OnGameStateMainStart's COND_HOOK leg,
 *      OnGameStateMainFinish, OnPlayDrawWorldEnd, OnPlayDestroy,
 *      OnInterfaceDrawStart, Before/AfterInterfaceClockDraw,
 *      OnConsoleLogoUpdate). Eight types, ONE signature, adjacent one-line
 *      bridges -- so cross-type separation carries this check rather than the run
 *      counts do: a bridge Executing a neighbour's registry links clean and
 *      leaves every count at 1.
 *   14 is OnPlayerPostLimbDraw, where the id-keyed (limbIndex) leg is
 *      load-bearing: all four production registrations are limb-keyed, so an
 *      Execute-only bridge would leave every one of them dead.
 *   15 is the camera trio, and the only check here that asserts a bridge REFUSES
 *      to dispatch: those three bridges key their id leg on camera->uid, so
 *      "never dispatch a NULL camera" is the bridge's contract and not the
 *      registrants', which is what lets FreeLook.cpp stay textually upstream.
 *   16 is AfterRoomSceneCommands / OnRoomInit -- the WRONG-REGISTRY pair, and the
 *      one regression shape no link error and no missing symbol can catch: both
 *      had linked dispatchers running a real loop over the upstream
 *      GameInteractor registry while every MM registrant sat in S2H::GameHooks.
 *      Revert the swap and the build stays green and only this row notices.
 *   17 is OnSeqPlayerInit, the check-7 shape (OoT defines the name as a gated
 *      wrapper, so a dropped rebind is silent).
 *   18 is the NULL-play-state lock for all of them at once. Read its own comment
 *      before trusting it: it locks that DISPATCH never needs or invents a play
 *      state, and it explicitly does NOT cover the null guards the same change
 *      added to the eight link-elided registrant TUs -- referencing those from
 *      this TU would un-elide them, which is the trap
 *      mm_registrar_coverage_test.cpp's header names.
 *
 * WHAT THIS DOES NOT COVER -- READ BEFORE TRUSTING THE SUITE ON IT. The
 * z_actor.c half (two stale `#ifdef RSBS_SINGLE_EXECUTABLE` blocks that skipped
 * the ShouldActorInit call site outright) is a call-site edit in ROM-dependent
 * code. This row proves the dispatcher works when called; nothing here proves
 * z_actor.c calls it.
 *
 * No ROM-free row covers that leg. BootMM is the obvious candidate and does NOT
 * qualify: `boot-mm` asserts MM-first bring-up prerequisites (#330) -- archive
 * load, resource-factory registration -- and returns without spawning an actor
 * or running a play frame, so it cannot observe actor init at all.
 *
 * That matters more than usual here, because removing the guards is a real
 * behavior change on every MM actor spawn: a ShouldActorInit registrant
 * returning false now kills the actor, which was impossible while the guards
 * stood. That is the upstream semantic rando needs, and it is unverified by
 * automation -- it belongs to the gameplay tier (int-*), which needs the
 * self-hosted ROM runner. Operator playtest is the check.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
/**
 * Everything under test here -- the S2H::GameHooks registry and the rebind
 * #defines -- exists only in the single-exe build. Outside it MM keeps its own
 * GameInteractor and there is no split to lock, so the row reports pass rather
 * than failing to link. test_runner.cpp declares this unconditionally.
 */
extern "C" int MM_HookDispatch_RunHeadless(void) {
    printf("[TEST] mm-hook-dispatch: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include "2s2h/GameInteractor/GameInteractor.h"
#include "mm_game_hooks.h"

#include <cstring>

namespace {

#define HOOK_ASSERT(cond, code, msg)                                                    \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

int sShouldActorInitRuns = 0;
int sOnActorInitRuns = 0;
int sOnOpenTextRuns = 0;
int sOnActorDrawRuns = 0;
int sBeforeEndOfCycleSaveRuns = 0;
int sAfterEndOfCycleSaveRuns = 0;
int sOnActorKillRuns = 0;
int sOnActorKillForIdRuns = 0;
int sOnActorDestroyForIdRuns = 0;
int sOnGameCompletionRuns = 0;
int sOnKaleidoUpdateRuns = 0;
int sBeforeKaleidoDrawRuns = 0;
int sBeforeKaleidoDrawForIdRuns = 0;
int sAfterKaleidoDrawRuns = 0;
int sAfterKaleidoDrawForIdRuns = 0;
int sOnFileSelectSaveLoadRuns = 0;
s16 sFileSelectLastFileNum = -1;
bool sFileSelectLastOwl = false;
SaveContext* sFileSelectLastCtx = nullptr;
int sOnItemGiveRuns = 0;
int sOnItemGiveForIdRuns = 0;
u8 sOnItemGiveLastItem = 0;
int sOnBottleContentsUpdateRuns = 0;
int sOnBottleContentsUpdateForIdRuns = 0;
u8 sOnBottleContentsUpdateLastItem = 0;
int sOnBossDefeatedRuns = 0;
int sOnBossDefeatedForIdRuns = 0;
s16 sOnBossDefeatedLastActorId = 0;

// Checks 13-18 -- the #438 remainder.
int sOnGameStateMainStartRuns = 0;
int sOnGameStateMainFinishRuns = 0;
int sOnPlayDrawWorldEndRuns = 0;
int sOnPlayDestroyRuns = 0;
int sOnInterfaceDrawStartRuns = 0;
int sBeforeInterfaceClockDrawRuns = 0;
int sAfterInterfaceClockDrawRuns = 0;
int sOnConsoleLogoUpdateRuns = 0;
int sOnPlayerPostLimbDrawRuns = 0;
int sOnPlayerPostLimbDrawForIdRuns = 0;
Player* sPostLimbLastPlayer = nullptr;
s32 sPostLimbLastLimb = -1;
int sOnCameraChangeModeFlagsRuns = 0;
int sOnCameraChangeModeFlagsForIdRuns = 0;
int sOnCameraChangeModeFlagsForPtrRuns = 0;
Camera* sCameraModeLastCamera = nullptr;
int sOnCameraChangeSettingsFlagsRuns = 0;
int sOnCameraChangeSettingsFlagsForIdRuns = 0;
int sAfterCameraUpdateRuns = 0;
int sAfterCameraUpdateForIdRuns = 0;
int sAfterRoomSceneCommandsRuns = 0;
int sAfterRoomSceneCommandsForIdRuns = 0;
s8 sRoomSceneLastRoom = -1;
int sOnRoomInitRuns = 0;
int sOnRoomInitForIdRuns = 0;
int sOnSeqPlayerInitRuns = 0;
int32_t sSeqLastPlayerIdx = -1;
int32_t sSeqLastSeqId = -1;
// Check 18 records what each registrant SAW rather than only that it ran: the
// property under test is that dispatch neither requires nor invents a play
// state, so "the body observed MM_gPlayState == NULL and returned" is the
// observation, and a bridge that started guarding on the global would show up
// here as a run count of 0 rather than as a wrong flag.
int sNullPlayStateObservations = 0;
int sNullPlayStateDispatches = 0;

// Distinct sentinel ids, so no check can be satisfied by another's registrant.
constexpr s16 kActorIdInit = 0x0BAD;
constexpr s16 kActorIdDraw = 0x0BAE;
constexpr s16 kActorIdKill = 0x0BAF;
constexpr s16 kActorIdDestroy = 0x0BB0;
constexpr u16 kTextIdPlain = 0x0C0D;
constexpr u16 kPauseIndexKaleido = 0x0BB1;
constexpr s16 kFileSelectFileNum = 2;
// Check 12. OnItemGive and OnBottleContentsUpdate are both (u8 item), so the
// two sentinels differ: a bridge that dispatches one type's registry from the
// other's entry point is a silent copy-paste regression the run counts alone
// would not separate.
// They are also kept two apart, not adjacent: the check dispatches
// kItemGiveItem + 1 to prove the id-keyed leg discriminates, and that probe
// must not collide with the other type's sentinel.
constexpr u8 kItemGiveItem = 0x5A;
constexpr u8 kBottleContentsItem = 0x6B;
constexpr s16 kBossActorId = 0x0BB2;

// Checks 14-17. Every id-keyed leg gets its own sentinel for the reason the
// block above gives; the camera uids additionally differ per hook type because
// all three camera bridges key their ForID leg on camera->uid, so one shared uid
// would let a bridge that dispatched the wrong type's registry still satisfy
// the id-keyed assertion.
constexpr s32 kPostLimbIndex = 0x0BB3;
constexpr s16 kCameraUidMode = 0x0BB4;
constexpr s16 kCameraUidSettings = 0x0BB5;
constexpr s16 kCameraUidAfterUpdate = 0x0BB6;
constexpr s16 kRoomSceneId = 0x0BB7;
constexpr s16 kRoomInitSceneId = 0x0BB8;
constexpr s8 kRoomNum = 3;
constexpr int32_t kSeqPlayerIdx = 2;
constexpr int32_t kSeqId = 0x0BB9;

// Argument targets for checks 14-17. Static for the same reason
// sFileSelectProbeSave is: MM's port-side Player and Camera are large, and the
// probes only need stable addresses plus (for the cameras) a uid the id-keyed
// leg can match.
Player sPostLimbProbePlayer;
Camera sCameraProbeMode;
Camera sCameraProbeSettings;
Camera sCameraProbeAfterUpdate;

// Pointer target for check 11 only; the probe never reads through it, it just
// proves the dispatcher forwards the pointer untouched. Static because MM's
// port-side SaveContext is ~64KB -- too big to put on the test's stack.
SaveContext sFileSelectProbeSave;

// ResetForTest is per-hook-type (templated on H), so the types this row touches
// are cleared explicitly rather than through one global reset.
void ResetAll() {
    S2H::GameHooks::ResetForTest<GameInteractor::ShouldActorInit>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnActorInit>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnActorDraw>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnOpenText>();
    S2H::GameHooks::ResetForTest<GameInteractor::BeforeEndOfCycleSave>();
    S2H::GameHooks::ResetForTest<GameInteractor::AfterEndOfCycleSave>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnActorKill>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnActorDestroy>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnGameCompletion>();
    // The pause-menu/file-select batch (checks 9-11). Resetting these before
    // dispatch is also the headless-safety guarantee: KaleidoItemPage's
    // production registrants dereference MM_gPlayState, which is null in this
    // ROM-free row, so only this row's own probes may be in the registries
    // when the dispatchers below run.
    S2H::GameHooks::ResetForTest<GameInteractor::OnKaleidoUpdate>();
    S2H::GameHooks::ResetForTest<GameInteractor::BeforeKaleidoDrawPage>();
    S2H::GameHooks::ResetForTest<GameInteractor::AfterKaleidoDrawPage>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnFileSelectSaveLoad>();
    // The item/progression trio (check 12). Unlike the batch above, these three
    // have NO live registrant to keep out of the way -- every registrant TU is
    // link-elided (see the check's own note) -- so the reset here is hygiene
    // against a future one rather than a headless-safety requirement.
    S2H::GameHooks::ResetForTest<GameInteractor::OnItemGive>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnBottleContentsUpdate>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnBossDefeated>();
    // The #438 remainder (checks 13-18). Same hygiene reasoning as the trio
    // above -- none of these has a linked registrant today -- with one
    // difference worth naming: AfterRoomSceneCommands' registrants
    // (Enhancements/Cheats/TimeStop.cpp, Enhancements/Restorations/JPGrottos.cpp)
    // would SPAWN ACTORS against MM_gPlayState if they ever linked and ran here,
    // so for that type the reset is a headless-safety requirement like the
    // Kaleido batch's, not hygiene.
    S2H::GameHooks::ResetForTest<GameInteractor::OnGameStateMainStart>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnGameStateMainFinish>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnPlayDrawWorldEnd>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnPlayDestroy>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnInterfaceDrawStart>();
    S2H::GameHooks::ResetForTest<GameInteractor::BeforeInterfaceClockDraw>();
    S2H::GameHooks::ResetForTest<GameInteractor::AfterInterfaceClockDraw>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnConsoleLogoUpdate>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnPlayerPostLimbDraw>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnCameraChangeModeFlags>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnCameraChangeSettingsFlags>();
    S2H::GameHooks::ResetForTest<GameInteractor::AfterCameraUpdate>();
    S2H::GameHooks::ResetForTest<GameInteractor::AfterRoomSceneCommands>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnRoomInit>();
    S2H::GameHooks::ResetForTest<GameInteractor::OnSeqPlayerInit>();
    // The raw extern "C" OnGameStateMainStart vector is a SECOND container
    // behind the same dispatcher (the CI integration blocks and mm_gi_shim_test
    // use it). Emptying it is what makes check 13's S2H-side count attributable.
    MM_GameHooks_ResetForTest();
}

} // namespace

extern "C" int MM_HookDispatch_RunHeadless(void) {
    ResetAll();

    // ---------------------------------------------------------------- 1 + 2
    // ShouldActorInit: the leg that carries EnBox's chest-content rewrite. 21
    // TUs register this; none of them ran.
    {
        sShouldActorInitRuns = 0;
        S2H::GameHooks::RegisterForID<GameInteractor::ShouldActorInit>(kActorIdInit, [](Actor* actor, bool* should) {
            (void)actor;
            (void)should;
            sShouldActorInitRuns++;
        });

        HOOK_ASSERT(sShouldActorInitRuns == 0, 1, "ShouldActorInit ran at registration time");

        Actor actor;
        memset(&actor, 0, sizeof(actor));
        actor.id = kActorIdInit;

        // Spelled exactly as z_actor.c spells it, so the rebind is under test.
        (void)GameInteractor_ShouldActorInit(&actor);

        HOOK_ASSERT(sShouldActorInitRuns == 1, 1,
                    "ShouldActorInit registrant never ran -- dispatch is not reaching S2H::GameHooks");

        // An actor of another id must NOT reach the id-keyed registrant.
        Actor other;
        memset(&other, 0, sizeof(other));
        other.id = kActorIdInit + 1;
        (void)GameInteractor_ShouldActorInit(&other);
        HOOK_ASSERT(sShouldActorInitRuns == 1, 2, "ShouldActorInit id-keyed leg fired for the wrong actor id");
    }

    // -------------------------------------------------------------------- 3
    // OnActorInit: the post-init notification that runs beside it.
    {
        sOnActorInitRuns = 0;
        S2H::GameHooks::RegisterForID<GameInteractor::OnActorInit>(kActorIdInit, [](Actor* actor) {
            (void)actor;
            sOnActorInitRuns++;
        });

        HOOK_ASSERT(sOnActorInitRuns == 0, 3, "OnActorInit ran at registration time");

        Actor actor;
        memset(&actor, 0, sizeof(actor));
        actor.id = kActorIdInit;

        GameInteractor_ExecuteOnActorInit(&actor);

        HOOK_ASSERT(sOnActorInitRuns == 1, 3,
                    "OnActorInit registrant never ran -- dispatch is not reaching S2H::GameHooks");
    }

    // -------------------------------------------------------------------- 4
    // OnActorDraw: EnSob1 (the shop) is the live registrant.
    {
        sOnActorDrawRuns = 0;
        S2H::GameHooks::RegisterForID<GameInteractor::OnActorDraw>(kActorIdDraw, [](Actor* actor) {
            (void)actor;
            sOnActorDrawRuns++;
        });

        HOOK_ASSERT(sOnActorDrawRuns == 0, 4, "OnActorDraw ran at registration time");

        Actor actor;
        memset(&actor, 0, sizeof(actor));
        actor.id = kActorIdDraw;

        GameInteractor_ExecuteOnActorDraw(&actor);

        HOOK_ASSERT(sOnActorDrawRuns == 1, 4,
                    "OnActorDraw registrant never ran -- dispatch is not reaching S2H::GameHooks");
    }

    // -------------------------------------------------------------------- 5
    // OnOpenText: 24 TUs register this; every rando dialog override rode it.
    // Both legs are checked, because the rando registrants are id-keyed
    // (COND_ID_HOOK on a text id) and the unkeyed leg is what a plain COND_HOOK
    // would use.
    {
        sOnOpenTextRuns = 0;
        S2H::GameHooks::RegisterForID<GameInteractor::OnOpenText>(kTextIdPlain,
                                                                  [](u16* textId, bool* loadFromMessageTable) {
                                                                      (void)textId;
                                                                      (void)loadFromMessageTable;
                                                                      sOnOpenTextRuns++;
                                                                  });

        HOOK_ASSERT(sOnOpenTextRuns == 0, 5, "OnOpenText ran at registration time");

        u16 textId = kTextIdPlain;
        bool loadFromMessageTable = true;
        GameInteractor_ExecuteOnOpenText(&textId, &loadFromMessageTable);

        HOOK_ASSERT(sOnOpenTextRuns == 1, 5,
                    "OnOpenText registrant never ran -- dispatch is not reaching S2H::GameHooks");

        // A different text id must not reach it.
        u16 otherId = kTextIdPlain + 1;
        GameInteractor_ExecuteOnOpenText(&otherId, &loadFromMessageTable);
        HOOK_ASSERT(sOnOpenTextRuns == 1, 5, "OnOpenText id-keyed leg fired for the wrong text id");
    }

    // -------------------------------------------------------------------- 6
    // Before/AfterEndOfCycleSave (#514): the pair that brackets
    // Sram_SaveEndOfCycle. Registered by Rando::MiscBehavior under IS_RANDO and
    // dispatched nowhere, so every Song of Time reset ran the vanilla three-day
    // wipe with the rando restore dead and took dungeon keys, stray fairies,
    // tokens and trade slots permanently -- the checks stay flagged obtained.
    //
    // Both are asserted in ONE block, deliberately: the failure this guards
    // against is not "a bridge is missing" but "only one bridge is here". A
    // Before-only wiring snapshots into OnCycleSave.cpp's saveContextCopy and
    // nothing reads it, which is indistinguishable from the bug at the level of
    // player-visible state, so the row must not be able to go green on half.
    //
    // These use the unkeyed leg because both hook types are 0-arg
    // (DEFINE_HOOK(..., ())) and every production registrant is a plain
    // COND_HOOK / Register, matching the bridges' single Execute<> leg.
    {
        sBeforeEndOfCycleSaveRuns = 0;
        sAfterEndOfCycleSaveRuns = 0;
        S2H::GameHooks::Register<GameInteractor::BeforeEndOfCycleSave>([]() { sBeforeEndOfCycleSaveRuns++; });
        S2H::GameHooks::Register<GameInteractor::AfterEndOfCycleSave>([]() { sAfterEndOfCycleSaveRuns++; });

        HOOK_ASSERT(sBeforeEndOfCycleSaveRuns == 0, 6, "BeforeEndOfCycleSave ran at registration time");
        HOOK_ASSERT(sAfterEndOfCycleSaveRuns == 0, 6, "AfterEndOfCycleSave ran at registration time");

        // Spelled exactly as z_sram_NES.c spells them, so the rebinds are under
        // test. Unlike the checks above, dropping a rebind here does not bind a
        // gated OoT wrapper -- these names are MM-only and their mm_stubs.c
        // no-ops were deleted -- so a regression surfaces as a link error.
        GameInteractor_ExecuteBeforeEndOfCycleSave();
        HOOK_ASSERT(sBeforeEndOfCycleSaveRuns == 1, 6,
                    "BeforeEndOfCycleSave registrant never ran -- the cycle snapshot is not taken");
        HOOK_ASSERT(sAfterEndOfCycleSaveRuns == 0, 6, "the Before dispatcher also ran After registrants");

        GameInteractor_ExecuteAfterEndOfCycleSave();
        HOOK_ASSERT(sAfterEndOfCycleSaveRuns == 1, 6,
                    "AfterEndOfCycleSave registrant never ran -- rando progress is not restored after the wipe");
        HOOK_ASSERT(sBeforeEndOfCycleSaveRuns == 1, 6, "the After dispatcher also re-ran Before registrants");
    }

    // -------------------------------------------------------------------- 7
    // OnActorKill + OnActorDestroy (#515): the actor-lifecycle pair. Dead in
    // exactly the silent way -- no mm_stubs.c entry to notice, so both bound
    // OoT's GI_SINGLE_EXE_GATE()'d wrappers. That cost the 18 DROP_TYPE_KILL
    // enemy drops (EnemyDrops.cpp, unkeyed) and every grass check (ObjGrass.cpp,
    // COND_ID_HOOK on ACTOR_OBJ_GRASS_UNIT -- the sole writer of grass
    // RandoCheckIds, ~230 checks with no on-screen tell that they were dead).
    //
    // BOTH LEGS OF Kill ARE ASSERTED SEPARATELY, and that is the point of this
    // check rather than a flourish: an Execute-only bridge revives the enemy
    // drops and leaves every grass check as dead as before, which is the
    // plausible half-fix here and would otherwise ship green.
    //
    // Destroy is in the SAME block for the pairing reason recorded on the
    // bridge: it is what frees the element-keyed ObjectExtension entries the
    // Kill registrant creates, so a Kill-only wiring is not a partial fix but a
    // per-scene leak of stale check-id keys. The row must not pass on half.
    {
        sOnActorKillRuns = 0;
        sOnActorKillForIdRuns = 0;
        sOnActorDestroyForIdRuns = 0;

        // The EnemyDrops.cpp shape (COND_HOOK -> unkeyed).
        S2H::GameHooks::Register<GameInteractor::OnActorKill>([](Actor* actor) {
            (void)actor;
            sOnActorKillRuns++;
        });
        // The ObjGrass.cpp shape (COND_ID_HOOK -> id-keyed).
        S2H::GameHooks::RegisterForID<GameInteractor::OnActorKill>(kActorIdKill, [](Actor* actor) {
            (void)actor;
            sOnActorKillForIdRuns++;
        });
        S2H::GameHooks::RegisterForID<GameInteractor::OnActorDestroy>(kActorIdDestroy, [](Actor* actor) {
            (void)actor;
            sOnActorDestroyForIdRuns++;
        });

        HOOK_ASSERT(sOnActorKillRuns == 0, 7, "OnActorKill ran at registration time");
        HOOK_ASSERT(sOnActorKillForIdRuns == 0, 7, "OnActorKill (id-keyed) ran at registration time");
        HOOK_ASSERT(sOnActorDestroyForIdRuns == 0, 7, "OnActorDestroy ran at registration time");

        Actor killed;
        memset(&killed, 0, sizeof(killed));
        killed.id = kActorIdKill;

        // Spelled exactly as z_actor.c's MM_Actor_Kill spells it.
        GameInteractor_ExecuteOnActorKill(&killed);

        HOOK_ASSERT(sOnActorKillRuns == 1, 7,
                    "OnActorKill unkeyed registrant never ran -- enemy kill-drops stay vanilla");
        HOOK_ASSERT(sOnActorKillForIdRuns == 1, 7,
                    "OnActorKill id-keyed registrant never ran -- every grass check stays uncollectable");
        HOOK_ASSERT(sOnActorDestroyForIdRuns == 0, 7, "the Kill dispatcher also ran Destroy registrants");

        // Another actor id reaches the unkeyed leg but must NOT reach the
        // id-keyed one -- the discrimination ObjGrass depends on.
        Actor otherKilled;
        memset(&otherKilled, 0, sizeof(otherKilled));
        otherKilled.id = kActorIdKill + 1;
        GameInteractor_ExecuteOnActorKill(&otherKilled);

        HOOK_ASSERT(sOnActorKillRuns == 2, 7, "OnActorKill unkeyed leg skipped an actor of another id");
        HOOK_ASSERT(sOnActorKillForIdRuns == 1, 7, "OnActorKill id-keyed leg fired for the wrong actor id");

        Actor destroyed;
        memset(&destroyed, 0, sizeof(destroyed));
        destroyed.id = kActorIdDestroy;

        // Spelled exactly as z_actor.c's MM_Actor_Delete spells it.
        GameInteractor_ExecuteOnActorDestroy(&destroyed);

        HOOK_ASSERT(sOnActorDestroyForIdRuns == 1, 7,
                    "OnActorDestroy registrant never ran -- element-keyed check ids are never freed");
        HOOK_ASSERT(sOnActorKillRuns == 2, 7, "the Destroy dispatcher also ran Kill registrants");
        HOOK_ASSERT(sOnActorKillForIdRuns == 1, 7, "the Destroy dispatcher also ran id-keyed Kill registrants");
    }

    // ---------------------------------------------------------------- 8
    // OnGameCompletion (#438): RegisterSavingEnhancements' fileCompletedAt stamp
    // went live with #520, but the call sites (z_boss_07.c, Rando/GiveItem.cpp)
    // bound the mm_stubs.c no-op. 0-arg, single unkeyed leg. Unlike the others
    // this symbol is MM-only and its stub was DELETED with the fix, so dropping
    // the rebind or the dispatcher is a LINK error, not a silent pass — this
    // runtime check additionally proves the dispatcher reaches the registry.
    {
        sOnGameCompletionRuns = 0;
        S2H::GameHooks::Register<GameInteractor::OnGameCompletion>([]() { sOnGameCompletionRuns++; });

        HOOK_ASSERT(sOnGameCompletionRuns == 0, 8, "OnGameCompletion ran at registration time");

        // Spelled exactly as z_boss_07.c / Rando/GiveItem.cpp spell it.
        GameInteractor_ExecuteOnGameCompletion();

        HOOK_ASSERT(sOnGameCompletionRuns == 1, 8,
                    "OnGameCompletion registrant never ran -- the game-completion stamp is dropped");
    }

    // ---------------------------------------------------------------- 9
    // OnKaleidoUpdate (#438): the trade-slot cycling input handler
    // (KaleidoItemPage.cpp, COND_HOOK -> unkeyed). Dead the check-7 way -- OoT
    // defines the same extern "C" name -- but with an arity twist: OoT's
    // definition is 0-ARG against MM's 1-arg call site
    // (z_kaleido_scope_NES.c KaleidoScope_Update), and C linkage encodes no
    // arity, so the wrong bind linked without a diagnostic. Dropping the
    // rebind re-creates exactly that, which is why this runtime check is
    // load-bearing and not a link error.
    {
        sOnKaleidoUpdateRuns = 0;
        S2H::GameHooks::Register<GameInteractor::OnKaleidoUpdate>([](PauseContext* pauseCtx) {
            (void)pauseCtx;
            sOnKaleidoUpdateRuns++;
        });

        HOOK_ASSERT(sOnKaleidoUpdateRuns == 0, 9, "OnKaleidoUpdate ran at registration time");

        PauseContext pauseCtx;
        memset(&pauseCtx, 0, sizeof(pauseCtx));

        // Spelled exactly as z_kaleido_scope_NES.c's KaleidoScope_Update
        // spells it.
        GameInteractor_ExecuteOnKaleidoUpdate(&pauseCtx);

        HOOK_ASSERT(sOnKaleidoUpdateRuns == 1, 9,
                    "OnKaleidoUpdate registrant never ran -- trade-slot cycling input is dead in the pause menu");
    }

    // ---------------------------------------------------------------- 10
    // Before/AfterKaleidoDrawPage (#438): the draw pair bracketing every
    // kaleido page draw. One block like check 6, because the call sites come
    // in pairs and a half-wired bracket is the plausible regression. Both
    // dispatchers need an id-keyed leg -- KaleidoItemPage keys After on
    // PAUSE_ITEM (the cycling arrows/previews), PersistentMasks keys Before
    // on PAUSE_MASK (the active-border quad) -- so each ForID leg is asserted
    // separately from the unkeyed one: an Execute-only bridge would revive
    // plain COND_HOOK registrants and leave both production draws dead.
    {
        sBeforeKaleidoDrawRuns = 0;
        sBeforeKaleidoDrawForIdRuns = 0;
        sAfterKaleidoDrawRuns = 0;
        sAfterKaleidoDrawForIdRuns = 0;

        // The PersistentMasks.cpp shape (RegisterForID on a page index).
        S2H::GameHooks::RegisterForID<GameInteractor::BeforeKaleidoDrawPage>(
            kPauseIndexKaleido, [](PauseContext* pauseCtx, u16 pauseIndex) {
                (void)pauseCtx;
                (void)pauseIndex;
                sBeforeKaleidoDrawForIdRuns++;
            });
        // Both legs of BOTH halves are probed, not just the leg each production
        // registrant happens to use today: the bridges are four independent
        // Execute/ExecuteForID lines, so dropping any one of them is a
        // single-line regression that the other three cannot catch.
        S2H::GameHooks::Register<GameInteractor::BeforeKaleidoDrawPage>([](PauseContext* pauseCtx, u16 pauseIndex) {
            (void)pauseCtx;
            (void)pauseIndex;
            sBeforeKaleidoDrawRuns++;
        });
        // A plain COND_HOOK would use the unkeyed leg.
        S2H::GameHooks::Register<GameInteractor::AfterKaleidoDrawPage>([](PauseContext* pauseCtx, u16 pauseIndex) {
            (void)pauseCtx;
            (void)pauseIndex;
            sAfterKaleidoDrawRuns++;
        });
        // The KaleidoItemPage.cpp shape (COND_ID_HOOK on PAUSE_ITEM).
        S2H::GameHooks::RegisterForID<GameInteractor::AfterKaleidoDrawPage>(kPauseIndexKaleido,
                                                                            [](PauseContext* pauseCtx, u16 pauseIndex) {
                                                                                (void)pauseCtx;
                                                                                (void)pauseIndex;
                                                                                sAfterKaleidoDrawForIdRuns++;
                                                                            });

        HOOK_ASSERT(sBeforeKaleidoDrawForIdRuns == 0, 10, "BeforeKaleidoDrawPage (id-keyed) ran at registration time");
        HOOK_ASSERT(sBeforeKaleidoDrawRuns == 0, 10, "BeforeKaleidoDrawPage ran at registration time");
        HOOK_ASSERT(sAfterKaleidoDrawRuns == 0, 10, "AfterKaleidoDrawPage ran at registration time");
        HOOK_ASSERT(sAfterKaleidoDrawForIdRuns == 0, 10, "AfterKaleidoDrawPage (id-keyed) ran at registration time");

        PauseContext pauseCtx;
        memset(&pauseCtx, 0, sizeof(pauseCtx));

        // Spelled exactly as z_kaleido_scope_NES.c spells them.
        GameInteractor_ExecuteBeforeKaleidoDrawPage(&pauseCtx, kPauseIndexKaleido);
        HOOK_ASSERT(sBeforeKaleidoDrawForIdRuns == 1, 10,
                    "BeforeKaleidoDrawPage id-keyed registrant never ran -- the pre-draw bracket is dead");
        HOOK_ASSERT(sBeforeKaleidoDrawRuns == 1, 10,
                    "BeforeKaleidoDrawPage unkeyed registrant never ran -- the pre-draw bracket is dead");
        HOOK_ASSERT(sAfterKaleidoDrawRuns == 0, 10, "the Before dispatcher also ran After registrants");
        HOOK_ASSERT(sAfterKaleidoDrawForIdRuns == 0, 10, "the Before dispatcher also ran id-keyed After registrants");

        // Another page index must NOT reach the id-keyed leg -- the
        // discrimination both production registrants depend on.
        GameInteractor_ExecuteBeforeKaleidoDrawPage(&pauseCtx, kPauseIndexKaleido + 1);
        HOOK_ASSERT(sBeforeKaleidoDrawForIdRuns == 1, 10,
                    "BeforeKaleidoDrawPage id-keyed leg fired for the wrong page index");
        HOOK_ASSERT(sBeforeKaleidoDrawRuns == 2, 10, "BeforeKaleidoDrawPage unkeyed leg skipped another page index");

        GameInteractor_ExecuteAfterKaleidoDrawPage(&pauseCtx, kPauseIndexKaleido);
        HOOK_ASSERT(sAfterKaleidoDrawRuns == 1, 10,
                    "AfterKaleidoDrawPage unkeyed registrant never ran -- the post-draw bracket is dead");
        HOOK_ASSERT(sAfterKaleidoDrawForIdRuns == 1, 10,
                    "AfterKaleidoDrawPage id-keyed registrant never ran -- trade-slot cycling draws no affordance");
        HOOK_ASSERT(sBeforeKaleidoDrawForIdRuns == 1, 10, "the After dispatcher also re-ran Before registrants");
        HOOK_ASSERT(sBeforeKaleidoDrawRuns == 2, 10, "the After dispatcher also re-ran unkeyed Before registrants");

        GameInteractor_ExecuteAfterKaleidoDrawPage(&pauseCtx, kPauseIndexKaleido + 1);
        HOOK_ASSERT(sAfterKaleidoDrawRuns == 2, 10, "AfterKaleidoDrawPage unkeyed leg skipped another page index");
        HOOK_ASSERT(sAfterKaleidoDrawForIdRuns == 1, 10,
                    "AfterKaleidoDrawPage id-keyed leg fired for the wrong page index");
    }

    // ---------------------------------------------------------------- 11
    // OnFileSelectSaveLoad (#438): the isRando[] writer behind the rando
    // file-select presentation (FileSelect.cpp, plain Register -> unkeyed).
    // Argument FIDELITY is asserted, not just the run count: the retired
    // mm_stubs.c stub was (void*, int) against the real
    // (s16, bool, SaveContext*), and the production registrant indexes
    // isRando[] off fileNum/isOwlSave and reads saveType through the pointer,
    // so a bridge that dispatches but marshals wrong corrupts that array
    // while a count-only check stays green.
    {
        sOnFileSelectSaveLoadRuns = 0;
        sFileSelectLastFileNum = -1;
        sFileSelectLastOwl = false;
        sFileSelectLastCtx = nullptr;
        S2H::GameHooks::Register<GameInteractor::OnFileSelectSaveLoad>(
            [](s16 fileNum, bool isOwlSave, SaveContext* saveContext) {
                sOnFileSelectSaveLoadRuns++;
                sFileSelectLastFileNum = fileNum;
                sFileSelectLastOwl = isOwlSave;
                sFileSelectLastCtx = saveContext;
            });

        HOOK_ASSERT(sOnFileSelectSaveLoadRuns == 0, 11, "OnFileSelectSaveLoad ran at registration time");

        // Spelled exactly as z_sram_NES.c's five file-select flows spell it.
        GameInteractor_ExecuteOnFileSelectSaveLoad(kFileSelectFileNum, true, &sFileSelectProbeSave);

        HOOK_ASSERT(sOnFileSelectSaveLoadRuns == 1, 11,
                    "OnFileSelectSaveLoad registrant never ran -- rando files render as vanilla on file select");
        HOOK_ASSERT(sFileSelectLastFileNum == kFileSelectFileNum, 11,
                    "OnFileSelectSaveLoad fileNum arrived corrupted -- isRando[] would index the wrong slot");
        HOOK_ASSERT(sFileSelectLastOwl == true, 11,
                    "OnFileSelectSaveLoad isOwlSave arrived corrupted -- owl rows would alias file rows");
        HOOK_ASSERT(sFileSelectLastCtx == &sFileSelectProbeSave, 11,
                    "OnFileSelectSaveLoad saveContext pointer arrived corrupted");
    }

    // ---------------------------------------------------------------- 12
    // The item/progression trio (#438): OnItemGive, OnBottleContentsUpdate and
    // OnBossDefeated. All three bound header-checked no-ops in
    // games/mm/2s2h/mm_gameinteractor_stubs.c while their call sites --
    // z_parameter.c's MM_Item_Give (:4604), Inventory_Dpad_UpdateBottleItem /
    // MM_Inventory_UpdateBottleItem (:4905/:4922) and the five boss overlays --
    // ran live and unguarded on every MM frame that gave an item or killed a
    // boss.
    //
    // WHY THIS TRANCHE AND NOT ANOTHER. None of the 13 stubbed types has a
    // linked registrant today (build-cmake/redship.map lists no registrant TU
    // for any of them; all sit in the plain-archive 2ship_enh or in outright
    // excluded DeveloperTools). What separates these three is what happens when
    // 2ship_enh flips to WHOLE_ARCHIVE: their only registrant,
    // Enhancements/Trackers/TimeSplits/TimeSplitsActions.cpp, touches nothing
    // but its own MM_splitList and gSaveContext -- no MM_gPlayState, no
    // gfxCtx. Every other candidate in the batch dereferences live play state
    // with no null check (PersistentMasks/BowReticle/HyruleWarriorsStyledLink
    // on OnPlayerPostLimbDraw, BetterSongOfDoubleTime's UpdateDayTexture on the
    // clock pair, SkipToFileSelect's MM_gGameState cast), which is the #516
    // SIGSEGV class and needs the guards landed with the flip, not before it.
    // So this is the sub-batch whose revival is inert now and safe later.
    //
    // ARGUMENT FIDELITY IS ASSERTED, AND CROSS-TYPE SEPARATION WITH IT.
    // OnItemGive and OnBottleContentsUpdate have the SAME signature, (u8 item),
    // and adjacent bridges: a copy-paste that dispatches one type's registry
    // from the other's entry point links clean, keeps every run count at 1, and
    // silently ticks the wrong split. Each dispatcher is therefore asserted to
    // leave the other two types' counters untouched.
    {
        sOnItemGiveRuns = 0;
        sOnItemGiveForIdRuns = 0;
        sOnItemGiveLastItem = 0;
        sOnBottleContentsUpdateRuns = 0;
        sOnBottleContentsUpdateForIdRuns = 0;
        sOnBottleContentsUpdateLastItem = 0;
        sOnBossDefeatedRuns = 0;
        sOnBossDefeatedForIdRuns = 0;
        sOnBossDefeatedLastActorId = 0;

        // The TimeSplitsActions.cpp shape for all three (COND_HOOK -> unkeyed).
        S2H::GameHooks::Register<GameInteractor::OnItemGive>([](u8 item) {
            sOnItemGiveRuns++;
            sOnItemGiveLastItem = item;
        });
        S2H::GameHooks::Register<GameInteractor::OnBottleContentsUpdate>([](u8 item) {
            sOnBottleContentsUpdateRuns++;
            sOnBottleContentsUpdateLastItem = item;
        });
        S2H::GameHooks::Register<GameInteractor::OnBossDefeated>([](s16 actorId) {
            sOnBossDefeatedRuns++;
            sOnBossDefeatedLastActorId = actorId;
        });
        // The id-keyed legs mirror the excluded GameInteractor.cpp twins
        // (:266-277 key OnItemGive/OnBottleContentsUpdate on the item byte,
        // :186-191 keys OnBossDefeated on the actor id). No MM TU registers
        // these by id today, so an Execute-only bridge would ship green
        // without them -- they are probed for the same reason check 7 probes
        // OnActorKill's.
        S2H::GameHooks::RegisterForID<GameInteractor::OnItemGive>(kItemGiveItem, [](u8 item) {
            (void)item;
            sOnItemGiveForIdRuns++;
        });
        S2H::GameHooks::RegisterForID<GameInteractor::OnBottleContentsUpdate>(kBottleContentsItem, [](u8 item) {
            (void)item;
            sOnBottleContentsUpdateForIdRuns++;
        });
        S2H::GameHooks::RegisterForID<GameInteractor::OnBossDefeated>(kBossActorId, [](s16 actorId) {
            (void)actorId;
            sOnBossDefeatedForIdRuns++;
        });

        HOOK_ASSERT(sOnItemGiveRuns == 0, 12, "OnItemGive ran at registration time");
        HOOK_ASSERT(sOnBottleContentsUpdateRuns == 0, 12, "OnBottleContentsUpdate ran at registration time");
        HOOK_ASSERT(sOnBossDefeatedRuns == 0, 12, "OnBossDefeated ran at registration time");

        // Spelled exactly as z_parameter.c's MM_Item_Give spells it.
        GameInteractor_ExecuteOnItemGive(kItemGiveItem);
        HOOK_ASSERT(sOnItemGiveRuns == 1, 12,
                    "OnItemGive registrant never ran -- dispatch is not reaching S2H::GameHooks");
        HOOK_ASSERT(sOnItemGiveForIdRuns == 1, 12, "OnItemGive id-keyed registrant never ran");
        HOOK_ASSERT(sOnItemGiveLastItem == kItemGiveItem, 12,
                    "OnItemGive item arrived corrupted -- split ids would be matched against the wrong byte");
        HOOK_ASSERT(sOnBottleContentsUpdateRuns == 0, 12,
                    "the OnItemGive dispatcher also ran OnBottleContentsUpdate registrants");
        HOOK_ASSERT(sOnBossDefeatedRuns == 0, 12, "the OnItemGive dispatcher also ran OnBossDefeated registrants");

        // Another item reaches the unkeyed leg but must NOT reach the id-keyed one.
        GameInteractor_ExecuteOnItemGive((u8)(kItemGiveItem + 1));
        HOOK_ASSERT(sOnItemGiveRuns == 2, 12, "OnItemGive unkeyed leg skipped an item of another id");
        HOOK_ASSERT(sOnItemGiveForIdRuns == 1, 12, "OnItemGive id-keyed leg fired for the wrong item");

        // Spelled exactly as z_parameter.c's bottle updates spell it.
        GameInteractor_ExecuteOnBottleContentsUpdate(kBottleContentsItem);
        HOOK_ASSERT(sOnBottleContentsUpdateRuns == 1, 12,
                    "OnBottleContentsUpdate registrant never ran -- dispatch is not reaching S2H::GameHooks");
        HOOK_ASSERT(sOnBottleContentsUpdateForIdRuns == 1, 12, "OnBottleContentsUpdate id-keyed registrant never ran");
        HOOK_ASSERT(sOnBottleContentsUpdateLastItem == kBottleContentsItem, 12,
                    "OnBottleContentsUpdate item arrived corrupted");
        HOOK_ASSERT(sOnItemGiveRuns == 2, 12, "the OnBottleContentsUpdate dispatcher also ran OnItemGive registrants");

        // Spelled exactly as the five boss overlays spell it.
        GameInteractor_ExecuteOnBossDefeated(kBossActorId);
        HOOK_ASSERT(sOnBossDefeatedRuns == 1, 12,
                    "OnBossDefeated registrant never ran -- dispatch is not reaching S2H::GameHooks");
        HOOK_ASSERT(sOnBossDefeatedForIdRuns == 1, 12, "OnBossDefeated id-keyed registrant never ran");
        HOOK_ASSERT(sOnBossDefeatedLastActorId == kBossActorId, 12, "OnBossDefeated actorId arrived corrupted");
        HOOK_ASSERT(sOnItemGiveRuns == 2, 12, "the OnBossDefeated dispatcher also ran OnItemGive registrants");
        HOOK_ASSERT(sOnBottleContentsUpdateRuns == 1, 12,
                    "the OnBossDefeated dispatcher also ran OnBottleContentsUpdate registrants");

        GameInteractor_ExecuteOnBossDefeated((s16)(kBossActorId + 1));
        HOOK_ASSERT(sOnBossDefeatedRuns == 2, 12, "OnBossDefeated unkeyed leg skipped an actor of another id");
        HOOK_ASSERT(sOnBossDefeatedForIdRuns == 1, 12, "OnBossDefeated id-keyed leg fired for the wrong actor id");
    }

    // ---------------------------------------------------------------- 13
    // The void() batch of #438's remainder: OnGameStateMainStart's new S2H leg,
    // OnGameStateMainFinish, OnPlayDrawWorldEnd, OnPlayDestroy,
    // OnInterfaceDrawStart, Before/AfterInterfaceClockDraw and
    // OnConsoleLogoUpdate.
    //
    // CROSS-TYPE SEPARATION IS THE WHOLE POINT OF THIS BLOCK, more than for any
    // earlier check. Eight hook types, all signature void(), all dispatched from
    // adjacent one-line bridges in the same block of
    // GameExports_SingleExe.cpp. A copy-paste that has one bridge Execute
    // another type's registry links clean, keeps every run count at exactly 1,
    // and is invisible to any check that only asks "did my registrant run" --
    // so every dispatcher below is asserted to leave the other seven counters
    // untouched, and the assertions are spelled out per type rather than looped,
    // so a failure names the pair that crossed.
    //
    // Two of the eight are driven differently on purpose:
    //  - OnGameStateMainStart goes through the MM-OWNED spelling, not the
    //    upstream one. games/mm/src/code/game.c calls BOTH: the upstream name
    //    (which binds OoT's gated wrapper and correctly no-ops on MM frames) and
    //    then MM_GameHooks_ExecuteOnGameStateMainStart inside an
    //    RSBS_SINGLE_EXECUTABLE block. Driving the upstream spelling here would
    //    test OoT's no-op.
    //  - OnPlayDestroy goes through the upstream spelling and that is
    //    load-bearing: OoT DEFINES GameInteractor_ExecuteOnPlayDestroy as an
    //    active-game-gated wrapper, so dropping the rebind #define links fine
    //    and silently restores the bug. Same shape as check 7.
    {
        sOnGameStateMainStartRuns = 0;
        sOnGameStateMainFinishRuns = 0;
        sOnPlayDrawWorldEndRuns = 0;
        sOnPlayDestroyRuns = 0;
        sOnInterfaceDrawStartRuns = 0;
        sBeforeInterfaceClockDrawRuns = 0;
        sAfterInterfaceClockDrawRuns = 0;
        sOnConsoleLogoUpdateRuns = 0;

        S2H::GameHooks::Register<GameInteractor::OnGameStateMainStart>([]() { sOnGameStateMainStartRuns++; });
        S2H::GameHooks::Register<GameInteractor::OnGameStateMainFinish>([]() { sOnGameStateMainFinishRuns++; });
        S2H::GameHooks::Register<GameInteractor::OnPlayDrawWorldEnd>([]() { sOnPlayDrawWorldEndRuns++; });
        S2H::GameHooks::Register<GameInteractor::OnPlayDestroy>([]() { sOnPlayDestroyRuns++; });
        S2H::GameHooks::Register<GameInteractor::OnInterfaceDrawStart>([]() { sOnInterfaceDrawStartRuns++; });
        S2H::GameHooks::Register<GameInteractor::BeforeInterfaceClockDraw>([]() { sBeforeInterfaceClockDrawRuns++; });
        S2H::GameHooks::Register<GameInteractor::AfterInterfaceClockDraw>([]() { sAfterInterfaceClockDrawRuns++; });
        S2H::GameHooks::Register<GameInteractor::OnConsoleLogoUpdate>([]() { sOnConsoleLogoUpdateRuns++; });

        HOOK_ASSERT(sOnGameStateMainStartRuns == 0, 13, "OnGameStateMainStart ran at registration time");
        HOOK_ASSERT(sOnInterfaceDrawStartRuns == 0, 13, "OnInterfaceDrawStart ran at registration time");
        HOOK_ASSERT(sOnPlayDestroyRuns == 0, 13, "OnPlayDestroy ran at registration time");

        // Spelled as games/mm/src/code/game.c spells the MM-owned leg.
        MM_GameHooks_ExecuteOnGameStateMainStart();
        HOOK_ASSERT(sOnGameStateMainStartRuns == 1, 13,
                    "OnGameStateMainStart COND_HOOK registrant never ran -- the dispatcher is still walking only "
                    "the raw extern \"C\" vector");
        HOOK_ASSERT(sOnGameStateMainFinishRuns == 0, 13,
                    "the OnGameStateMainStart dispatcher also ran OnGameStateMainFinish registrants");

        // Spelled as games/mm/src/code/game.c spells it.
        GameInteractor_ExecuteOnGameStateMainFinish();
        HOOK_ASSERT(sOnGameStateMainFinishRuns == 1, 13,
                    "OnGameStateMainFinish registrant never ran -- dispatch is not reaching S2H::GameHooks");
        HOOK_ASSERT(sOnGameStateMainStartRuns == 1, 13,
                    "the OnGameStateMainFinish dispatcher also re-ran OnGameStateMainStart registrants");

        // Spelled as games/mm/src/code/z_play.c spells it.
        GameInteractor_ExecuteOnPlayDrawWorldEnd();
        HOOK_ASSERT(sOnPlayDrawWorldEndRuns == 1, 13,
                    "OnPlayDrawWorldEnd registrant never ran -- dispatch is not reaching S2H::GameHooks");
        HOOK_ASSERT(sOnPlayDestroyRuns == 0, 13,
                    "the OnPlayDrawWorldEnd dispatcher also ran OnPlayDestroy "
                    "registrants");

        // Spelled as games/mm/src/code/z_play.c spells it -- and through the
        // UPSTREAM name, which OoT also defines.
        GameInteractor_ExecuteOnPlayDestroy();
        HOOK_ASSERT(sOnPlayDestroyRuns == 1, 13,
                    "OnPlayDestroy registrant never ran -- the call is still binding OoT's active-game-gated "
                    "wrapper");
        HOOK_ASSERT(sOnPlayDrawWorldEndRuns == 1, 13,
                    "the OnPlayDestroy dispatcher also re-ran OnPlayDrawWorldEnd registrants");

        // Spelled as games/mm/src/code/z_parameter.c spells it.
        GameInteractor_ExecuteOnInterfaceDrawStart();
        HOOK_ASSERT(sOnInterfaceDrawStartRuns == 1, 13,
                    "OnInterfaceDrawStart registrant never ran -- enemy health bars and the ammo-buyback digits "
                    "stay invisible");
        HOOK_ASSERT(sBeforeInterfaceClockDrawRuns == 0, 13,
                    "the OnInterfaceDrawStart dispatcher also ran BeforeInterfaceClockDraw registrants");
        HOOK_ASSERT(sAfterInterfaceClockDrawRuns == 0, 13,
                    "the OnInterfaceDrawStart dispatcher also ran AfterInterfaceClockDraw registrants");

        // The clock pair, asserted as a PAIR like check 6: Before hijacks the
        // save's time/day so the clock renders the scrub target and After puts
        // them back, so a half-wired pair leaves the player's real time
        // overwritten. FAIL(13) has to be reachable from half a fix, which means
        // the two legs must not be interchangeable -- hence the separation
        // assertions in both directions.
        GameInteractor_ExecuteBeforeInterfaceClockDraw();
        HOOK_ASSERT(sBeforeInterfaceClockDrawRuns == 1, 13,
                    "BeforeInterfaceClockDraw registrant never ran -- Better Song of Double Time scrubs against a "
                    "clock showing the current time");
        HOOK_ASSERT(sAfterInterfaceClockDrawRuns == 0, 13,
                    "the Before dispatcher also ran AfterInterfaceClockDraw registrants -- the restore would run "
                    "before the draw");

        GameInteractor_ExecuteAfterInterfaceClockDraw();
        HOOK_ASSERT(sAfterInterfaceClockDrawRuns == 1, 13,
                    "AfterInterfaceClockDraw registrant never ran -- the hijacked time/day is never restored");
        HOOK_ASSERT(sBeforeInterfaceClockDrawRuns == 1, 13,
                    "the After dispatcher also re-ran BeforeInterfaceClockDraw registrants");

        // Spelled as z_title.c ConsoleLogo_Main spells it. This row proves the
        // dispatcher works when called; it deliberately does NOT claim the call
        // site is reached on a cross-game switch, because it is not -- the
        // arrival fast-forward returns above it, which is the correct behaviour
        // (see the bridge comment). Nothing in the ROM-free tier can observe
        // that ordering.
        GameInteractor_ExecuteOnConsoleLogoUpdate();
        HOOK_ASSERT(sOnConsoleLogoUpdateRuns == 1, 13,
                    "OnConsoleLogoUpdate registrant never ran -- dispatch is not reaching S2H::GameHooks");
        HOOK_ASSERT(sOnInterfaceDrawStartRuns == 1, 13,
                    "the OnConsoleLogoUpdate dispatcher also re-ran OnInterfaceDrawStart registrants");
        HOOK_ASSERT(sOnGameStateMainFinishRuns == 1, 13,
                    "the OnConsoleLogoUpdate dispatcher also re-ran OnGameStateMainFinish registrants");
    }

    // ---------------------------------------------------------------- 14
    // OnPlayerPostLimbDraw (#438): the hook three mask/reticle enhancements
    // draw through. ARGUMENT FIDELITY and the ID-KEYED LEG are both asserted,
    // and the id leg is the load-bearing one -- all four production
    // registrations are COND_ID_HOOK keyed on a limb index (PersistentMasks and
    // HyruleWarriorsStyledLink on PLAYER_LIMB_HEAD, BowReticle on
    // PLAYER_LIMB_RIGHT_HAND, HyruleWarriorsStyledLink again on
    // PLAYER_LIMB_WAIST), so an Execute-only bridge would link, read correctly,
    // and leave every one of them dead.
    {
        sOnPlayerPostLimbDrawRuns = 0;
        sOnPlayerPostLimbDrawForIdRuns = 0;
        sPostLimbLastPlayer = nullptr;
        sPostLimbLastLimb = -1;

        S2H::GameHooks::Register<GameInteractor::OnPlayerPostLimbDraw>([](Player* player, s32 limbIndex) {
            sOnPlayerPostLimbDrawRuns++;
            sPostLimbLastPlayer = player;
            sPostLimbLastLimb = limbIndex;
        });
        S2H::GameHooks::RegisterForID<GameInteractor::OnPlayerPostLimbDraw>(kPostLimbIndex,
                                                                            [](Player* player, s32 limbIndex) {
                                                                                (void)player;
                                                                                (void)limbIndex;
                                                                                sOnPlayerPostLimbDrawForIdRuns++;
                                                                            });

        HOOK_ASSERT(sOnPlayerPostLimbDrawRuns == 0, 14, "OnPlayerPostLimbDraw ran at registration time");

        // Spelled exactly as games/mm/src/code/z_player_lib.c spells it.
        GameInteractor_ExecuteOnPlayerPostLimbDraw(&sPostLimbProbePlayer, kPostLimbIndex);
        HOOK_ASSERT(sOnPlayerPostLimbDrawRuns == 1, 14,
                    "OnPlayerPostLimbDraw registrant never ran -- persistent Bunny Hood, the bow reticle and both "
                    "Hyrule Warriors masks stay undrawn");
        HOOK_ASSERT(sOnPlayerPostLimbDrawForIdRuns == 1, 14,
                    "OnPlayerPostLimbDraw id-keyed registrant never ran -- all four production registrations are "
                    "limb-keyed");
        HOOK_ASSERT(sPostLimbLastPlayer == &sPostLimbProbePlayer, 14,
                    "OnPlayerPostLimbDraw player pointer arrived corrupted");
        HOOK_ASSERT(sPostLimbLastLimb == kPostLimbIndex, 14,
                    "OnPlayerPostLimbDraw limbIndex arrived corrupted -- masks would draw on the wrong limb");

        // Another limb reaches the unkeyed leg but must NOT reach the id-keyed one.
        GameInteractor_ExecuteOnPlayerPostLimbDraw(&sPostLimbProbePlayer, kPostLimbIndex + 1);
        HOOK_ASSERT(sOnPlayerPostLimbDrawRuns == 2, 14, "OnPlayerPostLimbDraw unkeyed leg skipped another limb");
        HOOK_ASSERT(sOnPlayerPostLimbDrawForIdRuns == 1, 14,
                    "OnPlayerPostLimbDraw id-keyed leg fired for the wrong limb");
    }

    // ---------------------------------------------------------------- 15
    // The camera trio (#438): OnCameraChangeModeFlags,
    // OnCameraChangeSettingsFlags, AfterCameraUpdate.
    //
    // THE NULL-CAMERA ASSERTION IS THE REASON THIS CHECK EXISTS SEPARATELY FROM
    // 13. These three bridges are the only ones in the batch that dereference an
    // argument themselves: they key their id leg on camera->uid, the way the
    // excluded upstream twins in 2s2h/GameInteractor/GameInteractor.cpp do. So
    // the bridge -- not the registrant -- owns "do not dispatch a NULL camera",
    // which is what lets Enhancements/Camera/FreeLook.cpp's UpdateFreeLookState
    // stay textually upstream with no guard of its own. Asserted by dispatching
    // NULL and requiring that NOTHING ran, which also fails if someone
    // "simplifies" the guard into a deref-then-check.
    //
    // Three types, one signature, adjacent bridges: cross-type separation is
    // asserted for the same reason as check 13, and each type's probe camera
    // carries a DIFFERENT uid so a bridge dispatching the wrong registry cannot
    // satisfy the id-keyed assertion either.
    {
        sOnCameraChangeModeFlagsRuns = 0;
        sOnCameraChangeModeFlagsForIdRuns = 0;
        sOnCameraChangeModeFlagsForPtrRuns = 0;
        sCameraModeLastCamera = nullptr;
        sOnCameraChangeSettingsFlagsRuns = 0;
        sOnCameraChangeSettingsFlagsForIdRuns = 0;
        sAfterCameraUpdateRuns = 0;
        sAfterCameraUpdateForIdRuns = 0;
        sCameraProbeMode.uid = kCameraUidMode;
        sCameraProbeSettings.uid = kCameraUidSettings;
        sCameraProbeAfterUpdate.uid = kCameraUidAfterUpdate;

        S2H::GameHooks::Register<GameInteractor::OnCameraChangeModeFlags>([](Camera* camera) {
            sOnCameraChangeModeFlagsRuns++;
            sCameraModeLastCamera = camera;
        });
        S2H::GameHooks::RegisterForID<GameInteractor::OnCameraChangeModeFlags>(kCameraUidMode, [](Camera* camera) {
            (void)camera;
            sOnCameraChangeModeFlagsForIdRuns++;
        });
        S2H::GameHooks::RegisterForPtr<GameInteractor::OnCameraChangeModeFlags>(
            (uintptr_t)&sCameraProbeMode, [](Camera* camera) {
                (void)camera;
                sOnCameraChangeModeFlagsForPtrRuns++;
            });
        S2H::GameHooks::Register<GameInteractor::OnCameraChangeSettingsFlags>([](Camera* camera) {
            (void)camera;
            sOnCameraChangeSettingsFlagsRuns++;
        });
        S2H::GameHooks::RegisterForID<GameInteractor::OnCameraChangeSettingsFlags>(
            kCameraUidSettings, [](Camera* camera) {
                (void)camera;
                sOnCameraChangeSettingsFlagsForIdRuns++;
            });
        S2H::GameHooks::Register<GameInteractor::AfterCameraUpdate>([](Camera* camera) {
            (void)camera;
            sAfterCameraUpdateRuns++;
        });
        S2H::GameHooks::RegisterForID<GameInteractor::AfterCameraUpdate>(kCameraUidAfterUpdate, [](Camera* camera) {
            (void)camera;
            sAfterCameraUpdateForIdRuns++;
        });

        HOOK_ASSERT(sOnCameraChangeModeFlagsRuns == 0, 15, "OnCameraChangeModeFlags ran at registration time");

        // The NULL-camera contract, asserted BEFORE the positive legs so a
        // bridge that crashes here never reaches them. Spelled as
        // games/mm/src/code/z_camera.c spells each call.
        GameInteractor_ExecuteOnCameraChangeModeFlags(nullptr);
        GameInteractor_ExecuteOnCameraChangeSettingsFlags(nullptr);
        GameInteractor_ExecuteAfterCameraUpdate(nullptr);
        HOOK_ASSERT(sOnCameraChangeModeFlagsRuns == 0, 15,
                    "OnCameraChangeModeFlags dispatched a NULL camera -- the id leg keys on camera->uid");
        HOOK_ASSERT(sOnCameraChangeSettingsFlagsRuns == 0, 15, "OnCameraChangeSettingsFlags dispatched a NULL camera");
        HOOK_ASSERT(sAfterCameraUpdateRuns == 0, 15, "AfterCameraUpdate dispatched a NULL camera");

        GameInteractor_ExecuteOnCameraChangeModeFlags(&sCameraProbeMode);
        HOOK_ASSERT(sOnCameraChangeModeFlagsRuns == 1, 15,
                    "OnCameraChangeModeFlags registrant never ran -- free look's latch is never cleared on "
                    "non-Z camera transitions");
        HOOK_ASSERT(sOnCameraChangeModeFlagsForIdRuns == 1, 15,
                    "OnCameraChangeModeFlags id-keyed (camera->uid) registrant never ran");
        HOOK_ASSERT(sOnCameraChangeModeFlagsForPtrRuns == 1, 15,
                    "OnCameraChangeModeFlags ptr-keyed registrant never ran");
        HOOK_ASSERT(sCameraModeLastCamera == &sCameraProbeMode, 15,
                    "OnCameraChangeModeFlags camera pointer arrived corrupted");
        HOOK_ASSERT(sOnCameraChangeSettingsFlagsRuns == 0, 15,
                    "the OnCameraChangeModeFlags dispatcher also ran OnCameraChangeSettingsFlags registrants");
        HOOK_ASSERT(sAfterCameraUpdateRuns == 0, 15,
                    "the OnCameraChangeModeFlags dispatcher also ran AfterCameraUpdate registrants");

        GameInteractor_ExecuteOnCameraChangeSettingsFlags(&sCameraProbeSettings);
        HOOK_ASSERT(sOnCameraChangeSettingsFlagsRuns == 1, 15,
                    "OnCameraChangeSettingsFlags registrant never ran -- dispatch is not reaching S2H::GameHooks");
        HOOK_ASSERT(sOnCameraChangeSettingsFlagsForIdRuns == 1, 15,
                    "OnCameraChangeSettingsFlags id-keyed registrant never ran");
        HOOK_ASSERT(sOnCameraChangeModeFlagsRuns == 1, 15,
                    "the OnCameraChangeSettingsFlags dispatcher also re-ran OnCameraChangeModeFlags registrants");

        GameInteractor_ExecuteAfterCameraUpdate(&sCameraProbeAfterUpdate);
        HOOK_ASSERT(sAfterCameraUpdateRuns == 1, 15,
                    "AfterCameraUpdate registrant never ran -- dispatch is not reaching S2H::GameHooks");
        HOOK_ASSERT(sAfterCameraUpdateForIdRuns == 1, 15, "AfterCameraUpdate id-keyed registrant never ran");
        HOOK_ASSERT(sOnCameraChangeModeFlagsRuns == 1, 15,
                    "the AfterCameraUpdate dispatcher also re-ran OnCameraChangeModeFlags registrants");
        HOOK_ASSERT(sOnCameraChangeSettingsFlagsRuns == 1, 15,
                    "the AfterCameraUpdate dispatcher also re-ran OnCameraChangeSettingsFlags registrants");

        // A camera with a different uid reaches the unkeyed leg only.
        sCameraProbeMode.uid = kCameraUidMode + 1;
        GameInteractor_ExecuteOnCameraChangeModeFlags(&sCameraProbeMode);
        HOOK_ASSERT(sOnCameraChangeModeFlagsRuns == 2, 15,
                    "OnCameraChangeModeFlags unkeyed leg skipped a camera with another uid");
        HOOK_ASSERT(sOnCameraChangeModeFlagsForIdRuns == 1, 15,
                    "OnCameraChangeModeFlags id-keyed leg fired for the wrong uid");
        sCameraProbeMode.uid = kCameraUidMode;
    }

    // ---------------------------------------------------------------- 16
    // AfterRoomSceneCommands and OnRoomInit (#438) -- the WRONG-REGISTRY pair,
    // and the only members of this batch that never had a stub to notice.
    //
    // Both had a real, linked dispatcher in GameExports_SingleExe.cpp that ran a
    // real loop over GameInteractor::RegisteredGameHooks<H> while every MM
    // registrant sat in S2H::GameHooks. Nothing to grep, nothing to link-error,
    // and a bridge that reads correct. The regression shape is therefore
    // different from every other check here: revert the registry swap and this
    // check fails, but the build stays green and no symbol goes missing -- which
    // is exactly why the row is the only gate.
    //
    // THE ID-KEYED LEG IS THE DESTRUCTIVE HALF. JPGrottos.cpp registers
    // COND_ID_HOOK(AfterRoomSceneCommands, SCENE_22DEKUCITY, ...), and that
    // registrant is the ONLY writer of the isSpawningJPGrottos flag its four
    // already-live ShouldActorInit legs read before deleting Deku Palace's
    // vanilla grottos and torches. An unkeyed-only registry swap revives
    // TimeStop and leaves that half broken, so the two legs are asserted
    // separately.
    //
    // Driven through the upstream spelling, which here is also the definition's
    // own name: these two are MM-only names with no OoT counterpart and no
    // rebind, so what is under test is the registry the definition walks.
    {
        sAfterRoomSceneCommandsRuns = 0;
        sAfterRoomSceneCommandsForIdRuns = 0;
        sRoomSceneLastRoom = -1;
        sOnRoomInitRuns = 0;
        sOnRoomInitForIdRuns = 0;

        S2H::GameHooks::Register<GameInteractor::AfterRoomSceneCommands>([](s8 sceneId, s8 roomNum) {
            (void)sceneId;
            sAfterRoomSceneCommandsRuns++;
            sRoomSceneLastRoom = roomNum;
        });
        S2H::GameHooks::RegisterForID<GameInteractor::AfterRoomSceneCommands>(kRoomSceneId, [](s8 sceneId, s8 roomNum) {
            (void)sceneId;
            (void)roomNum;
            sAfterRoomSceneCommandsForIdRuns++;
        });
        S2H::GameHooks::Register<GameInteractor::OnRoomInit>([](s8 sceneId, s8 roomNum) {
            (void)sceneId;
            (void)roomNum;
            sOnRoomInitRuns++;
        });
        S2H::GameHooks::RegisterForID<GameInteractor::OnRoomInit>(kRoomInitSceneId, [](s8 sceneId, s8 roomNum) {
            (void)sceneId;
            (void)roomNum;
            sOnRoomInitForIdRuns++;
        });

        HOOK_ASSERT(sAfterRoomSceneCommandsRuns == 0, 16, "AfterRoomSceneCommands ran at registration time");

        // Spelled exactly as games/mm/2s2h/z_play_2SH.cpp spells it.
        GameInteractor_ExecuteAfterRoomSceneCommands(kRoomSceneId, kRoomNum);
        HOOK_ASSERT(sAfterRoomSceneCommandsRuns == 1, 16,
                    "AfterRoomSceneCommands registrant never ran -- the dispatcher is still walking the upstream "
                    "GameInteractor registry");
        HOOK_ASSERT(sAfterRoomSceneCommandsForIdRuns == 1, 16,
                    "AfterRoomSceneCommands id-keyed registrant never ran -- JP Grottos would delete Deku Palace's "
                    "vanilla grottos and spawn no replacements");
        HOOK_ASSERT(sRoomSceneLastRoom == kRoomNum, 16, "AfterRoomSceneCommands roomNum arrived corrupted");
        HOOK_ASSERT(sOnRoomInitRuns == 0, 16,
                    "the AfterRoomSceneCommands dispatcher also ran OnRoomInit "
                    "registrants");

        // A different scene reaches the unkeyed leg but not the id-keyed one.
        GameInteractor_ExecuteAfterRoomSceneCommands(kRoomSceneId + 1, kRoomNum);
        HOOK_ASSERT(sAfterRoomSceneCommandsRuns == 2, 16, "AfterRoomSceneCommands unkeyed leg skipped another scene");
        HOOK_ASSERT(sAfterRoomSceneCommandsForIdRuns == 1, 16,
                    "AfterRoomSceneCommands id-keyed leg fired for the wrong scene");

        // Spelled exactly as games/mm/2s2h/z_scene_2SH.cpp spells it.
        GameInteractor_ExecuteOnRoomInit(kRoomInitSceneId, kRoomNum);
        HOOK_ASSERT(sOnRoomInitRuns == 1, 16,
                    "OnRoomInit registrant never ran -- the dispatcher is still walking the upstream "
                    "GameInteractor registry");
        HOOK_ASSERT(sOnRoomInitForIdRuns == 1, 16, "OnRoomInit id-keyed registrant never ran");
        HOOK_ASSERT(sAfterRoomSceneCommandsRuns == 2, 16,
                    "the OnRoomInit dispatcher also re-ran AfterRoomSceneCommands registrants");
    }

    // ---------------------------------------------------------------- 17
    // OnSeqPlayerInit (#438): the check-7 shape again. OoT DEFINES
    // GameInteractor_ExecuteOnSeqPlayerInit as an active-game-gated wrapper, so
    // MM's call site in games/mm/src/audio/lib/load.c linked and bound an
    // unconditional return for the whole MM session -- with a cross-game tell,
    // because gAudioEditor.SeqNameNotification is a converged shared CVar and
    // OoT's registrant is whole-archived: the sequence-name toast worked in OoT
    // and went silent at the Clock Tower crossing. Dropping the rebind restores
    // that silently, so the upstream spelling is what this drives.
    //
    // Both arguments are asserted because they are the same type and adjacent:
    // a bridge that forwarded (seqId, playerIdx) would keep the run count at 1
    // and notify on the wrong sequence.
    {
        sOnSeqPlayerInitRuns = 0;
        sSeqLastPlayerIdx = -1;
        sSeqLastSeqId = -1;

        S2H::GameHooks::Register<GameInteractor::OnSeqPlayerInit>([](s32 playerIdx, s32 seqId) {
            sOnSeqPlayerInitRuns++;
            sSeqLastPlayerIdx = playerIdx;
            sSeqLastSeqId = seqId;
        });

        HOOK_ASSERT(sOnSeqPlayerInitRuns == 0, 17, "OnSeqPlayerInit ran at registration time");

        // Spelled exactly as games/mm/src/audio/lib/load.c spells it.
        GameInteractor_ExecuteOnSeqPlayerInit(kSeqPlayerIdx, kSeqId);
        HOOK_ASSERT(sOnSeqPlayerInitRuns == 1, 17,
                    "OnSeqPlayerInit registrant never ran -- the call is still binding OoT's active-game-gated "
                    "wrapper");
        HOOK_ASSERT(sSeqLastPlayerIdx == kSeqPlayerIdx, 17, "OnSeqPlayerInit playerIdx arrived corrupted");
        HOOK_ASSERT(sSeqLastSeqId == kSeqId, 17,
                    "OnSeqPlayerInit seqId arrived corrupted -- the toast would name the wrong sequence");
    }

    // ---------------------------------------------------------------- 18
    // NULL PLAY STATE. Every type wired by #438's remainder, dispatched with
    // MM_gPlayState explicitly NULL, asserted to reach its registrant and
    // survive.
    //
    // READ WHAT THIS DOES AND DOES NOT ESTABLISH, because the distinction is the
    // whole reason the guards this change also landed are NOT covered here.
    //
    // WHAT IT LOCKS: that DISPATCH neither requires nor invents a play state.
    // None of the bridges in GameExports_SingleExe.cpp reads MM_gPlayState, and
    // two of these hook types (OnConsoleLogoUpdate, OnGameStateMainFinish)
    // legitimately fire when there is none -- so a future edit that "helpfully"
    // adds a blanket `if (MM_gPlayState == NULL) return;` at the bridges would
    // break them, and shows up here as a run count of 0. The registrants below
    // record that they OBSERVED a null global rather than merely that they ran,
    // so the check also fails if something under dispatch were to substitute a
    // play state.
    //
    // WHAT IT CANNOT LOCK: the null guards this change added to the real
    // registrant bodies (AmmoBuyback, PersistentMasks, BowReticle,
    // HyruleWarriorsStyledLink, BetterSongOfDoubleTime, SkipToFileSelect,
    // TimeStop, JPGrottos). Those TUs are link-elided from the plain-archive
    // 2ship_enh, so their code is not in this binary and no ROM-free row can
    // call it. Referencing any of their registrar functions from this TU would
    // ITSELF be the inbound reference that un-elides them -- the trap
    // mm_registrar_coverage_test.cpp's header names -- which would defeat the
    // measurement the elision gate makes and arm eight enhancements as a side
    // effect of a test. So the guards are verified by inspection and by the
    // lock's shape (a NULL-play-state dispatch of every type reaches its
    // registrant), and they become executable only when 2ship_enh flips to
    // WHOLE_ARCHIVE. The flip is where a real gameplay check belongs; the
    // operator playtest is the check until then.
    //
    // Not covered either: mm-registrar-coverage and mm-shipinit-driver cannot be
    // extended for these types for the same reason. Those rows attribute a
    // non-empty registry to exactly one production registrar, and none of these
    // types has a registrar in the binary to attribute anything to.
    {
        PlayState* savedPlayState = MM_gPlayState;
        MM_gPlayState = nullptr;
        sNullPlayStateObservations = 0;
        sNullPlayStateDispatches = 0;

        ResetAll();

        auto observe = []() {
            sNullPlayStateDispatches++;
            if (MM_gPlayState == nullptr) {
                sNullPlayStateObservations++;
            }
        };
        S2H::GameHooks::Register<GameInteractor::OnGameStateMainStart>(observe);
        S2H::GameHooks::Register<GameInteractor::OnGameStateMainFinish>(observe);
        S2H::GameHooks::Register<GameInteractor::OnPlayDrawWorldEnd>(observe);
        S2H::GameHooks::Register<GameInteractor::OnPlayDestroy>(observe);
        S2H::GameHooks::Register<GameInteractor::OnInterfaceDrawStart>(observe);
        S2H::GameHooks::Register<GameInteractor::BeforeInterfaceClockDraw>(observe);
        S2H::GameHooks::Register<GameInteractor::AfterInterfaceClockDraw>(observe);
        S2H::GameHooks::Register<GameInteractor::OnConsoleLogoUpdate>(observe);
        S2H::GameHooks::Register<GameInteractor::OnPlayerPostLimbDraw>([](Player* player, s32 limbIndex) {
            (void)player;
            (void)limbIndex;
            sNullPlayStateDispatches++;
            if (MM_gPlayState == nullptr) {
                sNullPlayStateObservations++;
            }
        });
        S2H::GameHooks::Register<GameInteractor::AfterRoomSceneCommands>([](s8 sceneId, s8 roomNum) {
            (void)sceneId;
            (void)roomNum;
            sNullPlayStateDispatches++;
            if (MM_gPlayState == nullptr) {
                sNullPlayStateObservations++;
            }
        });
        S2H::GameHooks::Register<GameInteractor::OnRoomInit>([](s8 sceneId, s8 roomNum) {
            (void)sceneId;
            (void)roomNum;
            sNullPlayStateDispatches++;
            if (MM_gPlayState == nullptr) {
                sNullPlayStateObservations++;
            }
        });
        S2H::GameHooks::Register<GameInteractor::OnSeqPlayerInit>([](s32 playerIdx, s32 seqId) {
            (void)playerIdx;
            (void)seqId;
            sNullPlayStateDispatches++;
            if (MM_gPlayState == nullptr) {
                sNullPlayStateObservations++;
            }
        });
        auto observeCamera = [](Camera* camera) {
            (void)camera;
            sNullPlayStateDispatches++;
            if (MM_gPlayState == nullptr) {
                sNullPlayStateObservations++;
            }
        };
        S2H::GameHooks::Register<GameInteractor::OnCameraChangeModeFlags>(observeCamera);
        S2H::GameHooks::Register<GameInteractor::OnCameraChangeSettingsFlags>(observeCamera);
        S2H::GameHooks::Register<GameInteractor::AfterCameraUpdate>(observeCamera);

        MM_GameHooks_ExecuteOnGameStateMainStart();
        GameInteractor_ExecuteOnGameStateMainFinish();
        GameInteractor_ExecuteOnPlayDrawWorldEnd();
        GameInteractor_ExecuteOnPlayDestroy();
        GameInteractor_ExecuteOnInterfaceDrawStart();
        GameInteractor_ExecuteBeforeInterfaceClockDraw();
        GameInteractor_ExecuteAfterInterfaceClockDraw();
        GameInteractor_ExecuteOnConsoleLogoUpdate();
        GameInteractor_ExecuteOnPlayerPostLimbDraw(&sPostLimbProbePlayer, kPostLimbIndex);
        GameInteractor_ExecuteAfterRoomSceneCommands(kRoomSceneId, kRoomNum);
        GameInteractor_ExecuteOnRoomInit(kRoomInitSceneId, kRoomNum);
        GameInteractor_ExecuteOnSeqPlayerInit(kSeqPlayerIdx, kSeqId);
        GameInteractor_ExecuteOnCameraChangeModeFlags(&sCameraProbeMode);
        GameInteractor_ExecuteOnCameraChangeSettingsFlags(&sCameraProbeSettings);
        GameInteractor_ExecuteAfterCameraUpdate(&sCameraProbeAfterUpdate);

        MM_gPlayState = savedPlayState;

        // Fifteen dispatchers, one registrant each. Spelled as a literal rather
        // than counted from a table so that adding a bridge without adding it
        // here is a failure rather than a silent pass.
        HOOK_ASSERT(sNullPlayStateDispatches == 15, 18,
                    "a #438-remainder dispatcher did not reach its registrant with MM_gPlayState NULL");
        HOOK_ASSERT(sNullPlayStateObservations == 15, 18,
                    "a #438-remainder registrant saw a non-NULL MM_gPlayState -- something under dispatch is "
                    "substituting a play state");
    }

    // NOTE ON WHAT COVERS THE REBIND. An earlier draft of this row compared
    // &GameInteractor_ExecuteOnOpenText against &MM_GameHooks_ExecuteOnOpenText
    // to prove the #define was in place. That check is tautological: the rebind
    // is a preprocessor macro, so both spellings are the same token by the time
    // the compiler sees them, and it passes even with the rebind deleted.
    //
    // The checks above already cover it, and are the only thing that can:
    // each drives the dispatcher through the UPSTREAM spelling and asserts a
    // hook registered on the MM-owned registry ran. Drop the #define and those
    // calls bind OoT's active-game-gated wrapper instead, which links fine,
    // does nothing, and fails FAIL(1)/FAIL(3)/FAIL(4)/FAIL(5)/FAIL(7)/FAIL(9).

    ResetAll();

    printf("[TEST] mm-hook-dispatch: PASS\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
