/**
 * @file mm_enhancement_toggles_test.cpp
 * ROM-free, display-free lock for #682's EVIDENCE half: every key the unified
 * menu now exposes for Majora's Mask must actually reach a live provider, and
 * the key the widget writes must be the key MM re-arms on.
 * CTest label "redship", row MMEnhancementToggles in CMake/SingleExecutable.cmake,
 * dispatch "mm-enhancement-toggles" in src/common/test_runner.cpp.
 *
 * WHAT #682 IS ABOUT. `games/mm/2s2h/BenGui/BenMenu.cpp` is excluded from the
 * single exe, so MM's enhancement toggles had no widget anywhere in the binary.
 * The issue's own constraint is the one that matters here: "a toggle that does
 * nothing is worse than no toggle", because #673 wired MM's dispatchers while
 * most `2ship_enh` registrants stayed link-elided (#427 item 3). So a row is only
 * allowed onto the page with ADR 0004 section 5's three legs measured — the
 * provider TU links, its registrar runs, the hook type it rides has an MM
 * dispatch point — and this row is where two of the three are measured at
 * runtime, per key, on every platform.
 *
 * THE MANIFEST IS THE INPUT. `RSBS::kHostedMmEnhancements`
 * (src/common/cvar_shared_keys.h) is what the menu page draws from and what this
 * row iterates. Driving the row off the same table is the point: a key added to
 * the allowlist without its evidence fails here rather than shipping as a
 * decorative checkbox, and a key whose `registrarKey` drifts from the CVar the
 * widget writes fails at leg 1 — which is not cosmetic, because #539's forward is
 * keyed on the EXACT name OoT's menu just wrote (`MM_ShipInit_OnCVarChanged`), so
 * a mismatch means clicking the row re-arms nothing at all.
 *
 * WHAT THIS ROW DELIBERATELY DOES NOT NAME. It never mentions `RegisterAutosave`,
 * `RegisterBetterSongOfDoubleTime`, `RegisterSkipSoTCutscenes`,
 * `RegisterSavingEnhancements` or any symbol inside a provider TU. A reference
 * from this always-linked TU would itself keep those objects past /OPT:REF, so
 * the row would pass with the link contract reverted — the discipline
 * mm_registrar_coverage_test.cpp and mm_clock_shuffle_songs_test.cpp both follow.
 * What it reads instead is MM's own `S2H::ShipInit` map and the `S2H::GameHooks`
 * registries, which only those TUs' static initializers and registrars populate.
 *
 * ATTRIBUTION -- why each probe names exactly one registrar. The manifest carries
 * the probe name per row (`registryProbe`) and this is the argument for each:
 *   - `OnGameStateDrawFinish` (gEnhancements.Autosave): exactly one registrant in
 *     the whole MM tree, inside SavingEnhancements.cpp's CVar-gated autosave
 *     branch. This is the ONLY possible gate for that registrar, because OoT
 *     ships a static `RegisterAutosave` of its own and a demangled-name grep
 *     cannot tell the two apart — the reason
 *     .github/scripts/check-registrar-elision.sh leaves it off its allowlist
 *     and delegates to registry content.
 *   - `ShouldVanillaBehavior[VB_DISPLAY_SONG_OF_DOUBLE_TIME_PROMPT]`
 *     (gEnhancements.Songs.BetterSongOfDoubleTime): sole registrant tree-wide.
 *   - `OnActorUpdate[ACTOR_EN_TEST6]` (gEnhancements.Songs.SkipSoTCutscenes):
 *     sole registrant tree-wide. BetterSongOfDoubleTime.cpp also keys on
 *     ACTOR_EN_TEST6, but on `OnActorKill`, and only from inside a live
 *     Song-of-Double-Time selection this headless row never enters.
 * Each is asserted to hold EXACTLY ONE registrant rather than a non-zero count,
 * so a future second registrant fails loudly as a precondition violation instead
 * of silently propping the probe up. And each key is armed ONE AT A TIME with the
 * others forced off, so a probe can only be satisfied by its own key's
 * registrar — without that, three keys sharing one "something registered"
 * assertion would pass with two of them misspelled.
 *
 * NON-VACUITY. Leg 2 drives every registrar with its CVar forced OFF and requires
 * all three registries to settle EMPTY first, so a registry populated by an
 * earlier row in a shared `--test all` process cannot satisfy leg 3. Leg 4 puts
 * them all back to empty, which is both the cleanup that shared process needs and
 * a real assertion: it is what shows the `COND_*` gate is re-evaluated on each
 * drive rather than latched at first boot, which is the #539 defect one layer
 * down. (MMRegistrarCoverage asserts `OnGameStateDrawFinish` is EMPTY before its
 * own bring-up, so leaving autosave armed here would fail that row instead of
 * this one — the two are order-independent only because leg 4 exists.)
 *
 * LEG 5 IS A DIFFERENT KIND OF PROBE, and the difference is the point.
 * `gEnhancements.Kaleido.GameOver` has NO registrar: both readers are inline
 * `CVarGetInteger` calls in MM's decomp, on paths `z_play.c` drives every frame,
 * so there is nothing to arm and no registry to count. What can be measured is
 * the READ SITE's effect, which is exactly the lock #653's triage asked for: drive
 * the real `MM_GameOver_Update` from `GAMEOVER_DEATH_FADE_OUT` with the key
 * cleared and then set, and assert the branch it selects — vanilla reload
 * (`respawnFlag == -6`, `health == 0x30`, state advanced) versus the kaleido
 * prompt arm (none of those touched). It is NOT a link probe and does not pretend
 * to be: this TU names `MM_GameOver_Update`, which is an inbound reference, and
 * that is harmless only because `z_play.c:1173` already calls it and the TU lives
 * in `2ship_src` rather than in an elidable enhancement archive.
 *
 * WHAT THIS ROW DOES NOT COVER. That an enabled toggle produces the right
 * behaviour in play. Song of Double Time refusing an unowned half-day, the
 * game-over prompt's own artwork (`icon_item_gameover_static` /
 * `icon_item_jpn_static` have never been loaded in this repository, #682), and
 * the periodic owl save actually landing all need a ROM, a play state and an
 * operator. The menu-side half — that a row exists, bound to the manifest key, in
 * the manifest's presentation class — is MenuMmEnhancementRows.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
/**
 * `S2H::ShipInit`, `S2H::GameHooks` and the BenMenu exclusion this lock is about
 * all exist only in the single-exe build. A standalone 2ship build compiles its
 * own menu and nothing is elided, so the row reports pass rather than failing to
 * link. test_runner.cpp declares this unconditionally.
 */
extern "C" int MM_EnhancementToggles_RunHeadless(void) {
    printf("[TEST] mm-enhancement-toggles: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "mm_game_hooks.h"

#include "cvar_shared_keys.h"

#include <cstring>
#include <string>
#include <vector>

extern "C" {
// Production accessors over MM's registrar map (2s2h/ShipInitBridge_SingleExe.cpp).
// Declared, never defined here. RegistrarCountForPath is non-inserting, unlike
// ShipInit::Init's operator[]; OnCVarChanged is the exact entry point OoT's
// unified menu reaches MM's registrars through (#539), so the legs below drive
// the real path a player's click takes rather than a test-only shortcut.
void MM_ShipInit_OnCVarChanged(const char* path);
int MM_ShipInit_RegistrarCountForPath(const char* path);
}

namespace {

#define ET_ASSERT(cond, code, msg)                                                      \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

// Settled counts. Unregistration in S2H::GameHooks is DEFERRED -- Unregister only
// queues an id, drained at the next Execute of that hook type -- and both the
// COND_* macros and RegisterAutosave unregister-then-register on every drive.
// Counting the raw maps without flushing would credit a hook already slated for
// removal, which is exactly what leg 4 must not do.
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

/**
 * The three registrar-backed manifest keys, each paired with the registry whose
 * content attributes its registrar. The manifest carries the probe NAME as a
 * string; this table is the one place that string is bound to the actual
 * template instantiation, and leg 0 asserts the two agree so a renamed probe
 * cannot silently keep measuring the old registry.
 */
struct ProbeBinding {
    const char* key;
    const char* probeName;
    size_t (*settledCount)();
};

size_t AutosaveProbe() {
    return SettledUnkeyedCount<GameInteractor::OnGameStateDrawFinish>();
}
size_t BsodtProbe() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_DISPLAY_SONG_OF_DOUBLE_TIME_PROMPT);
}
size_t SkipSotProbe() {
    return SettledCountForId<GameInteractor::OnActorUpdate>(ACTOR_EN_TEST6);
}

constexpr ProbeBinding kProbes[] = {
    { "gEnhancements.Autosave", "OnGameStateDrawFinish", AutosaveProbe },
    { "gEnhancements.Songs.BetterSongOfDoubleTime", "ShouldVanillaBehavior[VB_DISPLAY_SONG_OF_DOUBLE_TIME_PROMPT]",
      BsodtProbe },
    { "gEnhancements.Songs.SkipSoTCutscenes", "OnActorUpdate[ACTOR_EN_TEST6]", SkipSotProbe },
};
constexpr size_t kProbeCount = sizeof(kProbes) / sizeof(kProbes[0]);

/** Set a key and re-drive MM's registrars for it through the production path,
 *  mirroring exactly what a unified-menu widget does on a click (#539). */
void SetAndDrive(const char* key, int value) {
    CVarSetInteger(key, value);
    MM_ShipInit_OnCVarChanged(key);
}

/** All three registrar keys off, each driven, so every probed registry settles
 *  empty. Used as leg 2 and again as leg 4's cleanup. */
void DriveAllOff() {
    for (size_t i = 0; i < kProbeCount; i++) {
        SetAndDrive(kProbes[i].key, 0);
    }
}

// ---- Leg 5's state snapshot -------------------------------------------------
// MM_GameOver_Update's vanilla arm writes global save state and the rumble
// manager. `--test all` shares one process, so the leg puts every field it can
// reach back rather than leaving a dead Link and a queued respawn behind for
// whichever row runs next.
struct GameOverSnapshot {
    s32 respawnFlag; // z64save.h: s32, not s16 — -6 and -7 are its sentinels
    u8 nextTransitionType;
    s16 health;
    u8 playerForm;
    u8 equippedMask;
    u8 rumbleState;
};

GameOverSnapshot SnapshotGameOverState() {
    GameOverSnapshot s;
    s.respawnFlag = gSaveContext.respawnFlag;
    s.nextTransitionType = gSaveContext.nextTransitionType;
    s.health = gSaveContext.save.saveInfo.playerData.health;
    s.playerForm = gSaveContext.save.playerForm;
    s.equippedMask = gSaveContext.save.equippedMask;
    s.rumbleState = gRumbleMgr.state;
    return s;
}

void RestoreGameOverState(const GameOverSnapshot& s) {
    gSaveContext.respawnFlag = s.respawnFlag;
    gSaveContext.nextTransitionType = s.nextTransitionType;
    gSaveContext.save.saveInfo.playerData.health = s.health;
    gSaveContext.save.playerForm = s.playerForm;
    gSaveContext.save.equippedMask = s.equippedMask;
    gRumbleMgr.state = s.rumbleState;
}

} // namespace

extern "C" int MM_EnhancementToggles_RunHeadless(void) {
    printf("[TEST] mm-enhancement-toggles: every curated MM enhancement key (#682) reaches a live provider, and the "
           "key the menu writes is the key MM re-arms on\n");

    // ---- Leg 0: the manifest and the probe table agree ----------------------
    // Both directions. A manifest row with a registrar must have a probe here,
    // and a probe here must correspond to a manifest row -- otherwise this file
    // could keep measuring a key the menu no longer exposes, or stop measuring
    // one it does.
    size_t manifestRegistrarRows = 0;
    for (size_t i = 0; i < RSBS::kHostedMmEnhancementCount; i++) {
        const RSBS::HostedMmEnhancement& row = RSBS::kHostedMmEnhancements[i];
        if (row.registrarKey == nullptr) {
            continue;
        }
        manifestRegistrarRows++;

        // The CVar the widget writes MUST be the ShipInit path MM registered on.
        // #539's forward is a find() on the exact name, so a mismatch is a click
        // that re-arms nothing -- and nothing about it is a compile error.
        ET_ASSERT(std::strcmp(row.registrarKey, row.key) == 0, 1,
                  "a manifest row's registrarKey differs from the CVar the menu row writes; MM_ShipInit_OnCVarChanged "
                  "looks the path up by exact name, so the click would re-arm nothing");

        const ProbeBinding* binding = nullptr;
        for (size_t p = 0; p < kProbeCount; p++) {
            if (std::strcmp(kProbes[p].key, row.key) == 0) {
                binding = &kProbes[p];
            }
        }
        if (binding == nullptr) {
            printf("[TEST] FAIL(1): manifest key \"%s\" has a registrar but no registry probe in this row; its "
                   "liveness claim is unmeasured\n",
                   row.key);
            return 1;
        }
        ET_ASSERT(std::strcmp(binding->probeName, row.registryProbe) == 0, 1,
                  "the manifest's registryProbe name and this row's probe table disagree about which registry "
                  "attributes a key's registrar");
    }
    ET_ASSERT(manifestRegistrarRows == kProbeCount, 1,
              "the probe table and the manifest disagree on how many keys have registrars -- a key was added to one "
              "and not the other");

    // ---- Leg 1: the provider TU linked AND its static initializer ran --------
    // A RegisterShipInitFunc entry under the key exists only if the provider's
    // file-scope object was constructed, which requires the object to have
    // survived the link. That is ADR 0004 section 5 legs 1 and 2 in one
    // observation, and it needs no bring-up: the map is populated at static
    // init.
    for (size_t p = 0; p < kProbeCount; p++) {
        const int registrars = MM_ShipInit_RegistrarCountForPath(kProbes[p].key);
        if (registrars != 1) {
            printf("[TEST] FAIL(2): MM's ShipInit map holds %d registrars under \"%s\", expected exactly 1. Zero means "
                   "the provider TU was elided from the single-exe link or its RegisterShipInitFunc was removed, and "
                   "the menu row #682 added is a checkbox over nothing; more than one means a second registrant "
                   "appeared and the attribution below is no longer exact\n",
                   registrars, kProbes[p].key);
            return 2;
        }
    }
    // The registrar-LESS row is asserted too, in the other direction: if
    // something ever gives Kaleido.GameOver a ShipInit registrar, the manifest's
    // claim that it needs no arming is stale and the reason it gives for having
    // no probe is wrong.
    for (size_t i = 0; i < RSBS::kHostedMmEnhancementCount; i++) {
        const RSBS::HostedMmEnhancement& row = RSBS::kHostedMmEnhancements[i];
        if (row.registrarKey != nullptr) {
            continue;
        }
        ET_ASSERT(MM_ShipInit_RegistrarCountForPath(row.key) == 0, 2,
                  "a manifest row declares it needs no registrar, but MM's ShipInit map holds one under its key");
    }
    printf("[TEST] leg 1: each registrar-backed key has exactly one ShipInit registrar (provider linked, initializer "
           "ran)\n");

    // ---- Leg 2: non-vacuity. Every probed registry settles EMPTY -------------
    DriveAllOff();
    for (size_t p = 0; p < kProbeCount; p++) {
        if (kProbes[p].settledCount() != 0) {
            printf("[TEST] FAIL(3): %s is populated with \"%s\" forced OFF -- leg 3 would be vacuous (either the "
                   "COND_* gate is not re-evaluated on a drive, which is #539's defect, or a second registrant this "
                   "row cannot attribute has appeared)\n",
                   kProbes[p].probeName, kProbes[p].key);
            return 3;
        }
    }
    printf("[TEST] leg 2: all three registries settle empty with their keys off\n");

    // ---- Leg 3: one key at a time, and only its own registry arms ------------
    for (size_t p = 0; p < kProbeCount; p++) {
        SetAndDrive(kProbes[p].key, 1);

        if (kProbes[p].settledCount() != 1) {
            printf("[TEST] FAIL(4): %s holds %zu registrants after arming \"%s\" through the production "
                   "MM_ShipInit_OnCVarChanged path, expected exactly 1. Zero means the registrar linked but never ran, "
                   "or is keyed on a different path than the menu writes; more than one means a second registrant has "
                   "appeared and this probe no longer names one registrar\n",
                   kProbes[p].probeName, kProbes[p].settledCount(), kProbes[p].key);
            DriveAllOff();
            return 4;
        }

        // ATTRIBUTION, the half a shared "something registered" assertion cannot
        // do: the OTHER registries must still be empty. Without this, three keys
        // could share one live registrar and two of them be misspelled.
        for (size_t q = 0; q < kProbeCount; q++) {
            if (q == p) {
                continue;
            }
            if (kProbes[q].settledCount() != 0) {
                printf("[TEST] FAIL(5): arming \"%s\" also populated %s, which belongs to \"%s\". The probes no "
                       "longer attribute one registry to one key\n",
                       kProbes[p].key, kProbes[q].probeName, kProbes[q].key);
                DriveAllOff();
                return 5;
            }
        }

        SetAndDrive(kProbes[p].key, 0);
        if (kProbes[p].settledCount() != 0) {
            printf("[TEST] FAIL(6): %s still holds %zu registrants after \"%s\" was turned back off and re-driven. "
                   "The gate is latched rather than re-evaluated, which is #539's defect one layer down\n",
                   kProbes[p].probeName, kProbes[p].settledCount(), kProbes[p].key);
            DriveAllOff();
            return 6;
        }
    }
    printf("[TEST] leg 3: each key arms its own registry and only its own, and disarms again on a second drive\n");

    // ---- Leg 4: the process is left as this row found it ---------------------
    DriveAllOff();
    for (size_t p = 0; p < kProbeCount; p++) {
        ET_ASSERT(kProbes[p].settledCount() == 0, 7,
                  "a registry is still armed at the end of this row; MMRegistrarCoverage asserts "
                  "OnGameStateDrawFinish is empty before its own bring-up, so a leak here fails that row instead");
    }
    printf("[TEST] leg 4: every registry disarmed, CVars back to 0\n");

    // ---- Leg 5: the registrar-less key's READ SITE --------------------------
    // #653's recommended lock, and the only ROM-free evidence available for a key
    // whose whole implementation is two inline CVarGetInteger reads. Zeroed
    // PlayState on the heap: MM's PlayState is far too large for a stack frame,
    // and every field MM_GameOver_Update touches on this path is either written
    // by it or read as zero.
    {
        constexpr const char* kGameOverKey = "gEnhancements.Kaleido.GameOver";
        const GameOverSnapshot saved = SnapshotGameOverState();
        std::vector<char> storage(sizeof(PlayState), 0);
        PlayState* play = reinterpret_cast<PlayState*>(storage.data());
        int failures = 0;

        // --- key CLEARED: the vanilla reload arm (the operator's 2026-09-16
        //     ruling, and the behaviour #653 reported as a regression it is not).
        CVarSetInteger(kGameOverKey, 0);
        gSaveContext.respawnFlag = 0;
        gSaveContext.save.saveInfo.playerData.health = 0;
        play->gameOverCtx.state = GAMEOVER_DEATH_FADE_OUT;
        MM_GameOver_Update(play);

        if (gSaveContext.respawnFlag != -6) {
            printf("[TEST] FAIL(8): with \"%s\" cleared, GAMEOVER_DEATH_FADE_OUT left respawnFlag %d, expected -6 "
                   "(func_80169F78's reload-to-last-entrance). The read site at z_game_over.c:84 no longer selects "
                   "the vanilla arm, so the toggle's OFF state does not mean what #653 ruled it means\n",
                   (int)gSaveContext.respawnFlag);
            failures++;
        }
        if (gSaveContext.save.saveInfo.playerData.health != 0x30) {
            printf("[TEST] FAIL(8): with \"%s\" cleared, health is %d, expected 0x30 (three hearts)\n", kGameOverKey,
                   (int)gSaveContext.save.saveInfo.playerData.health);
            failures++;
        }
        if (play->gameOverCtx.state == GAMEOVER_DEATH_FADE_OUT) {
            printf("[TEST] FAIL(8): with \"%s\" cleared, the game-over state did not advance past "
                   "GAMEOVER_DEATH_FADE_OUT\n",
                   kGameOverKey);
            failures++;
        }
        if (play->pauseCtx.state != 0) {
            printf("[TEST] FAIL(8): with \"%s\" cleared, pauseCtx.state is %d; the vanilla arm must never open the "
                   "kaleido game-over page\n",
                   kGameOverKey, (int)play->pauseCtx.state);
            failures++;
        }

        // --- key SET: the kaleido prompt arm. It only counts down a file-static
        //     timer until it reaches zero, so what is asserted is that NONE of
        //     the vanilla writes happened -- which is the whole difference the
        //     key makes, and the reason #625/#626 and ADR 0009 4a/4b describe a
        //     screen a default build cannot show.
        std::memset(storage.data(), 0, storage.size());
        CVarSetInteger(kGameOverKey, 1);
        gSaveContext.respawnFlag = 0;
        gSaveContext.save.saveInfo.playerData.health = 0;
        play->gameOverCtx.state = GAMEOVER_DEATH_FADE_OUT;
        MM_GameOver_Update(play);

        if (gSaveContext.respawnFlag != 0) {
            printf("[TEST] FAIL(9): with \"%s\" set, respawnFlag became %d; the kaleido arm must not queue a respawn\n",
                   kGameOverKey, (int)gSaveContext.respawnFlag);
            failures++;
        }
        if (gSaveContext.save.saveInfo.playerData.health != 0) {
            printf("[TEST] FAIL(9): with \"%s\" set, health became %d; the kaleido arm must not refill Link\n",
                   kGameOverKey, (int)gSaveContext.save.saveInfo.playerData.health);
            failures++;
        }
        if (play->gameOverCtx.state != GAMEOVER_DEATH_FADE_OUT) {
            printf("[TEST] FAIL(9): with \"%s\" set, the game-over state advanced to %d; the kaleido arm advances only "
                   "once its own timer reaches zero\n",
                   kGameOverKey, (int)play->gameOverCtx.state);
            failures++;
        }

        // Cleanup: the key back to its default-off, and the save fields this leg
        // wrote back to what they were.
        CVarSetInteger(kGameOverKey, 0);
        RestoreGameOverState(saved);

        if (failures != 0) {
            printf("[TEST] mm-enhancement-toggles: %d failure(s) in leg 5\n", failures);
            return 8;
        }
    }
    printf("[TEST] leg 5: gEnhancements.Kaleido.GameOver's read site selects the vanilla reload when cleared and the "
           "kaleido prompt arm when set\n");

    printf("[TEST] mm-enhancement-toggles: PASS\n");
    return 0;
}

#endif
