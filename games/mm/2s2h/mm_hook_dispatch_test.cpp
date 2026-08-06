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
