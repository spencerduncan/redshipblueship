/**
 * ROM-free lock for MM's extended-culling binding (CTest label "redship",
 * row mm-culling-binding in src/common/test_runner.cpp). Issue #382.
 *
 * What was broken: games/mm/2s2h/ShipUtils.cpp is excluded from the single-exe
 * build (games/mm/CMakeLists.txt) because it collides wholesale with OoT's
 * soh/ShipUtils.cpp, so exactly one definition of each Ship_ExtendedCullingActor*
 * helper survived the link and it was OoT's. MM's six calling actor overlays
 * call them unprefixed, so MM executed OoT's bodies against MM's Actor — whose
 * projectedPos sits 8 bytes further in. There is no link error to catch this:
 * only ONE definition exists, so GNU ld (the repo's only working ODR gate) has
 * nothing to reject. Silent memory corruption.
 *
 * Ship_ExtendedCullingActorRestoreProjectedPos was worse still: OoT's body was
 * commented out and src/common/mm_stubs.c defined a ONE-parameter no-op, so
 * the restore did nothing for both games while every call site passed two
 * arguments.
 *
 * This test lives in an MM translation unit (like mm_scene_execute_test.cpp and
 * mm_resume_state_test.cpp) because it needs MM's real Actor / PlayState from
 * global.h, which must never enter src/common/test_runner.cpp's TU. It is
 * exposed through the C entry point MM_CullingBinding_RunHeadless().
 *
 * Three things are asserted, in order of strength:
 *
 * 1. LAYOUT (machine-verified, not assumed). offsetof(Actor, projectedPos) is
 *    compared between the two ports at runtime — MM's from this TU, OoT's from
 *    OoT_ActorProjectedPosOffset() in games/oot/soh/ShipUtils.cpp, each
 *    compiled with its own production headers and flags. If these two ever
 *    agree the whole fault class evaporates and this test should be revisited;
 *    while they differ, sharing one body between the ports is corruption.
 *
 * 2. BEHAVIOR (the real lock, and the one that cannot be defeated by linker
 *    ICF). The restore is invoked through its UNPREFIXED source-level name —
 *    exactly as ovl_En_Kusa2 et al. write it — against a real MM Actor and a
 *    real MM PlayState carrying an identity view-projection matrix. Passing
 *    requires all three of:
 *      - the include/mm_ship_utils_prefix.h rename to reach this TU (otherwise
 *        the call binds OoT's body, which writes MM's shape.feetPos[1] instead
 *        of projectedPos.y/z),
 *      - MM's body to write MM's projectedPos offset,
 *      - the restore to actually happen (the deleted no-op stub left the
 *        sentinel in place).
 *
 * 3. SYMBOL DISTINCTNESS (link-time). The address MM's call sites resolve to
 *    must differ from OoT's, proving two independent definitions survived
 *    rather than one shared body. The bodies differ in emitted code (different
 *    struct offsets, different projection helper), so identical-code-folding
 *    cannot merge them and make the != assertion lie. Stated plainly: the
 *    complementary "== MM_'s address" check is deliberately NOT made, because
 *    the rename is textual and that comparison would be vacuous inside this
 *    TU — see the note at the assertions.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "global.h"
#include "2s2h/ShipUtils.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Captured while the include/mm_ship_utils_prefix.h renames are still in
// force, so these hold whatever an ordinary MM caller's call site binds to.
// Must be read before the #undef block below.
static void* const sMacroBoundAdjustZ = reinterpret_cast<void*>(&Ship_ExtendedCullingActorAdjustProjectedZ);
static void* const sMacroBoundAdjustX = reinterpret_cast<void*>(&Ship_ExtendedCullingActorAdjustProjectedX);
static void* const sMacroBoundRestore = reinterpret_cast<void*>(&Ship_ExtendedCullingActorRestoreProjectedPos);

// Same binding as sMacroBoundRestore but properly typed, so the behavioral
// case below can CALL what an ordinary MM caller's source-level
// `Ship_ExtendedCullingActorRestoreProjectedPos(play, actor)` resolves to.
// Taken here rather than at the call site because the #undef below is what
// lets this TU also name OoT's symbols, and after that the plain spelling
// would resolve to OoT's.
static void (*const sMmRestoreProjectedPos)(PlayState*, Actor*) = &Ship_ExtendedCullingActorRestoreProjectedPos;

// Drop the renames so the unprefixed OoT-side symbols can be named directly.
// Declared with opaque parameters on purpose: these are OoT's entry points and
// OoT's Actor type does not exist in this TU. extern "C" means the prototype
// does not participate in the symbol name, and they are only ever addressed
// here, never called.
#undef Ship_ExtendedCullingActorAdjustProjectedZ
#undef Ship_ExtendedCullingActorAdjustProjectedX
#undef Ship_ExtendedCullingActorRestoreProjectedPos

extern "C" {
void Ship_ExtendedCullingActorAdjustProjectedZ(void* ootActor);
void Ship_ExtendedCullingActorAdjustProjectedX(void* ootActor);
void Ship_ExtendedCullingActorRestoreProjectedPos(void* ootPlay, void* ootActor);
// games/oot/soh/ShipUtils.cpp — OoT's own view of its Actor layout, compiled
// with OoT's headers. Reported rather than assumed.
size_t OoT_ActorProjectedPosOffset(void);
}

namespace {

#define CULL_ASSERT(cond, msg)                                            \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

constexpr f32 kSentinel = -12345.0f;

} // namespace

extern "C" int MM_CullingBinding_RunHeadless(void) {
    // ---- 1. Layout: the two ports really do disagree --------------------
    const size_t mmOffset = offsetof(Actor, projectedPos);
    const size_t ootOffset = OoT_ActorProjectedPosOffset();
    printf("[TEST] Actor::projectedPos offset — MM 0x%zX, OoT 0x%zX\n", mmOffset, ootOffset);
    CULL_ASSERT(mmOffset != ootOffset,
                "MM and OoT Actor::projectedPos offsets agree — the premise of #382 no longer holds, revisit");

    // ---- 3. Distinct symbols survived the link --------------------------
    // (Checked before the behavioral case so a link-level regression reports
    // as a link-level failure rather than as a confusing wrong-field result.)
    //
    // Note on what is and is not being proven here. Asserting
    // sMacroBound* == &MM_Ship_* would be vacuous: the rename is textual, so
    // inside this TU both spellings are literally the same symbol. Deleting
    // the rename does not make that comparison fail, it makes this file fail
    // to compile (MM_Ship_* would be undeclared). The assertions below are the
    // non-vacuous half — they prove OoT's definitions still exist as separate
    // symbols, i.e. that MM stopped sharing one body rather than that OoT's
    // copies were removed.
    CULL_ASSERT(sMacroBoundAdjustZ != reinterpret_cast<void*>(&Ship_ExtendedCullingActorAdjustProjectedZ),
                "MM and OoT AdjustProjectedZ are the same definition");
    CULL_ASSERT(sMacroBoundAdjustX != reinterpret_cast<void*>(&Ship_ExtendedCullingActorAdjustProjectedX),
                "MM and OoT AdjustProjectedX are the same definition");
    CULL_ASSERT(sMacroBoundRestore != reinterpret_cast<void*>(&Ship_ExtendedCullingActorRestoreProjectedPos),
                "MM and OoT RestoreProjectedPos are the same definition");

    // ---- 2. Behavior: the restore runs, against MM's layout --------------
    // PlayState is far too large for the test stack; heap it and zero it.
    PlayState* play = (PlayState*)calloc(1, sizeof(PlayState));
    CULL_ASSERT(play != NULL, "failed to allocate PlayState");

    // Identity view-projection: the projected position must come back equal to
    // the world position, which makes the expected values exact.
    MM_SkinMatrix_Clear(&play->viewProjectionMtxF);

    Actor actor;
    memset(&actor, 0, sizeof(actor));
    actor.world.pos.x = 1.0f;
    actor.world.pos.y = 2.0f;
    actor.world.pos.z = 3.0f;
    actor.projectedPos.x = kSentinel;
    actor.projectedPos.y = kSentinel;
    actor.projectedPos.z = kSentinel;
    // OoT's body would write here instead (OoT projectedPos 0x0E4 lands inside
    // MM's ActorShape). Held as a tripwire for the wrong-body case.
    actor.shape.feetPos[1].x = kSentinel;
    actor.shape.feetPos[1].y = kSentinel;
    actor.shape.feetPos[1].z = kSentinel;

    // Invoked through the binding an MM actor overlay's unprefixed call site
    // produces (see sMmRestoreProjectedPos) — the rename is what makes this
    // reach MM's body rather than OoT's.
    sMmRestoreProjectedPos(play, &actor);

    const bool restored = actor.projectedPos.x == 1.0f && actor.projectedPos.y == 2.0f && actor.projectedPos.z == 3.0f;
    const bool feetIntact = actor.shape.feetPos[1].x == kSentinel && actor.shape.feetPos[1].y == kSentinel &&
                            actor.shape.feetPos[1].z == kSentinel;

    if (!restored || !feetIntact) {
        printf("[TEST] projectedPos = (%f, %f, %f), feetPos[1] = (%f, %f, %f)\n", actor.projectedPos.x,
               actor.projectedPos.y, actor.projectedPos.z, actor.shape.feetPos[1].x, actor.shape.feetPos[1].y,
               actor.shape.feetPos[1].z);
    }
    free(play);

    CULL_ASSERT(restored, "RestoreProjectedPos did not write MM's projectedPos (no-op stub, or OoT's body bound)");
    CULL_ASSERT(feetIntact, "RestoreProjectedPos clobbered MM's shape.feetPos — OoT's body ran against MM's Actor");

    printf("[TEST] PASS: mm-culling-binding — MM binds its own Ship_ExtendedCulling* against MM's Actor\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
