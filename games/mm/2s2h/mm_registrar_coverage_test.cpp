/**
 * @file mm_registrar_coverage_test.cpp
 * ROM-free, display-free lock for #516: MM's single-exe bring-up must actually
 * POPULATE the hook registries that BenPort.cpp's exclusion emptied.
 * CTest label "redship", row MMRegistrarCoverage in CMake/SingleExecutable.cmake,
 * dispatch "mm-registrar-coverage" in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN (#516). games/mm/2s2h/BenPort.cpp is excluded from the single
 * exe (games/mm/CMakeLists.txt), and it was the SOLE caller of InitOTR's
 * registration sequence. 2ship_enh is a plain STATIC archive, so with no
 * inbound reference those TUs were link-elided outright: CustomItem::
 * RegisterHooks, S2H::CustomMessage::RegisterHooks, InitEnhancements (and
 * through it RegisterSavingEnhancements / RegisterAutosave) and
 * GfxPatcher_ApplyNecessaryAuthenticPatches were all 0-hit in redship.map.
 * Player-visible: every randomized custom item uncollectable, every rando/hint
 * textbox rendering vanilla message 0x004B, no owl-save persistence, no
 * autosave. The registrars have since been re-homed into MM_Rando_Init
 * (games/mm/2s2h/GameExports_SingleExe.cpp).
 *
 * WHY THIS ROW EXISTS ALONGSIDE THE CI SYMBOL GATE. .github/scripts/
 * check-registrar-elision.sh greps the linked binary for a per-symbol
 * allowlist. That proves the registrars LINKED. It cannot prove they RAN --
 * a call moved under a condition that is never true, or a once-guard that
 * latches before the call, both keep the symbol in the binary and leave every
 * registry as empty as full elision did. This row closes that gap the way the
 * script's own header recommends for ForeignItemsSingleExe.cpp: "ask the
 * running binary whether MM's pool actually registered. A dropped initializer
 * is a red test, not a subtle count."
 *
 * It is also the ONLY possible gate for RegisterAutosave. That symbol cannot
 * be allowlisted in the CI script: OoT ships a static RegisterAutosave
 * (games/oot/soh/Enhancements/QoL/Autosave.cpp:80), so a demangled-name grep
 * is satisfied by OoT's copy whether or not MM's ever links. Attribution by
 * name is impossible; attribution by REGISTRY CONTENT is exact, because
 * OnGameStateDrawFinish has exactly one registrant in the whole MM tree
 * (SavingEnhancements.cpp, inside RegisterAutosave's CVar-gated branch).
 * That closes #516's open question 1 without renaming upstream-tracked code.
 *
 * ATTRIBUTION -- why each probe names exactly one registrar.
 *   - BeforeMoonCrashSaveReset: sole registrant in all of games/mm is
 *     SavingEnhancements.cpp (RegisterSavingEnhancements). Non-empty => it ran.
 *   - OnGameStateDrawFinish:    sole registrant is SavingEnhancements.cpp
 *     (RegisterAutosave, CVar-gated). Non-empty => it ran.
 *   - OnOpenText[0x004B]:       sole registrant for that id is
 *     CustomMessage.cpp (CustomMessage::RegisterHooks).
 *   - ShouldActorInit[EN_ITEM00]: FOUR registrants exist -- CustomItem.cpp
 *     (unconditional) plus ChuDrops.cpp, JPGrottos.cpp (each CVar-gated) and
 *     Rando/ActorBehavior/EnItem00.cpp (IS_RANDO-gated). The two CVar-gated
 *     ones are forced OFF below, so a non-empty bucket can only be
 *     CustomItem's. Without that forcing this probe could pass on a default
 *     flip while CustomItem::RegisterHooks stayed dead -- the exact vacuity
 *     #516 is about. The IS_RANDO one cannot be forced from here without
 *     touching gSaveContext (MM's z64save.h carries no extern "C" guard, so a
 *     C++ TU that includes global.h at file scope cannot also take a C-linkage
 *     declaration of it), so it is caught rather than prevented: the bucket is
 *     asserted to hold EXACTLY ONE registrant, and a second one fails loudly as
 *     a precondition violation instead of silently propping up the probe.
 *
 * NON-VACUITY. Every probe asserts its registry is EMPTY before MM_Rando_Init
 * and non-empty after, so a registry populated by something else, or by an
 * earlier phase of the harness, cannot satisfy it. Verified red before green:
 * commenting out any one of the four calls in MM_Rando_Init fails this row at
 * that probe's own FAIL code, and re-adding it turns it green.
 *
 * DELIBERATELY NOT REFERENCED HERE. This row drives only the production entry
 * point MM_Rando_Init(); it never calls CustomItem::RegisterHooks and friends
 * directly. That is load-bearing, not stylistic: a direct call from this
 * always-linked test TU would itself be an inbound reference that keeps those
 * symbols in the binary, which would make the CI symbol gate pass on a build
 * where production had dropped every call. The test must not prop up what the
 * gate measures.
 *
 * WHAT THIS DOES NOT COVER. GfxPatcher_ApplyNecessaryAuthenticPatches is a
 * one-shot resource patcher, not a hook registrar, and MM_Rando_Init gates it
 * behind MM_Rando_AssetsReady() because its ResourceMgr_Load*ByName helpers
 * null-deref with no mm.o2r. It therefore does not run in this ROM-free row and
 * has no registry to probe; it stays covered by the CI symbol allowlist alone.
 * Nor does this row prove the registered handlers are ever DISPATCHED -- that
 * is MMHookDispatch's invariant (#511/#512), and the two rows are complementary:
 * dispatch without registration and registration without dispatch are both
 * silent, and each was a real shipped bug.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
/**
 * The S2H::GameHooks registry and the BenPort exclusion that motivates this
 * lock both exist only in the single-exe build. Outside it MM runs its own
 * InitOTR and there is nothing to re-home, so the row reports pass rather than
 * failing to link. test_runner.cpp declares this unconditionally.
 */
extern "C" int MM_RegistrarCoverage_RunHeadless(void) {
    printf("[TEST] mm-registrar-coverage: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "mm_game_hooks.h"

extern "C" {
// The production bring-up under test. Declared, never defined here.
void MM_Rando_Init(void);
}

namespace {

#define RC_ASSERT(cond, code, msg)                                                      \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

// Settled counts. Unregistration in S2H::GameHooks is DEFERRED -- Unregister
// only queues an id, and the queue is drained at the next Execute of that hook
// type. RegisterAutosave in particular unregisters-then-registers on every
// call, so counting the raw maps without draining first could credit a hook
// that is already slated for removal. Flushing makes each count the state a
// real dispatch would see.
template <typename H> size_t SettledUnkeyedCount() {
    S2H::GameHooks::FlushPendingUnregistrations<H>();
    return S2H::GameHooks::Registry<H>::functions.size();
}

template <typename H> size_t SettledCountForId(int32_t forId) {
    S2H::GameHooks::FlushPendingUnregistrations<H>();
    auto& buckets = S2H::GameHooks::Registry<H>::functionsForID;
    auto it = buckets.find(forId);
    return it == buckets.end() ? 0u : it->second.size();
}

// The two CVar-gated OTHER ShouldActorInit[ACTOR_EN_ITEM00] registrants, forced
// off so the probe can only be satisfied by CustomItem's unconditional one.
// (The third competitor, EnItem00.cpp's IS_RANDO leg, is caught by the
// exactly-one assertion rather than prevented -- see the file header.)
constexpr const char* kChuDropsCVar = "gEnhancements.Equipment.ChuDrops";
constexpr const char* kJPGrottosCVar = "gEnhancements.Restorations.JPGrottos";

// RegisterAutosave registers nothing unless this is set -- it is a runtime
// toggle, not an unconditional registrar. Forced ON so its absence is a real
// failure rather than a configuration artifact.
constexpr const char* kAutosaveCVar = "gEnhancements.Autosave";

} // namespace

extern "C" int MM_RegistrarCoverage_RunHeadless(void) {
    printf("[TEST] mm-registrar-coverage: MM bring-up populates the registries BenPort's exclusion emptied (#516)\n");

    auto ctx = Ship::Context::GetInstance();
    RC_ASSERT(ctx != nullptr, 1, "Ship::Context singleton missing — run the shared bring-up first");
    RC_ASSERT(ctx->GetConsoleVariables() != nullptr, 1,
              "ConsoleVariables missing — ShipInit registrars and the CVar forcing below need them");

    // ---- Harness configuration, before any registration runs ---------------
    // Competing EN_ITEM00 registrants off and autosave on (see ATTRIBUTION in
    // the file header). These must be set before MM_Rando_Init, because
    // COND_ID_HOOK evaluates its gate once, at ShipInit::InitAll time.
    CVarSetInteger(kChuDropsCVar, 0);
    CVarSetInteger(kJPGrottosCVar, 0);
    CVarSetInteger(kAutosaveCVar, 1);

    // ---- Non-vacuity: every probed registry starts empty -------------------
    // If any of these is already populated, the "non-empty after" assertions
    // below prove nothing, so refuse to run rather than pass vacuously.
    RC_ASSERT(SettledCountForId<GameInteractor::ShouldActorInit>(ACTOR_EN_ITEM00) == 0, 2,
              "ShouldActorInit[EN_ITEM00] was already populated before MM_Rando_Init — probe would be vacuous");
    RC_ASSERT(SettledCountForId<GameInteractor::OnOpenText>(CUSTOM_MESSAGE_ID) == 0, 2,
              "OnOpenText[0x004B] was already populated before MM_Rando_Init — probe would be vacuous");
    RC_ASSERT(SettledUnkeyedCount<GameInteractor::BeforeMoonCrashSaveReset>() == 0, 2,
              "BeforeMoonCrashSaveReset was already populated before MM_Rando_Init — probe would be vacuous");
    RC_ASSERT(SettledUnkeyedCount<GameInteractor::OnGameStateDrawFinish>() == 0, 2,
              "OnGameStateDrawFinish was already populated before MM_Rando_Init — probe would be vacuous");

    // ---- The production bring-up -------------------------------------------
    // Once-only guarded internally; this row is the only caller in its process.
    MM_Rando_Init();

    // ---- CustomItem::RegisterHooks (#516 critical) -------------------------
    // Without it CustomItem::Spawn's ACTOR_EN_ITEM00/ITEM00_NOTHING placeholder
    // is never swapped for the real item: every rando reward and every
    // cross-game arrival delivers nothing.
    const size_t enItem00Registrants = SettledCountForId<GameInteractor::ShouldActorInit>(ACTOR_EN_ITEM00);
    RC_ASSERT(enItem00Registrants > 0, 3,
              "ShouldActorInit[EN_ITEM00] empty after MM_Rando_Init — CustomItem::RegisterHooks never ran, "
              "every randomized custom item is uncollectable (#516)");
    // Attribution integrity, not a feature assertion. With ChuDrops and
    // JPGrottos forced off, CustomItem's unconditional registrant is the only
    // one that may be here. A second means a competing gate came on -- most
    // likely EnItem00.cpp's IS_RANDO leg, if a shared-process `--test all`
    // ordering left saveType == SAVETYPE_RANDO -- and the probe above would no
    // longer be evidence that CustomItem ran. Fail loudly rather than pass on
    // someone else's registrant; if a fifth registrant is added deliberately,
    // update the ATTRIBUTION note in this file's header along with this count.
    RC_ASSERT(enItem00Registrants == 1, 7,
              "ShouldActorInit[EN_ITEM00] has a competing registrant — the CustomItem probe above is no longer "
              "attributable; see ATTRIBUTION in this file's header (#516)");

    // ---- CustomMessage::RegisterHooks (#516 critical) ----------------------
    // Its registrant is the only code that clears loadFromMessageTable and
    // loads the staged custom message; without it MM falls through to the real
    // vanilla message table and every rando/hint textbox renders entry 0x004B.
    RC_ASSERT(SettledCountForId<GameInteractor::OnOpenText>(CUSTOM_MESSAGE_ID) > 0, 4,
              "OnOpenText[0x004B] empty after MM_Rando_Init — CustomMessage::RegisterHooks never ran, "
              "every rando/hint textbox renders vanilla message 0x004B (#516)");

    // ---- RegisterSavingEnhancements (#516 Phase 2) -------------------------
    // Sole registrant of BeforeMoonCrashSaveReset. Also carries the OnSaveLoad
    // leg that seeds shipSaveContext.lastTimeLog — the seeder whose absence
    // injected a full Unix epoch into filePlaytime (#513).
    RC_ASSERT(SettledUnkeyedCount<GameInteractor::BeforeMoonCrashSaveReset>() > 0, 5,
              "BeforeMoonCrashSaveReset empty after MM_Rando_Init — RegisterSavingEnhancements never ran, "
              "owl-save persistence and moon-crash cleanup are inert (#516)");

    // ---- RegisterAutosave (#516 Phase 2; ungateable by symbol name) --------
    // Sole registrant of OnGameStateDrawFinish, and the reason this row exists
    // at all: OoT's static twin makes the CI name-grep unattributable.
    RC_ASSERT(SettledUnkeyedCount<GameInteractor::OnGameStateDrawFinish>() > 0, 6,
              "OnGameStateDrawFinish empty after MM_Rando_Init — RegisterAutosave never ran, "
              "MM has no periodic autosave and no autosave icon (#516)");

    printf("[TEST] mm-registrar-coverage: PASS (4 re-homed registrars all populated their registries)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
