#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <ctime>

#include "menu.hpp"
#include "playthrough.hpp"
#include "spoiler_log.hpp"
#include "../location_access.h"
#include "soh/Enhancements/debugger/performanceTimer.h"
#include "soh/ShipUtils.h"
#include <spdlog/spdlog.h>
#include <libultraship/bridge.h>
#include "../../randomizer/randomizerTypes.h"
#include "../static_data.h"
#include "../SeedContext.h"
#include "../settings.h"
#include "../item_location.h"
#include "context.h" // src/common — gComboCtx, Lane B unified-seed carrier (ADR 0002)

namespace {
bool seedChanged;
uint16_t pastSeedLength;
std::vector<std::string> presetEntries;
Rando::Option* currentSetting;
} // namespace

// Test bridge for the RandoGen regression test (issue #337): drive seed
// generation without ImGui interaction. The real bring-up
// (InitOTRForMMFirstBoot) normally created the rando context/settings/
// randomizer already; fall back to manual init only if it didn't (mirrors
// OTRGlobals.cpp init order — InitItemTable depends on the context's Logic).
extern "C" int Rando_HeadlessSeedTest(const char* seedStr) {
    auto ctx = Rando::Context::GetInstance();
    if (!ctx) {
        ctx = Rando::Context::CreateInstance();
        ctx->InitStaticData();
        Rando::Settings::GetInstance()->AssignContext(ctx);
        Rando::StaticData::InitItemTable();
        Rando::StaticData::InitLocationTable();
    }
    // Normally SohMenuRandomizer's menu setup creates the Settings options; in
    // archive-less harness runs the GUI layer is skipped, which used to leave
    // every Option blank (empty CVar name, default 0). SetAllToContext then fed
    // all-zero settings into generation, so this test silently ran with every
    // shuffle Off instead of the real stock defaults, and RSBS_DIAG_CVARS had
    // no effect. Create the real options so the test exercises what users get.
    Rando::Settings::GetInstance()->CreateOptions();
    // Optional non-default settings, e.g.
    // RSBS_DIAG_CVARS="gRandoSettings.ShuffleSongs=2,gRandoSettings.ShuffleSpeak=1"
    if (const char* cvars = std::getenv("RSBS_DIAG_CVARS")) {
        std::stringstream ss(cvars);
        std::string pair;
        while (std::getline(ss, pair, ',')) {
            auto eq = pair.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            CVarSetInteger(pair.substr(0, eq).c_str(), std::stoi(pair.substr(eq + 1)));
            fprintf(stderr, "[rando-diag] cvar %s\n", pair.c_str());
        }
    }
    Rando::Settings::GetInstance()->SetAllToContext();
    bool ok = GenerateRandomizer({}, {}, seedStr ? seedStr : "");
    fprintf(stderr, "[rando-diag] GenerateRandomizer returned %s\n", ok ? "true" : "false");
    return ok ? 0 : 1;
}

// Hint-validity harness bridge (#441): run ONE seed generation, then prove that
// every generated hint names a REAL item — no hint may resolve to the no-item
// sentinel (itemTable[RG_NONE], "No Item"). The operator-visible bug was a
// gossip stone reading "They say that catching Big Poes leads to No Item" while
// the spoiler for that same seed correctly said "Nayru's Love", i.e. the fill
// was complete and generation-time text was right, but a later resolution of
// location -> placed item yielded RG_NONE.
//
// Three passes, deliberately layered so a failure says WHICH link broke:
//   (1) placement  — every location an item-hint points at holds a real item;
//   (2) name<->enum round trip — RC -> Location::GetName() -> locationNameToEnum
//       must return the SAME RC. This is exactly the lossy transform the save
//       file round-trips hints through (SaveManager writes hinted locations as
//       names and Hint(json) reads them back through locationNameToEnum with
//       operator[], which silently yields 0 on a miss), so a collision or a
//       missing entry shows up here instead of as "No Item" in someone's game;
//   (3) rendered text — no hint's final message may contain the sentinel, which
//       catches any resolution path the first two passes don't model.
extern "C" int Rando_HeadlessHintValidityTest(const char* seedStr) {
    int rc = Rando_HeadlessSeedTest(seedStr);
    if (rc != 0) {
        fprintf(stderr, "[rando-hints] generation failed rc=%d\n", rc);
        return rc;
    }

    auto ctx = Rando::Context::GetInstance();
    if (!ctx) {
        fprintf(stderr, "[rando-hints] no Rando::Context after generation\n");
        return 4;
    }

    // The sentinel is read from the table rather than hardcoded, so renaming
    // RG_NONE's text cannot silently defeat this lock.
    const std::string sentinel = Rando::StaticData::RetrieveItem(RG_NONE).GetName().GetEnglish();
    if (sentinel.empty()) {
        fprintf(stderr, "[rando-hints] itemTable[RG_NONE] has no name; item table not initialised\n");
        return 5;
    }

    size_t hintsChecked = 0;
    size_t failures = 0;

    for (int h = RH_NONE + 1; h < RH_MAX; h++) {
        const auto hintKey = static_cast<RandomizerHint>(h);
        Rando::Hint* hint = ctx->GetHint(hintKey);
        if (hint == nullptr || !hint->IsEnabled()) {
            continue;
        }
        hintsChecked++;

        const std::string hintName =
            Rando::StaticData::hintNames[hintKey].GetForCurrentLanguage(MF_CLEAN);
        const HintType hintType = hint->GetHintType();
        const bool namesAnItem = (hintType == HINT_TYPE_ITEM || hintType == HINT_TYPE_ITEM_AREA);

        for (const RandomizerCheck hintedCheck : hint->GetHintedLocations()) {
            // (1) An item hint that points at an empty location is the
            // under-placement half of this bug class.
            if (namesAnItem && ctx->GetItemLocation(hintedCheck)->GetPlacedRandomizerGet() == RG_NONE) {
                fprintf(stderr, "[rando-hints] FAIL %s: hinted check %d holds no item\n", hintName.c_str(),
                        (int)hintedCheck);
                failures++;
            }

            // (2) The save-file transform must be lossless for this check.
            const Rando::Location* loc = Rando::StaticData::GetLocation(hintedCheck);
            if (loc == nullptr) {
                fprintf(stderr, "[rando-hints] FAIL %s: hinted check %d has no location entry\n", hintName.c_str(),
                        (int)hintedCheck);
                failures++;
                continue;
            }
            const std::string& locName = loc->GetName();
            const auto it = Rando::StaticData::locationNameToEnum.find(locName);
            if (it == Rando::StaticData::locationNameToEnum.end()) {
                fprintf(stderr, "[rando-hints] FAIL %s: location name '%s' (check %d) is absent from "
                                "locationNameToEnum; it would load back as check 0\n",
                        hintName.c_str(), locName.c_str(), (int)hintedCheck);
                failures++;
            } else if (it->second != hintedCheck) {
                fprintf(stderr, "[rando-hints] FAIL %s: location name '%s' round-trips to check %d, not %d\n",
                        hintName.c_str(), locName.c_str(), (int)it->second, (int)hintedCheck);
                failures++;
            }
        }

        // (3) Whatever the path, the text a player reads must name a real item.
        const std::vector<std::string> liveMessages = hint->GetAllMessageStrings(MF_CLEAN);
        for (const std::string& message : liveMessages) {
            if (message.find(sentinel) != std::string::npos) {
                fprintf(stderr, "[rando-hints] FAIL %s: message resolves to the no-item sentinel: \"%s\"\n",
                        hintName.c_str(), message.c_str());
                failures++;
            }
        }

        // (4) Serialize/deserialize round trip. The spoiler proved that
        // generation-time text is correct, so the operator-visible break has to
        // be downstream of generation: a hint is written out as names and read
        // back through the name tables. toJSON()/Hint(key, json) are that
        // declared pair (the same schema the spoiler and the plando loader
        // use), so re-rendering a reloaded hint must reproduce the live text
        // exactly -- and in particular must not decay to the sentinel.
        // Dump and reparse rather than handing the ordered_json straight to the
        // constructor: it makes the nlohmann::json overload unambiguous, and it
        // is the more faithful simulation anyway, since a real save round trip
        // goes through serialized text.
        const nlohmann::json roundTripped = nlohmann::json::parse(hint->toJSON().dump());
        Rando::Hint reloaded(hintKey, roundTripped);
        const std::vector<std::string> reloadedMessages = reloaded.GetAllMessageStrings(MF_CLEAN);
        if (reloadedMessages.size() != liveMessages.size()) {
            fprintf(stderr, "[rando-hints] FAIL %s: round trip changed message count %zu -> %zu\n", hintName.c_str(),
                    liveMessages.size(), reloadedMessages.size());
            failures++;
        } else {
            for (size_t m = 0; m < reloadedMessages.size(); m++) {
                if (reloadedMessages[m] == liveMessages[m]) {
                    continue;
                }
                const bool decayed = reloadedMessages[m].find(sentinel) != std::string::npos;
                fprintf(stderr, "[rando-hints] FAIL %s: round trip changed message%s\n  live:     \"%s\"\n"
                                "  reloaded: \"%s\"\n",
                        hintName.c_str(), decayed ? " and it decayed to the no-item sentinel" : "",
                        liveMessages[m].c_str(), reloadedMessages[m].c_str());
                failures++;
            }
        }
    }

    if (hintsChecked == 0) {
        fprintf(stderr, "[rando-hints] no enabled hints were generated; the lock would vacuously pass\n");
        return 6;
    }

    fprintf(stderr, "[rando-hints] checked %zu enabled hints, %zu failures\n", hintsChecked, failures);
    return failures == 0 ? 0 : 1;
}

// Implemented in SaveManager.cpp (which owns the private SaveRandomizer /
// LoadRandomizer and the section json cursor). Drives the REAL randomizer
// section through save -> Rando::Context reset -> reload, in memory, exactly as
// loading a saved file does.
extern "C" void Rando_TestRoundTripRandomizerSection(int* outResetCleared, int* outPlacementRestored);

// Hint-RELOAD lock (#441). PR #445 proved generation-time text is correct and
// added a hint-validity lock, but that lock (and its toJSON()/Hint(json) round
// trip) re-renders every hint against the SAME live, fully-filled context, so it
// cannot see the operator-visible break: an ITEM hint resolves its item at
// RUNTIME from ctx->GetItemLocation(loc)->GetPlacedItem(), and that placement
// table is thrown away and rebuilt from the save on every load (a fresh new
// game and a reload BOTH go FileChoose_LoadGame -> Save_LoadFile, which resets
// the Rando::Context and rehydrates it via LoadRandomizer). A gossip stone read
// "catching Big Poes leads to No Item" while the spoiler named a real item.
// Gossip item hints carry NO hintKeys, so the location phrasing comes straight
// from StaticData::GetLocation(loc)->GetHint() -- it survived, which proves the
// hint's location was intact and only the location->placement resolution had
// decayed to RG_NONE ("No Item" is itemTable[RG_NONE]).
//
// This drives the actual save -> context-reset -> reload cycle and asserts that
// every enabled item hint still renders the SAME text it did live and never the
// no-item sentinel. It is the reload-path complement to #445's generation-time
// lock.
extern "C" int Rando_HeadlessHintReloadTest(const char* seedStr) {
    int rc = Rando_HeadlessSeedTest(seedStr);
    if (rc != 0) {
        fprintf(stderr, "[hint-reload] generation failed rc=%d\n", rc);
        return rc;
    }

    auto ctx = Rando::Context::GetInstance();
    if (!ctx) {
        fprintf(stderr, "[hint-reload] no Rando::Context after generation\n");
        return 4;
    }

    const std::string sentinel = Rando::StaticData::RetrieveItem(RG_NONE).GetName().GetEnglish();
    if (sentinel.empty()) {
        fprintf(stderr, "[hint-reload] itemTable[RG_NONE] has no name; item table not initialised\n");
        return 5;
    }

    // Snapshot every enabled item hint's live rendered text BEFORE the reload.
    struct HintSnapshot {
        RandomizerHint key;
        std::string name;
        std::vector<std::string> messages;
    };
    std::vector<HintSnapshot> snapshots;
    for (int h = RH_NONE + 1; h < RH_MAX; h++) {
        const auto hintKey = static_cast<RandomizerHint>(h);
        Rando::Hint* hint = ctx->GetHint(hintKey);
        if (hint == nullptr || !hint->IsEnabled()) {
            continue;
        }
        const HintType hintType = hint->GetHintType();
        if (hintType != HINT_TYPE_ITEM && hintType != HINT_TYPE_ITEM_AREA) {
            continue; // only item hints name an item that can decay to the sentinel
        }
        HintSnapshot snap;
        snap.key = hintKey;
        snap.name = Rando::StaticData::hintNames[hintKey].GetForCurrentLanguage(MF_CLEAN);
        snap.messages = hint->GetAllMessageStrings(MF_CLEAN);
        snapshots.push_back(std::move(snap));
    }
    fprintf(stderr, "[hint-reload] snapshotted %zu enabled item hints\n", snapshots.size());

    // Drop this local strong reference before the reset. Context::mContext is a
    // weak_ptr, so the reset inside the round trip only builds a fresh context if
    // every shared_ptr owner is released first; holding `ctx` here would pin the
    // old, still-populated context and make the reload a no-op (a false pass).
    ctx = nullptr;

    // Do the real save -> reset -> reload. This rebuilds the Rando::Context.
    int resetCleared = 0;
    int placementRestored = 0;
    Rando_TestRoundTripRandomizerSection(&resetCleared, &placementRestored);
    fprintf(stderr, "[hint-reload] after reload: resetCleared=%d placementRestored=%d\n", resetCleared,
            placementRestored);
    if (resetCleared == 0) {
        // The context reset did not empty the placement table, so the reload was
        // never actually exercised; a pass here would be meaningless.
        fprintf(stderr, "[hint-reload] the context reset did not clear placements; test cannot exercise the reload\n");
        return 7;
    }

    // Re-render each hint against the REBUILT context and compare.
    auto reloaded = Rando::Context::GetInstance();
    if (!reloaded) {
        fprintf(stderr, "[hint-reload] no Rando::Context after reload\n");
        return 4;
    }
    size_t failures = 0;
    for (const HintSnapshot& snap : snapshots) {
        Rando::Hint* hint = reloaded->GetHint(snap.key);
        const std::vector<std::string> after =
            hint != nullptr ? hint->GetAllMessageStrings(MF_CLEAN) : std::vector<std::string>{};

        for (const std::string& message : after) {
            if (message.find(sentinel) != std::string::npos) {
                fprintf(stderr, "[hint-reload] FAIL %s: after reload the hint resolves to the no-item sentinel: \"%s\"\n",
                        snap.name.c_str(), message.c_str());
                failures++;
            }
        }

        if (after.size() != snap.messages.size()) {
            fprintf(stderr, "[hint-reload] FAIL %s: reload changed message count %zu -> %zu\n", snap.name.c_str(),
                    snap.messages.size(), after.size());
            failures++;
            continue;
        }
        for (size_t m = 0; m < after.size(); m++) {
            if (after[m] == snap.messages[m]) {
                continue;
            }
            const bool decayed = after[m].find(sentinel) != std::string::npos;
            fprintf(stderr,
                    "[hint-reload] FAIL %s: reload changed message%s\n  live:     \"%s\"\n  reloaded: \"%s\"\n",
                    snap.name.c_str(), decayed ? " and it decayed to the no-item sentinel" : "",
                    snap.messages[m].c_str(), after[m].c_str());
            failures++;
        }
    }

    if (snapshots.empty()) {
        fprintf(stderr, "[hint-reload] no enabled item hints were generated; the lock would vacuously pass\n");
        return 6;
    }

    fprintf(stderr, "[hint-reload] checked %zu item hints across a reload, %zu failures\n", snapshots.size(), failures);
    return failures == 0 ? 0 : 1;
}

// Implemented in SaveManager.cpp / src/common/entrance.cpp. Save_Init() is the
// SRAM boot init the gamestate framework dispatches from Title_Destroy on every
// pass through the title chain -- including the cold-boot chain a switch INTO
// OoT walks. The Combo_* startup-entrance calls stand in for "a cross-game OoT
// arrival is in flight".
extern "C" void Save_Init(void);
extern "C" void Combo_SetStartupEntrance(uint16_t entrance);
extern "C" void Combo_ClearStartupEntrance(void);
extern "C" bool Combo_HasStartupEntranceForGame(const char* gameId);

// Cross-game arrival lock (#441). THE actual operator-visible defect: gossip
// stones read "catching Big Poes leads to No Item" after switching OoT->MM->OoT.
// The reload lock above proves a file LOAD rehydrates correctly; this proves the
// OTHER entry into OoT does not. A switch into OoT cold-boots the gamestate chain
// through the title screen, and Title_Destroy runs OoT_Sram_InitSram -> Save_Init
// -> SaveManager::Init, whose tail wiped the Rando::Context placement table
// (ClearItemLocations). The frozen OoT save is restored afterwards at
// Play_ConsumeStartupEntrance WITHOUT going through Save_LoadFile, so nothing
// re-runs LoadRandomizer to rehydrate it -- the #447 class. ClearItemLocations
// only empties itemLocationTable, never hintTable, which is exactly why the hint
// kept its location phrasing ("catching Big Poes") while the item resolved to
// itemTable[RG_NONE] ("No Item").
//
// The fix guards that clear with Combo_HasStartupEntranceForGame("oot"). This
// test drives the real Save_Init both WITH the arrival flag set (placements must
// survive; item hints must still name their real item) and WITHOUT it (the clear
// must still fire, proving the guard is the only thing that saved the world and
// the assertion above is not vacuous).
extern "C" int Rando_HeadlessHintCrossGameTest(const char* seedStr) {
    int rc = Rando_HeadlessSeedTest(seedStr);
    if (rc != 0) {
        fprintf(stderr, "[hint-crossgame] generation failed rc=%d\n", rc);
        return rc;
    }
    auto ctx = Rando::Context::GetInstance();
    if (!ctx) {
        fprintf(stderr, "[hint-crossgame] no Rando::Context after generation\n");
        return 4;
    }

    const std::string sentinel = Rando::StaticData::RetrieveItem(RG_NONE).GetName().GetEnglish();
    if (sentinel.empty()) {
        fprintf(stderr, "[hint-crossgame] itemTable[RG_NONE] has no name; item table not initialised\n");
        return 5;
    }

    // A sample location that holds a real item, to prove the placement table is
    // (a) preserved on arrival and (b) genuinely cleared without the guard.
    bool haveSample = false;
    size_t sample = 0;
    for (int i = 1; i < RC_MAX; i++) {
        if (ctx->GetItemLocation(i)->GetPlacedRandomizerGet() != RG_NONE) {
            haveSample = true;
            sample = (size_t)i;
            break;
        }
    }
    if (!haveSample) {
        fprintf(stderr, "[hint-crossgame] no placed items after generation; cannot exercise the lock\n");
        return 8;
    }

    // Snapshot every enabled item hint's live text before the arrival.
    struct HintSnapshot {
        RandomizerHint key;
        std::string name;
        std::vector<std::string> messages;
    };
    std::vector<HintSnapshot> snapshots;
    for (int h = RH_NONE + 1; h < RH_MAX; h++) {
        const auto hintKey = static_cast<RandomizerHint>(h);
        Rando::Hint* hint = ctx->GetHint(hintKey);
        if (hint == nullptr || !hint->IsEnabled()) {
            continue;
        }
        const HintType hintType = hint->GetHintType();
        if (hintType != HINT_TYPE_ITEM && hintType != HINT_TYPE_ITEM_AREA) {
            continue;
        }
        HintSnapshot snap;
        snap.key = hintKey;
        snap.name = Rando::StaticData::hintNames[hintKey].GetForCurrentLanguage(MF_CLEAN);
        snap.messages = hint->GetAllMessageStrings(MF_CLEAN);
        snapshots.push_back(std::move(snap));
    }
    if (snapshots.empty()) {
        fprintf(stderr, "[hint-crossgame] no enabled item hints were generated; the lock would vacuously pass\n");
        return 6;
    }
    fprintf(stderr, "[hint-crossgame] snapshotted %zu enabled item hints; sample check %zu\n", snapshots.size(), sample);

    // ---- Arrival case: Save_Init must NOT wipe placements. ----
    Combo_SetStartupEntrance(0x0100); // any pending entrance -> "a switch into OoT is in flight"
    if (!Combo_HasStartupEntranceForGame("oot")) {
        fprintf(stderr, "[hint-crossgame] could not arm the OoT arrival flag; test setup is broken\n");
        Combo_ClearStartupEntrance();
        return 9;
    }
    Save_Init();
    const bool survivedArrival =
        Rando::Context::GetInstance()->GetItemLocation(sample)->GetPlacedRandomizerGet() != RG_NONE;

    size_t failures = 0;
    auto afterArrival = Rando::Context::GetInstance();
    for (const HintSnapshot& snap : snapshots) {
        Rando::Hint* hint = afterArrival->GetHint(snap.key);
        const std::vector<std::string> after =
            hint != nullptr ? hint->GetAllMessageStrings(MF_CLEAN) : std::vector<std::string>{};
        for (const std::string& message : after) {
            if (message.find(sentinel) != std::string::npos) {
                fprintf(stderr,
                        "[hint-crossgame] FAIL %s: after a cross-game OoT arrival the hint resolves to the no-item "
                        "sentinel: \"%s\"\n",
                        snap.name.c_str(), message.c_str());
                failures++;
            }
        }
        if (after != snap.messages) {
            fprintf(stderr, "[hint-crossgame] FAIL %s: the arrival changed the hint text\n", snap.name.c_str());
            failures++;
        }
    }
    if (!survivedArrival) {
        fprintf(stderr, "[hint-crossgame] FAIL: sample check %zu was wiped to RG_NONE by the arrival Save_Init\n",
                sample);
        failures++;
    }

    // ---- Control: without the arrival flag, the clear MUST still fire, so the
    // pass above cannot be a no-op that never reached ClearItemLocations. ----
    Combo_ClearStartupEntrance();
    Save_Init();
    const bool clearedWhenNotArriving =
        Rando::Context::GetInstance()->GetItemLocation(sample)->GetPlacedRandomizerGet() == RG_NONE;
    if (!clearedWhenNotArriving) {
        fprintf(stderr, "[hint-crossgame] FAIL: Save_Init did NOT clear placements off the arrival path -- the lock is "
                        "vacuous (ClearItemLocations was never reached)\n");
        failures++;
    }

    fprintf(stderr, "[hint-crossgame] checked %zu item hints across a cross-game arrival, %zu failures\n",
            snapshots.size(), failures);
    return failures == 0 ? 0 : 1;
}

// Determinism harness bridge (Lane B, Phase 3.0): run ONE seed generation and
// emit a canonical, side-effect-free digest of (a) the unified-seed producer's
// output in gComboCtx and (b) the full item placement, so an external wrapper
// can prove two same-seed runs produce byte-identical worlds. Returns 0 only if
// generation succeeded AND the live producer stamped gComboCtx; non-zero (with a
// stderr marker) otherwise. `outPath` NULL/empty => write the digest to stdout.
//
// Placements are captured IN-MEMORY here rather than diffed from the spoiler
// JSON on disk: a same-seed rerun overwrites Randomizer/<hash>.json, so a naive
// file diff would compare a file against itself (a verified pitfall). Iterating
// RandomizerCheck in fixed enum order [0, RC_MAX) also makes the digest
// independent of any hash-map iteration order and of the timestamp/version
// fields the spoiler JSON carries.
//
// Determinism scope: the harness drives the synchronous 3-arg GenerateRandomizer
// with EMPTY excludedLocations/enabledTricks (see Rando_HeadlessSeedTest), so
// this locks reproducibility for the pinned settings profile with no per-check
// exclusions or extra tricks. Exclusion/trick-driven fills are out of scope for
// this lock.
extern "C" int Rando_HeadlessSeedDeterminismDigest(const char* seedStr, const char* outPath) {
    int rc = Rando_HeadlessSeedTest(seedStr);
    if (rc != 0) {
        fprintf(stderr, "[rando-determinism] generation failed rc=%d\n", rc);
        return rc;
    }

    // The live unified-seed producer (Playthrough_Init) must have stamped the
    // ComboContext during generation. This ties the determinism lock directly to
    // Lane B's producer: if it ever stops firing, sourceIsRando/sharedRandoSeed
    // read 0 and this fails, instead of silently passing on an unwritten world.
    if (!gComboCtx.sourceIsRando) {
        fprintf(stderr, "[rando-determinism] gComboCtx.sourceIsRando not set by generation\n");
        return 2;
    }
    if (gComboCtx.sharedRandoSeed == 0) {
        fprintf(stderr, "[rando-determinism] gComboCtx.sharedRandoSeed still 0 after generation\n");
        return 3;
    }

    auto ctx = Rando::Context::GetInstance();
    if (!ctx) {
        fprintf(stderr, "[rando-determinism] no Rando::Context after generation\n");
        return 4;
    }

    // Canonical placement blob: "<rc>:<placedItem>;" for every location, in
    // ascending RandomizerCheck order, folded through the project's FNV-1a so the
    // digest file stays small and stable.
    std::string blob;
    blob.reserve(static_cast<size_t>(RC_MAX) * 8);
    size_t placedCount = 0;
    for (int i = 0; i < RC_MAX; i++) {
        RandomizerGet item = ctx->GetItemLocation(static_cast<RandomizerCheck>(i))->GetPlacedRandomizerGet();
        if (item != RG_NONE) {
            placedCount++;
        }
        blob += std::to_string(i);
        blob += ':';
        blob += std::to_string(static_cast<int>(item));
        blob += ';';
    }
    const uint32_t placementHash = SohUtils::Hash(blob);

    // #510: fold the REVERSE foreign placements (MM items hosted in OoT checks,
    // written by OoT_PlaceForeignItems during the generation above) into the same
    // digest. Without this the two-process diff would happily agree on a world
    // whose cross-game half was drawn differently each run — the placement pass
    // seeds a local xorshift32 from the paired identity precisely so it cannot,
    // and this is what holds it to that. Canonical, fixed-slot order, and stable
    // when the carve is empty (an unpaired world digests as all-zero slots rather
    // than as absent, so a pairing that stops firing shows up as a mismatch).
    std::string foreignBlob;
    size_t foreignCount = 0;
    for (int i = 0; i < (int)RSBS_FOREIGN_PLACEMENT_CAP; i++) {
        const ComboForeignPlacement& fp = gComboCtx.foreignPlacementsOoT[i];
        if (fp.item.originGame != (uint8_t)GAME_NONE) {
            foreignCount++;
        }
        foreignBlob += std::to_string(fp.mmCheckId);
        foreignBlob += ':';
        foreignBlob += std::to_string((int)fp.item.originGame);
        foreignBlob += ':';
        foreignBlob += std::to_string(fp.item.id);
        foreignBlob += ';';
    }
    const uint32_t foreignHash = SohUtils::Hash(foreignBlob);

    FILE* out = stdout;
    bool closeOut = false;
    if (outPath && outPath[0] != '\0') {
        out = fopen(outPath, "w");
        if (out == nullptr) {
            fprintf(stderr, "[rando-determinism] cannot open digest output '%s'\n", outPath);
            return 5;
        }
        closeOut = true;
    }
    fprintf(out,
            "seed=%08X\n"
            "settingsHash=%08X\n"
            "sourceIsRando=%d\n"
            "placementHash=%08X\n"
            "placedCount=%zu\n"
            "foreignOoTHash=%08X\n"
            "foreignOoTCount=%zu\n",
            gComboCtx.sharedRandoSeed, gComboCtx.sharedRandoSettingsHash, gComboCtx.sourceIsRando ? 1 : 0,
            placementHash, placedCount, foreignHash, foreignCount);
    if (closeOut) {
        fclose(out);
    }
    fprintf(stderr,
            "[rando-determinism] digest: seed=%08X settingsHash=%08X placementHash=%08X placed=%zu "
            "foreignOoTHash=%08X foreignOoT=%zu\n",
            gComboCtx.sharedRandoSeed, gComboCtx.sharedRandoSettingsHash, placementHash, placedCount, foreignHash,
            foreignCount);
    return 0;
}

bool GenerateRandomizer(std::set<RandomizerCheck> excludedLocations, std::set<RandomizerTrick> enabledTricks,
                        std::string seedInput) {
    const auto ctx = Rando::Context::GetInstance();
    ResetPerformanceTimers();
    StartPerformanceTimer(PT_WHOLE_SEED);

    // if a blank seed was entered, make a random one
    if (seedInput.empty()) {
        char seedString[11];
        for (size_t i = 0; i < 10; i++) {
            seedString[i] = '0' + ShipUtils::Random(0, 10);
        }
        seedString[10] = '\0';
        seedInput = std::string(seedString);
    } else if (seedInput.rfind("seed_testing_count", 0) == 0 && seedInput.length() > 18) {
        int count;
        try {
            count = std::stoi(seedInput.substr(18), nullptr);
        } catch (std::invalid_argument&) { count = 1; } catch (std::out_of_range&) {
            count = 1;
        }
        Playthrough::Playthrough_Repeat(excludedLocations, enabledTricks, count);
        return false; // TODO: Not sure if this is correct but I don't think we support this functionality yet anyway.
    }

    ctx->SetSeedString(seedInput);
    uint32_t seedHash = SohUtils::Hash(ctx->GetSeedString());
    ctx->SetSeed(seedHash);

    ctx->ClearItemLocations();
    int ret = Playthrough::Playthrough_Init(ctx->GetSeed(), excludedLocations, enabledTricks);
    if (ret < 0) {
        if (ret == -1) { // Failed to generate after 5 tries
            SPDLOG_ERROR("Failed to generate after 5 tries.");
            return false;
        } else {
            SPDLOG_ERROR("Error {} with fill.", ret);
            return false;
        }
    }

    StopPerformanceTimer(PT_WHOLE_SEED);
    SPDLOG_DEBUG("Full Seed Genration Time: {}ms", GetPerformanceTimer(PT_WHOLE_SEED).count());
    SPDLOG_DEBUG("LogicReset time: {}ms", GetPerformanceTimer(PT_LOGIC_RESET).count());
    SPDLOG_DEBUG("Area->Reset time: {}ms", GetPerformanceTimer(PT_REGION_RESET).count());
    SPDLOG_DEBUG("Total Entrance Shuffle time: {}ms", GetPerformanceTimer(PT_ENTRANCE_SHUFFLE).count());
    SPDLOG_DEBUG("Total Shopsanity time: {}ms", GetPerformanceTimer(PT_SHOPSANITY).count());
    SPDLOG_DEBUG("Total Dungeon Specific Items time: {}ms", GetPerformanceTimer(PT_OWN_DUNGEON).count());
    SPDLOG_DEBUG("Total Misc Limited Checks time: {}ms", GetPerformanceTimer(PT_LIMITED_CHECKS).count());
    SPDLOG_DEBUG("Total Advancment Checks time: {}ms", GetPerformanceTimer(PT_ADVANCEMENT_ITEMS).count());
    SPDLOG_DEBUG("Total Other Checks time: {}ms", GetPerformanceTimer(PT_REMAINING_ITEMS).count());
    SPDLOG_DEBUG("Total Playthrough Generation time: {}ms", GetPerformanceTimer(PT_PLAYTHROUGH_GENERATION).count());
    SPDLOG_DEBUG("Total PareDownPlaythrough time: {}ms", GetPerformanceTimer(PT_PARE_DOWN_PLAYTHROUGH).count());
    SPDLOG_DEBUG("Total WotH generation time: {}ms", GetPerformanceTimer(PT_WOTH).count());
    SPDLOG_DEBUG("Total Foolish generation time: {}ms", GetPerformanceTimer(PT_FOOLISH).count());
    SPDLOG_DEBUG("Total Overrides time: {}ms", GetPerformanceTimer(PT_OVERRIDES).count());
    SPDLOG_DEBUG("Total Hint Generation time: {}ms", GetPerformanceTimer(PT_HINTS).count());
    SPDLOG_DEBUG("SpoilerLog writing time: {}ms", GetPerformanceTimer(PT_SPOILER_LOG).count());
    SPDLOG_DEBUG("Total Event Access Calculation time: {}ms", GetPerformanceTimer(PT_EVENT_ACCESS).count());
    SPDLOG_DEBUG("Total ToD Access Calculation: {}ms", GetPerformanceTimer(PT_TOD_ACCESS).count());
    SPDLOG_DEBUG("Total Entrance Logic Calculation time: {}ms", GetPerformanceTimer(PT_ENTRANCE_LOGIC).count());
    SPDLOG_DEBUG("Total Check Logic Calculation time: {}ms", GetPerformanceTimer(PT_LOCATION_LOGIC).count());
    return true;
}