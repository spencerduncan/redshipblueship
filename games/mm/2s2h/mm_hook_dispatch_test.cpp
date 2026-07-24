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

// Distinct sentinel ids, so no check can be satisfied by another's registrant.
constexpr s16 kActorIdInit = 0x0BAD;
constexpr s16 kActorIdDraw = 0x0BAE;
constexpr s16 kActorIdKill = 0x0BAF;
constexpr s16 kActorIdDestroy = 0x0BB0;
constexpr u16 kTextIdPlain = 0x0C0D;

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
    // does nothing, and fails FAIL(1)/FAIL(3)/FAIL(4)/FAIL(5)/FAIL(7).

    ResetAll();

    printf("[TEST] mm-hook-dispatch: PASS\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
