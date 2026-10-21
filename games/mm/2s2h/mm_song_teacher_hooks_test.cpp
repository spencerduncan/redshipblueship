/**
 * @file mm_song_teacher_hooks_test.cpp
 * ROM-free, display-free lock for #1071: the nine song-teacher hooks under
 * games/mm/2s2h/Enhancements/Cutscenes/StoryCutscenes/SkipLearning*.cpp must
 * survive the single-exe link and arm when a rando save loads.
 * CTest label "redship", row MMSongTeacherHooks in CMake/SingleExecutable.cmake,
 * dispatch "mm-song-teacher-hooks" in src/common/test_runner.cpp.
 *
 * WHAT WAS BROKEN (#1071). Each of the nine files registers only through a
 * file-scope `RegisterShipInitFunc` keyed on the Story cutscene CVar and on
 * "IS_RANDO", and exports nothing anything else references. They lived in
 * `2ship_enh`, a plain STATIC archive, so the linker dropped all nine objects
 * (none appears in redship.map: the #516 elided-provider class). For the
 * randomizer that is not a missing time-saver. The hooks are forced on under
 * IS_RANDO because they ARE the rando hand-off for those checks: the Sonata
 * hook is the only caller of VB_GIVE_ITEM_FROM_MNK, which
 * Rando/ActorBehavior/EnMnk.cpp waits on to make RC_DEKU_KINGS_CHAMBER_MONKEY
 * eligible; the Elegy hook is the only caller of VB_GIVE_ITEM_FROM_KNIGHT; the
 * Epona hook the only caller of VB_GIVE_ITEM_FROM_ROMANI; Soaring and Storms
 * set their checks eligible themselves and have no stand-in at all. Without
 * them the vanilla teacher teaches the vanilla song and the seed's item on that
 * check is never given.
 *
 * WHAT THIS ROW DELIBERATELY DOES NOT NAME. No `RegisterSkipLearning*` symbol
 * and nothing else those nine TUs define. A reference from this always-linked
 * TU would pull the objects in by itself and the row would pass with the CMake
 * carve-out reverted -- the discipline mm_clock_shuffle_songs_test.cpp and
 * mm_registrar_coverage_test.cpp follow. What it reads instead is the
 * S2H::GameHooks registries and the observable effects of the VB dispatch,
 * which only those TUs' registrars can produce.
 *
 * HOW IT ARMS THEM. Through the production rando arm path: saveType set to
 * SAVETYPE_RANDO and `ShipInit::Init("IS_RANDO")`, exactly what
 * Rando/Rando.cpp's OnSaveLoadHandler runs on every MM save load. The Story
 * CVar is held at 0 for the whole row, so only the `CVAR || IS_RANDO` gates can
 * arm, and the CVar-only cutscene skips sharing these VB ids stay down.
 *
 * ATTRIBUTION -- why each probe names exactly one of the nine.
 *   Registry legs (the count must rise by EXACTLY one when rando arms; the
 *   ShipInit "IS_RANDO" drive re-runs only ShipInit registrars, so the
 *   Rando/ActorBehavior stand-ins armed from OnFileLoad cannot move the delta):
 *   - ShouldVanillaBehavior[VB_QUEUE_CUTSCENE]: the only `IS_RANDO` registrant
 *     tree-wide is SkipLearningEponasSong.cpp (SkipEponaReveal,
 *     SkipBombBagTheftCutscene, SkipHealingDarmani and SkipIkanaCurseCutscenes
 *     are CVar-only).
 *   - ShouldVanillaBehavior[VB_JG_THINK_YOU_KNOW_LULLABY]: the only ShipInit
 *     registrant is SkipLearningGoronLullabyIntro.cpp (EnJg.cpp's is armed
 *     from OnFileLoad, not from ShipInit).
 *   - ShouldVanillaBehavior[VB_OSN_TEACH_SONG_OF_HEALING]: sole registrant
 *     tree-wide is SkipLearningSongOfHealing.cpp.
 *   - OnActorUpdate[ACTOR_EN_GO]: sole registrant tree-wide is
 *     SkipLearningGoronLullaby.cpp.
 *   - OnActorInit[ACTOR_EN_TIME_TAG] and OnOpenText[0x0C02]: sole registrant of
 *     each tree-wide is SkipLearningSongOfSoaring.cpp.
 *   Behaviour legs (VB_START_CUTSCENE has dozens of registrants, so a count
 *   cannot attribute; the dispatch does). Each sets up the (scene, csId) only
 *   one of the nine answers, against a calloc'd PlayState, and requires the
 *   verdict to flip from "play the cutscene" to "skip it":
 *   - SCENE_DEKU_KING csId 11: SkipLearningSonataOfAwakening.cpp.
 *   - SCENE_IKNINSIDE csId 10: SkipLearningElegyOfEmptiness.cpp.
 *   - SCENE_LABO csId 11: SkipLearningNewWaveBossaNova.cpp, which also sets
 *     WEEKEVENTREG_20_40 (the check's flag, Rando/StaticData/Checks.cpp).
 *   - SCENE_HAKASHITA csId 13: SkipLearningSongOfStorms.cpp, which under rando
 *     marks RC_BENEATH_THE_GRAVEYARD_SONG_OF_STORMS eligible -- the check's
 *     only hand-off.
 *
 * NON-VACUITY. Leg 1 disarms (saveType vanilla, the same "IS_RANDO" drive) and
 * requires every behaviour probe to read "play the cutscene" and the Storms
 * check to stay ineligible, so a registrant left armed by an earlier row in a
 * shared `--test all` process cannot satisfy leg 2. On origin/main without the
 * carve-out every leg-2 probe reads its leg-1 value and the row fails at the
 * first one (FAIL 10).
 *
 * LEG 3 -- the play-state guards (PR #673's criterion, the #516 SIGSEGV class).
 * With MM_gPlayState NULL, the Epona and Healing VB ids are dispatched while
 * armed; their hooks deref the play state on the first line, so an unguarded
 * hook crashes the row and a guarded one leaves the verdict untouched. Only
 * these two VB ids are driven null: both have no registrant outside the nine.
 * VB_START_CUTSCENE is not, because Rando/ActorBehavior/EnOsn.cpp's armed
 * registrant (outside this lane) reads MM_gPlayState->sceneId unguarded and a
 * shared `--test all` process may have armed it.
 *
 * LEG 4 IS ALSO THE CLEANUP. The saved SaveContext image is copied back, the
 * "IS_RANDO" drive re-runs under the restored saveType, the events the probes
 * queued are dropped, and MM_gPlayState and the Story CVar are put back, so the
 * row is order-free with respect to every other row.
 *
 * WHAT THIS DOES NOT COVER. That each teacher then hands over the seed's item
 * in play (the queued give, the check tracker, the get-item cutscene) needs a
 * real play state, an ocarina and a rando save. It is the operator playtest in
 * the PR.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
/**
 * The S2H registries, the ShipInit split and the plain-archive elision this lock
 * is about exist only in the single-exe build. test_runner.cpp declares this
 * unconditionally.
 */
extern "C" int MM_SongTeacherHooks_RunHeadless(void) {
    printf("[TEST] mm-song-teacher-hooks: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <cstdlib>
#include <cstring>
#include <memory>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/ShipInit.hpp"
#include "mm_game_hooks.h"

extern "C" {
#include "variables.h"
#include "functions.h"
}

namespace {

#define ST_ASSERT(cond, code, msg)                                                      \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            printf("[TEST] FAIL(%d): %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                              \
        }                                                                               \
    } while (0)

// The CVar every one of the nine is keyed on besides "IS_RANDO". Held at 0 so
// that only their `CVAR || IS_RANDO` gates can arm (see the file header).
constexpr const char* kStoryCVar = "gEnhancements.TimeSavers.SkipCutscene.Story";
// SkipLearningSongOfSoaring.cpp's ENGRAVING_TEXT_ID.
constexpr int32_t kSoaringEngravingTextId = 0x0C02;

// Settled counts: S2H::GameHooks unregistration is deferred until the next
// Execute of that hook type, and the COND_* macros unregister-then-register on
// every drive, so the raw maps can credit a hook already slated for removal.
template <typename H> size_t SettledCountForId(int32_t forId) {
    S2H::GameHooks::FlushPendingUnregistrations<H>();
    auto& buckets = S2H::GameHooks::Registry<H>::functionsForID;
    auto it = buckets.find(forId);
    return it == buckets.end() ? 0u : it->second.size();
}

struct RegistryProbe {
    const char* song;
    size_t (*count)();
    size_t before;
};

size_t EponaCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_QUEUE_CUTSCENE);
}
size_t LullabyIntroCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_JG_THINK_YOU_KNOW_LULLABY);
}
size_t HealingCount() {
    return SettledCountForId<GameInteractor::ShouldVanillaBehavior>(VB_OSN_TEACH_SONG_OF_HEALING);
}
size_t LullabyCount() {
    return SettledCountForId<GameInteractor::OnActorUpdate>(ACTOR_EN_GO);
}
size_t SoaringActorCount() {
    return SettledCountForId<GameInteractor::OnActorInit>(ACTOR_EN_TIME_TAG);
}
size_t SoaringTextCount() {
    return SettledCountForId<GameInteractor::OnOpenText>(kSoaringEngravingTextId);
}

// The production rando arm path (Rando/Rando.cpp OnSaveLoadHandler).
void DriveRandoArmState(bool rando) {
    gSaveContext.save.shipSaveInfo.saveType = rando ? SAVETYPE_RANDO : SAVETYPE_VANILLA;
    ShipInit::Init("IS_RANDO");
}

// Dispatch VB_START_CUTSCENE the way z_eventmgr.c does, in `sceneId`.
bool StartCutsceneVerdict(PlayState* play, s16 sceneId, s16 csId) {
    play->sceneId = sceneId;
    s16 cs = csId;
    return GameInteractor_Should(VB_START_CUTSCENE, true, &cs, (Actor*)nullptr);
}

struct CutsceneProbe {
    const char* song;
    s16 sceneId;
    s16 csId;
};

const CutsceneProbe kCutsceneProbes[] = {
    { "Sonata of Awakening (Deku King's chamber monkey)", SCENE_DEKU_KING, 11 },
    { "Elegy of Emptiness (Igos du Ikana)", SCENE_IKNINSIDE, 10 },
    { "New Wave Bossa Nova (Marine Research Lab)", SCENE_LABO, 11 },
    { "Song of Storms (Flat's tomb)", SCENE_HAKASHITA, 13 },
};

int RunLegs(PlayState* play) {
    RegistryProbe registryProbes[] = {
        { "Epona's Song: VB_QUEUE_CUTSCENE", EponaCount, 0 },
        { "Goron Lullaby Intro: VB_JG_THINK_YOU_KNOW_LULLABY", LullabyIntroCount, 0 },
        { "Song of Healing: VB_OSN_TEACH_SONG_OF_HEALING", HealingCount, 0 },
        { "Goron Lullaby: OnActorUpdate[ACTOR_EN_GO]", LullabyCount, 0 },
        { "Song of Soaring: OnActorInit[ACTOR_EN_TIME_TAG]", SoaringActorCount, 0 },
        { "Song of Soaring: OnOpenText[0x0C02]", SoaringTextCount, 0 },
    };

    // ---- Leg 1: disarmed baseline (non-vacuity) ----------------------------
    DriveRandoArmState(false);
    for (auto& probe : registryProbes) {
        probe.before = probe.count();
    }
    MM_gPlayState = play;
    for (const auto& probe : kCutsceneProbes) {
        if (!StartCutsceneVerdict(play, probe.sceneId, probe.csId)) {
            printf("[TEST] FAIL(2): %s cutscene is already skipped with rando disarmed and the Story CVar off - "
                   "the leg-2 probe would be vacuous\n",
                   probe.song);
            return 2;
        }
    }
    ST_ASSERT(!RANDO_SAVE_CHECKS[RC_BENEATH_THE_GRAVEYARD_SONG_OF_STORMS].eligible, 2,
              "the Song of Storms check became eligible with rando disarmed - the leg-2 probe would be vacuous");
    ST_ASSERT(!CHECK_WEEKEVENTREG(WEEKEVENTREG_20_40), 2,
              "WEEKEVENTREG_20_40 got set with rando disarmed - the leg-2 Bossa Nova probe would be vacuous");

    // ---- Leg 2: rando armed through the production path --------------------
    DriveRandoArmState(true);
    for (const auto& probe : kCutsceneProbes) {
        if (StartCutsceneVerdict(play, probe.sceneId, probe.csId)) {
            printf("[TEST] FAIL(10): with rando armed, the %s cutscene still plays - its SkipLearning*.cpp hook is "
                   "not in the link or never armed, so the vanilla teacher teaches the vanilla song and the "
                   "seed's item on that check is never given (#1071)\n",
                   probe.song);
            return 10;
        }
    }
    ST_ASSERT(RANDO_SAVE_CHECKS[RC_BENEATH_THE_GRAVEYARD_SONG_OF_STORMS].eligible, 11,
              "the Song of Storms hook skipped the cutscene but did not make its check eligible (#1071)");
    ST_ASSERT(CHECK_WEEKEVENTREG(WEEKEVENTREG_20_40), 12,
              "the New Wave Bossa Nova hook skipped the cutscene but did not set WEEKEVENTREG_20_40, the "
              "check's flag (#1071)");
    for (const auto& probe : registryProbes) {
        const size_t after = probe.count();
        if (after != probe.before + 1) {
            printf("[TEST] FAIL(13): %s went %zu -> %zu when rando armed, expected exactly +1 - its "
                   "SkipLearning*.cpp registrar is not in the link or never armed, or a second IS_RANDO "
                   "registrant appeared and the probe is no longer attributable (#1071)\n",
                   probe.song, probe.before, after);
            return 13;
        }
    }

    // ---- Leg 3: play-state guards ------------------------------------------
    MM_gPlayState = nullptr;
    {
        s16 cs = 14; // Epona reveal
        ST_ASSERT(GameInteractor_Should(VB_QUEUE_CUTSCENE, true, &cs), 14,
                  "VB_QUEUE_CUTSCENE changed the verdict with no play state - the Epona hook is not guarded");
    }
    ST_ASSERT(GameInteractor_Should(VB_OSN_TEACH_SONG_OF_HEALING, true, (void*)nullptr), 14,
              "VB_OSN_TEACH_SONG_OF_HEALING changed the verdict with no play state - the Healing hook is not "
              "guarded");
    return 0;
}

} // namespace

extern "C" int MM_SongTeacherHooks_RunHeadless(void) {
    printf("[TEST] mm-song-teacher-hooks: the nine SkipLearning*.cpp song-teacher hooks are linked and arm under "
           "rando (#1071)\n");

    auto ctx = Ship::Context::GetInstance();
    ST_ASSERT(ctx != nullptr, 1, "Ship::Context singleton missing - run the shared bring-up first");
    ST_ASSERT(ctx->GetConsoleVariables() != nullptr, 1, "ConsoleVariables missing - the registrars read CVars");

    // Everything the legs write, saved so leg 4 can put it back.
    auto saved = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(saved.get(), &gSaveContext, sizeof(gSaveContext));
    PlayState* const savedPlay = MM_gPlayState;
    const int32_t savedStory = CVarGetInteger(kStoryCVar, 0);
    auto& queue = MM_GameEvents_Queue();
    const size_t savedQueueDepth = queue.size();

    PlayState* play = static_cast<PlayState*>(calloc(1, sizeof(PlayState)));
    ST_ASSERT(play != nullptr, 1, "could not allocate a PlayState");

    if (savedStory != 0) {
        CVarSetInteger(kStoryCVar, 0);
    }
    RANDO_SAVE_CHECKS[RC_BENEATH_THE_GRAVEYARD_SONG_OF_STORMS].eligible = false;
    CLEAR_WEEKEVENTREG(WEEKEVENTREG_20_40);

    const int rc = RunLegs(play);

    // ---- Leg 4: cleanup (runs on pass and on fail) -------------------------
    MM_gPlayState = savedPlay;
    if (queue.size() > savedQueueDepth) {
        queue.erase(queue.begin() + static_cast<std::ptrdiff_t>(savedQueueDepth), queue.end());
    }
    memcpy(&gSaveContext, saved.get(), sizeof(gSaveContext));
    ShipInit::Init("IS_RANDO");
    if (savedStory != 0) {
        CVarSetInteger(kStoryCVar, savedStory);
    }
    free(play);

    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-song-teacher-hooks: PASS (four cutscene skips fire under rando, Storms and Bossa Nova hand "
           "off their checks, six single-registrant hooks armed, Epona and Healing guarded against a missing "
           "play state)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
