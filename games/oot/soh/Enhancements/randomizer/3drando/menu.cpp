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
#include "../entrance.h"          // #949: the entrance table the content digest folds
#include "starting_inventory.hpp" // #949: the starting inventory the content digest folds
#include "soh/cvar_prefixes.h"     // #982: the menu CVars a golden profile writes
#include <cstdio>
#include <fstream>
#include "context.h" // src/common — gComboCtx, Lane B unified-seed carrier (ADR 0002)
#include "crossing_store.h" // src/common — the paired world's hint checks (PR #743 review)
#ifdef RSBS_SINGLE_EXECUTABLE
#include "combo_area.h"    // src/common — the MM area a Termina slot names (#575 H3a)
#include "combo_logic.h"   // src/common — the coordinator's placements a foolish area may not hold (#575 H5a)
#include "foreign_items.h" // src/common — the crossed item's name and article (#575 H3b)
#include "shared_items.h"  // src/common — MM's grade of a crossed item (#575 H3b)
#include <set>
#endif
#include "notification_bridge.h"   // src/common — the refusal toast the reload bridge reads back
#include "pairing_refusal_toast.h" // src/common — that toast's copy

namespace {
bool seedChanged;
uint16_t pastSeedLength;
std::vector<std::string> presetEntries;
Rando::Option* currentSetting;
} // namespace

// randomizer.cpp (#982): the Generate button's read of the exclude and trick CVars.
void Rando_ReadMenuExcludesAndTricks(std::set<RandomizerCheck>& excludedLocations,
                                     std::set<RandomizerTrick>& enabledTricks);

namespace {
// #982: what a golden profile asked for, so the digest can prove it took. -1 when
// the environment named no set (every golden before #982, which passes {} {}).
int sGoldenExcludesRequested = -1;
int sGoldenTricksRequested = -1;
std::set<RandomizerCheck> sGoldenExcludes;
std::set<RandomizerTrick> sGoldenTricks;

/** How many distinct comma-separated keys @p list names. */
int CountGoldenKeys(const char* list) {
    std::set<std::string> keys;
    std::stringstream ss(list);
    std::string key;
    while (std::getline(ss, key, ',')) {
        if (!key.empty()) {
            keys.insert(key);
        }
    }
    return (int)keys.size();
}

/**
 * #982: a golden profile's OoT exclude and trick sets, and MM's excluded checks,
 * from the environment. RSBS_GOLDEN_EXCLUDES and RSBS_GOLDEN_TRICKS carry
 * comma-separated RandomizerCheck and RandomizerTrick ids, the form the menu stores
 * in gRandoSettings.ExcludedLocations and gRandoSettings.EnabledTricks. They are
 * written to those CVars and read back with the Generate button's own parse
 * (Rando_ReadMenuExcludesAndTricks), visibility filter included, so a golden pins
 * the sets a player's generation builds from the same keys. A key the filter drops
 * refuses the run: a profile must never shrink silently. RSBS_GOLDEN_MM_EXCLUDES is
 * written to gRando.ExcludedChecks as given, which MM's pool generation parses and
 * MM's profile digest folds (write it sorted, as MM's menu does). With none of the
 * three set, both sets stay empty, as they were before #982.
 * @return false when a requested key did not take or a key is not a number.
 */
bool ReadGoldenProfileSets(std::set<RandomizerCheck>& excludedLocations, std::set<RandomizerTrick>& enabledTricks) {
    sGoldenExcludesRequested = -1;
    sGoldenTricksRequested = -1;
    sGoldenExcludes.clear();
    sGoldenTricks.clear();
    if (const char* mmExcludes = std::getenv("RSBS_GOLDEN_MM_EXCLUDES")) {
        CVarSetString("gRando.ExcludedChecks", mmExcludes);
        fprintf(stderr, "[golden-profile] MM excluded checks: %s\n", mmExcludes);
    }
    const char* excludes = std::getenv("RSBS_GOLDEN_EXCLUDES");
    const char* tricks = std::getenv("RSBS_GOLDEN_TRICKS");
    if (excludes == nullptr && tricks == nullptr) {
        return true;
    }
    CVarSetString(CVAR_RANDOMIZER_SETTING("ExcludedLocations"), excludes != nullptr ? excludes : "");
    CVarSetString(CVAR_RANDOMIZER_SETTING("EnabledTricks"), tricks != nullptr ? tricks : "");
    try {
        Rando_ReadMenuExcludesAndTricks(excludedLocations, enabledTricks);
    } catch (const std::exception& e) {
        fprintf(stderr, "[golden-profile] FAIL: the menu's parse refused the profile's keys (%s)\n", e.what());
        return false;
    }
    sGoldenExcludesRequested = excludes != nullptr ? CountGoldenKeys(excludes) : 0;
    sGoldenTricksRequested = tricks != nullptr ? CountGoldenKeys(tricks) : 0;
    for (const RandomizerCheck rc : excludedLocations) {
        fprintf(stderr, "[golden-profile] exclude %d = %s\n", (int)rc,
                Rando::StaticData::GetLocation(rc)->GetName().c_str());
    }
    for (const RandomizerTrick rt : enabledTricks) {
        fprintf(stderr, "[golden-profile] trick %d = %s\n", (int)rt,
                Rando::Settings::GetInstance()->GetTrickOption(rt).GetName().c_str());
    }
    if ((int)excludedLocations.size() != sGoldenExcludesRequested ||
        (int)enabledTricks.size() != sGoldenTricksRequested) {
        fprintf(stderr,
                "[golden-profile] FAIL: the profile names %d exclude(s) and %d trick(s), the menu's parse kept %zu and "
                "%zu (a key that is not visible under this profile is dropped)\n",
                sGoldenExcludesRequested, sGoldenTricksRequested, excludedLocations.size(), enabledTricks.size());
        return false;
    }
    sGoldenExcludes = excludedLocations;
    sGoldenTricks = enabledTricks;
    return true;
}
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
    std::set<RandomizerCheck> excludedLocations;
    std::set<RandomizerTrick> enabledTricks;
    if (!ReadGoldenProfileSets(excludedLocations, enabledTricks)) {
        return 1;
    }
    bool ok = GenerateRandomizer(excludedLocations, enabledTricks, seedStr ? seedStr : "");
    fprintf(stderr, "[rando-diag] GenerateRandomizer returned %s\n", ok ? "true" : "false");
    return ok ? 0 : 1;
}

#ifdef RSBS_SINGLE_EXECUTABLE
extern "C" int OoT_ComboLogic_TestSetNativeGeneralPass(int native);
extern "C" int OoT_ComboLogic_GeneralPassDeferred(void);
#else
static int OoT_ComboLogic_TestSetNativeGeneralPass(int) {
    return 0;
}
#endif

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
#ifdef RSBS_SINGLE_EXECUTABLE
// ForeignItemsSingleExe.cpp / hints.cpp: the crossing store's view, for the paired
// world's hint checks below (PR #743 review).
extern "C" int OoT_Combo_CheckHostsCrossing(int rc);
extern "C" int Rando_HintAreaForItemOutsideHyrule(int item);
extern "C" int Rando_HintAreaFromCrossingStore(int item);
// hints.cpp / ComboLogicEngineOoT.cpp (#575 H3a): the MM area a Termina slot names.
extern "C" int Rando_HintForeignAreaFromCrossingStore(int item);
extern "C" int Rando_HintForeignAreaForItem(int item);
extern "C" int Rando_TestCreateStaticItemHint(int hintKey, int hintTextKey, int item);
extern "C" int Rando_TestCreateAltarHint(int hintKey, const int* rewards, int count);
extern "C" int Rando_TestCreateStaticHintFromData(int hintKey, const int* items, int count);
extern "C" void OoT_ComboLogic_TestSetHintingPairedRemainder(int on);
// ComboLogicEngineOoT.cpp (#575 H3b): the paired spoiler's gate, held open by a lock
// as OoT_ComboLogic_FinishGeneralPass holds it while it writes that spoiler; and the
// two creation gates' state, which the loaded-file check requires shut.
extern "C" void OoT_ComboLogic_TestSetWritingSingleBagSpoiler(int on);
extern "C" int OoT_ComboLogic_HintingPairedRemainder(void);
extern "C" int OoT_ComboLogic_WritingSingleBagSpoiler(void);
// hints.cpp: the "Named Item" distribution's filter (#575 H3b: a crossing is major
// when MM grades the MM item it yields major).
bool FilterGoodItems(RandomizerCheck loc);
#else
static int OoT_Combo_CheckHostsCrossing(int) {
    return 0;
}
#endif

/**
 * The three #441 passes (and the round trip) over every enabled hint of the LIVE
 * context. With `pairedWorld`, the world is a single-bag paired world completed by
 * the creation event (PR #743 review), and four more things must hold:
 *   (P1) #575 H3b: a hint that names a crossing host names the MM item the host
 *        yields, with its article and " (MM)", never the cover item the host
 *        physically holds, and so does the spoiler's "item" (the hint's text is
 *        read with the remainder's gate open, as the paired creation renders it);
 *   (P2) #575 H3b: every crossing host is hintable, and the hint pass's "Named
 *        Item" filter counts it as a major item. Because the pinned seed may hint
 *        no host, P1 and P2 are also probed on every host (ProbeCrossingHostHints):
 *        an item hint and an item-area hint built for it, their text, their
 *        spoiler "item" and their JSON round trip;
 *   (P3) an item hint whose target is at no OoT location names Termina, never
 *        "an Isolated Place" (the RA_NONE a location with no area answers);
 *   (P4) every OoT item the crossing store hosts in MM resolves to Termina, so P3
 *        is not vacuous on a seed whose hinted items all stayed home.
 *   (P5) OUTSIDE the remainder's hint pass (now: this validator runs after it) the
 *        hint pass does not read the store at all: every one of those items answers
 *        RA_NONE, so a later hint pass over another world cannot name Termina off
 *        this world's crossings. #575 H3b: nor does a host read the store outside
 *        the remainder (no file is loaded here): its probe names the cover, and the
 *        "Named Item" filter answers the cover's own majority.
 *   (P6) #575 H3a: every Termina slot names the exact MM area. A slot whose area is
 *        RA_TERMINA carries, in the hint's parallel foreignAreas, the shared
 *        vocabulary's area of the MM check that hosts THAT SLOT'S OWN item (the
 *        first store row for that item, as P4 pins), and no other slot carries
 *        one. The item behind each slot is recovered from the builder's own list
 *        (TerminaSlotItemsOf). Because a pinned seed's static hints may target no
 *        crossed item, P6 also drives all three production builders that push
 *        RA_TERMINA under the remainder's gate: the static item hint builder
 *        (one hint per OoT item hosted only in MM: its slot, its rendered text,
 *        its spoiler-JSON round trip and an old save's reading, with no
 *        foreignAreas still "Termina"), and the altar and static-from-data
 *        builders over a three-slot list {crossed A, an item in Hyrule, crossed B}
 *        (slot alignment, each slot's own area, the rendered names).
 *   (P7) #575 H5a: OoT's native Foolish entry works in a paired world, and only
 *        ever names a foolish area. Recomputed here in OoT's own RandomizerArea
 *        terms, from facts the production pass does not share a line with: an
 *        area is foolish when one of its locations is hintable and none of them
 *        holds an IMPORTANT item -- a major item by OoT's own predicate (what the
 *        native CalculateBarren reads), any placement the single bag's coordinator
 *        made on an OoT host (its tables still hold the whole fill here), or a
 *        crossing (the store's OoT-hosted rows). A location is a foolish
 *        candidate exactly when every one of its areas is foolish (the native
 *        FindIfBarren). Every location's barren flag must match that, at least one
 *        location must carry it (a paired world had none: the seam skips
 *        CalculateBarren, so the Foolish entry drew nothing), and every Foolish
 *        hint the world drew must name a foolish area. A pinned seed may draw no
 *        Foolish hint; that half is then reported as vacuous, the flag half is not.
 */
#ifdef RSBS_SINGLE_EXECUTABLE
/** (P4) The MM check of the FIRST store row hosting OoT item `item`, or 0. */
static uint16_t P4FirstHostOf(uint16_t item) {
    for (int i = 0; i < Combo_Crossings_Count(GAME_MM); i++) {
        ComboCrossing row;
        if (Combo_Crossings_At(GAME_MM, i, &row) && row.item.originGame == (uint8_t)GAME_OOT && row.item.id == item) {
            return row.hostCheck;
        }
    }
    return 0;
}

/** (P6) The MM area a Termina slot for OoT item `item` must name: the area of the
 *  item's first MM host, or 0 when the store hosts it nowhere in MM. */
static uint8_t ExpectedForeignArea(RandomizerGet item) {
    const uint16_t host = P4FirstHostOf((uint16_t)item);
    return host == 0 ? (uint8_t)COMBO_AREA_NONE : Combo_Area_OfCheck((uint8_t)GAME_MM, host).area;
}

static std::string ForeignAreaLabel(uint8_t area) {
    const char* name = area == COMBO_AREA_NONE ? nullptr : Combo_Area_Name(ComboArea{ (uint8_t)GAME_MM, area });
    return name != nullptr ? name : "?";
}

/**
 * (P6) The item behind each area slot of generated hint `key`, laid out exactly as
 * the hints.cpp builder that pushes RA_TERMINA lays its slots out: the item for a
 * slot that answers Termina, RG_NONE for any other slot. Only three builders push
 * RA_TERMINA: CreateAltarHint (the reward lists below mirror CreateChildAltarHint
 * and CreateAdultAltarHint; a reward found nowhere gets no slot), CreateStaticItemHint
 * for the Ganondorf hint (Light Arrows, plus the Master Sword when it hints two
 * locations) and CreateStaticHintFromData (staticHintInfoMap's targetItems). Any
 * other hint answers an empty list, so a Termina slot in it fails.
 */
static std::vector<RandomizerGet> TerminaSlotItemsOf(RandomizerHint key, const Rando::Hint& hint) {
    const std::vector<RandomizerCheck> locations = hint.GetHintedLocations();
    std::vector<RandomizerGet> builderItems = {};
    bool altar = false;
    if (key == RH_ALTAR_CHILD) {
        builderItems = { RG_KOKIRI_EMERALD, RG_GORON_RUBY, RG_ZORA_SAPPHIRE };
        altar = true;
    } else if (key == RH_ALTAR_ADULT) {
        builderItems = { RG_LIGHT_MEDALLION,  RG_FOREST_MEDALLION, RG_FIRE_MEDALLION,
                         RG_WATER_MEDALLION,  RG_SPIRIT_MEDALLION, RG_SHADOW_MEDALLION };
        altar = true;
    } else if (key == RH_GANONDORF_HINT) {
        builderItems = locations.size() == 2 ? std::vector<RandomizerGet>{ RG_LIGHT_ARROWS, RG_MASTER_SWORD }
                                             : std::vector<RandomizerGet>{ RG_LIGHT_ARROWS };
    } else if (Rando::StaticData::staticHintInfoMap.contains(key)) {
        builderItems = Rando::StaticData::staticHintInfoMap[key].targetItems;
    }
    if (builderItems.empty() || locations.size() != builderItems.size()) {
        return {};
    }
    std::vector<RandomizerGet> slots = {};
    for (size_t i = 0; i < locations.size(); i++) {
        if (locations[i] != RC_UNKNOWN_CHECK) {
            slots.push_back(RG_NONE);
        } else if (Rando_HintAreaFromCrossingStore((int)builderItems[i]) == RA_TERMINA) {
            slots.push_back(builderItems[i]);
        } else if (!altar) {
            slots.push_back(RG_NONE);
        }
    }
    return slots;
}

/**
 * (P6) Slot by slot: a Termina slot carries the area of ITS OWN item's first MM host
 * (`slotItems[ai]`, from TerminaSlotItemsOf or a probe's own list); any other slot
 * carries none. Counts the Termina slots into `terminaSlots` when given.
 */
static size_t CheckForeignAreaSlots(const std::string& label, const std::vector<RandomizerArea>& areas,
                                    const std::vector<uint8_t>& foreign, const std::vector<RandomizerGet>& slotItems,
                                    size_t* terminaSlots) {
    size_t failures = 0;
    if (!foreign.empty() && foreign.size() != areas.size()) {
        fprintf(stderr, "[rando-hints] FAIL %s: %zu MM area slot(s) beside %zu area slot(s) (P6)\n", label.c_str(),
                foreign.size(), areas.size());
        failures++;
    }
    for (size_t ai = 0; ai < areas.size(); ai++) {
        const uint8_t named = ai < foreign.size() ? foreign[ai] : (uint8_t)COMBO_AREA_NONE;
        if (areas[ai] != RA_TERMINA) {
            if (named != COMBO_AREA_NONE) {
                fprintf(stderr, "[rando-hints] FAIL %s: slot %zu is not Termina but names MM area %u (P6)\n",
                        label.c_str(), ai, (unsigned)named);
                failures++;
            }
            continue;
        }
        if (terminaSlots != nullptr) {
            (*terminaSlots)++;
        }
        const RandomizerGet item = ai < slotItems.size() ? slotItems[ai] : RG_NONE;
        if (item == RG_NONE) {
            fprintf(stderr,
                    "[rando-hints] FAIL %s: Termina slot %zu has no item behind it that the validator can name "
                    "(P6)\n",
                    label.c_str(), ai);
            failures++;
            continue;
        }
        const uint8_t expected = ExpectedForeignArea(item);
        if (expected == COMBO_AREA_NONE || named != expected) {
            fprintf(stderr,
                    "[rando-hints] FAIL %s: Termina slot %zu (OoT item %u) names MM area %u (\"%s\"), but the item's "
                    "first MM host is in \"%s\" (P6)\n",
                    label.c_str(), ai, (unsigned)item, (unsigned)named, ForeignAreaLabel(named).c_str(),
                    ForeignAreaLabel(expected).c_str());
            failures++;
        }
    }
    return failures;
}

static std::string JoinMessages(const std::vector<std::string>& messages) {
    std::string joined;
    for (const std::string& m : messages) {
        joined += m;
        joined += "\n";
    }
    return joined;
}

struct ForeignAreaProbeResult {
    int probes = 0;
    size_t failures = 0;
};

/**
 * (P6, the production builder) For each OoT item the store hosts in MM and no
 * OoT location holds, build the static item hint production builds for it
 * (CreateStaticItemHint, under the remainder's gate) into the Ganondorf hint's
 * slot, check it, and put the world's own Ganondorf hint back.
 */
static ForeignAreaProbeResult ProbeForeignAreaHints() {
    ForeignAreaProbeResult result;
    auto ctx = Rando::Context::GetInstance();
    const RandomizerHint probeKey = RH_GANONDORF_HINT;
    const Rando::Hint saved = *ctx->GetHint(probeKey);
    const std::string termina =
        Rando::StaticData::hintTextTable[RHT_TERMINA].GetClear().GetForCurrentLanguage(MF_CLEAN);
    std::set<uint16_t> probed;
    for (int i = 0; i < Combo_Crossings_Count(GAME_MM); i++) {
        ComboCrossing row;
        if (!Combo_Crossings_At(GAME_MM, i, &row) || row.item.originGame != (uint8_t)GAME_OOT ||
            !probed.insert(row.item.id).second) {
            continue;
        }
        const ComboArea expected = Combo_Area_OfCheck((uint8_t)GAME_MM, row.hostCheck);
        const std::string expectedName = Combo_Area_Name(expected) != nullptr ? Combo_Area_Name(expected) : "";
        OoT_ComboLogic_TestSetHintingPairedRemainder(1);
        const int built = Rando_TestCreateStaticItemHint((int)probeKey, (int)RHT_GANONDORF_HINT_LA_ONLY, row.item.id);
        OoT_ComboLogic_TestSetHintingPairedRemainder(0);
        if (built == 0) {
            continue; // a copy also sits in Hyrule: the hint names that location's area
        }
        result.probes++;
        Rando::Hint* probe = ctx->GetHint(probeKey);
        const std::vector<RandomizerArea> areas = probe->GetHintedAreas();
        const std::vector<uint8_t> foreign = probe->GetForeignAreas();
        const std::string live = JoinMessages(probe->GetAllMessageStrings(MF_CLEAN));
        if (areas.size() != 1 || areas[0] != RA_TERMINA) {
            fprintf(stderr, "[rando-hints] FAIL probe: OoT item %u is only in MM but its hint's area is not Termina "
                            "(P6)\n",
                    (unsigned)row.item.id);
            result.failures++;
            continue;
        }
        if (expected.area == COMBO_AREA_NONE || foreign.size() != 1 || foreign[0] != expected.area) {
            fprintf(stderr,
                    "[rando-hints] FAIL probe: OoT item %u on MM check %u (\"%s\"): the Termina slot names MM area "
                    "%d (P6)\n",
                    (unsigned)row.item.id, (unsigned)row.hostCheck, expectedName.c_str(),
                    foreign.size() == 1 ? (int)foreign[0] : -1);
            result.failures++;
        }
        if (expectedName.empty() || live.find(expectedName) == std::string::npos) {
            fprintf(stderr, "[rando-hints] FAIL probe: OoT item %u: the hint does not name \"%s\": \"%s\" (P6)\n",
                    (unsigned)row.item.id, expectedName.c_str(), live.c_str());
            result.failures++;
        }
        // The spoiler document's round trip (toJSON / Hint(key, json)), and an
        // old save's reading of the same document without the new field.
        nlohmann::json doc = nlohmann::json::parse(probe->toJSON().dump());
        const Rando::Hint reloaded(probeKey, doc);
        const std::string reloadedText = JoinMessages(reloaded.GetAllMessageStrings(MF_CLEAN));
        if (reloadedText != live || reloaded.GetForeignAreas() != foreign) {
            fprintf(stderr, "[rando-hints] FAIL probe: OoT item %u: the JSON round trip changed the hint\n  live:     "
                            "\"%s\"\n  reloaded: \"%s\" (P6)\n",
                    (unsigned)row.item.id, live.c_str(), reloadedText.c_str());
            result.failures++;
        }
        doc.erase("foreignAreas");
        const Rando::Hint oldSave(probeKey, doc);
        const std::string oldText = JoinMessages(oldSave.GetAllMessageStrings(MF_CLEAN));
        if (oldText.find(termina) == std::string::npos ||
            (!expectedName.empty() && oldText.find(expectedName) != std::string::npos)) {
            fprintf(stderr, "[rando-hints] FAIL probe: OoT item %u: a hint without foreignAreas no longer reads "
                            "\"%s\": \"%s\" (P6)\n",
                    (unsigned)row.item.id, termina.c_str(), oldText.c_str());
            result.failures++;
        }
        if (result.probes <= 3) {
            fprintf(stderr, "[rando-hints] probe: OoT item %u on MM check %u: \"%s\"\n", (unsigned)row.item.id,
                    (unsigned)row.hostCheck, live.c_str());
        }
    }
    ctx->AddHint(probeKey, saved);
    return result;
}

/**
 * (P6, the altar and static-from-data builders) Build, under the remainder's gate,
 * the child altar hint (CreateAltarHint) and the Sheik hint (CreateStaticHintFromData)
 * over the list {crossed A, an item in Hyrule, crossed B}: two OoT items the store
 * hosts only in MM (in different MM areas when the store allows it) around one that
 * an OoT location holds, so a misaligned or swapped area shows. The Hyrule item's
 * location is one already marked hint-accessible, so the builders change nothing
 * about it. Each world's own hint is put back.
 */
static ForeignAreaProbeResult ProbeMultiSlotForeignAreaHints() {
    ForeignAreaProbeResult result;
    auto ctx = Rando::Context::GetInstance();
    std::set<uint16_t> inHyrule;
    for (const RandomizerCheck loc : ctx->allLocations) {
        inHyrule.insert((uint16_t)ctx->GetItemLocation(loc)->GetPlacedRandomizerGet());
    }
    std::vector<RandomizerGet> crossed;
    std::set<uint16_t> seen;
    for (int i = 0; i < Combo_Crossings_Count(GAME_MM); i++) {
        ComboCrossing row;
        if (Combo_Crossings_At(GAME_MM, i, &row) && row.item.originGame == (uint8_t)GAME_OOT &&
            seen.insert(row.item.id).second && inHyrule.count(row.item.id) == 0 &&
            ExpectedForeignArea((RandomizerGet)row.item.id) != COMBO_AREA_NONE) {
            crossed.push_back((RandomizerGet)row.item.id);
        }
    }
    RandomizerGet hyruleItem = RG_NONE;
    RandomizerArea hyruleArea = RA_NONE;
    std::set<RandomizerGet> firstHolders;
    for (const RandomizerCheck loc : ctx->allLocations) {
        const auto* itemLoc = ctx->GetItemLocation(loc);
        const RandomizerGet item = itemLoc->GetPlacedRandomizerGet();
        if (!firstHolders.insert(item).second || item == RG_NONE) {
            continue; // FindItemsAndMarkHinted names an item's first holder only
        }
        if (itemLoc->IsAHintAccessible() && itemLoc->GetAreas().size() == 1) {
            hyruleItem = item;
            hyruleArea = *itemLoc->GetAreas().begin();
            break;
        }
    }
    if (crossed.size() < 2 || hyruleItem == RG_NONE) {
        fprintf(stderr,
                "[rando-hints] FAIL: the multi-slot P6 probe is vacuous: %zu OoT item(s) only in MM, Hyrule item %u "
                "(P6)\n",
                crossed.size(), (unsigned)hyruleItem);
        result.failures++;
        return result;
    }
    const RandomizerGet itemA = crossed[0];
    RandomizerGet itemB = crossed[1];
    for (size_t i = 1; i < crossed.size(); i++) {
        if (ExpectedForeignArea(crossed[i]) != ExpectedForeignArea(itemA)) {
            itemB = crossed[i];
            break;
        }
    }
    const int list[3] = { (int)itemA, (int)hyruleItem, (int)itemB };
    const std::vector<RandomizerGet> slotItems = { itemA, RG_NONE, itemB };
    const std::vector<RandomizerArea> expectedAreas = { RA_TERMINA, hyruleArea, RA_TERMINA };
    const std::string nameA = ForeignAreaLabel(ExpectedForeignArea(itemA));
    const std::string nameB = ForeignAreaLabel(ExpectedForeignArea(itemB));

    struct Build {
        RandomizerHint key;
        const char* builder;
        bool namesB; // the text has a place for the third slot (the altar's three stones)
    };
    const Build builds[] = { { RH_ALTAR_CHILD, "CreateAltarHint", true },
                             { RH_SHEIK_HINT, "CreateStaticHintFromData", false } };
    for (const Build& b : builds) {
        const Rando::Hint saved = *ctx->GetHint(b.key);
        OoT_ComboLogic_TestSetHintingPairedRemainder(1);
        const int built = b.key == RH_ALTAR_CHILD ? Rando_TestCreateAltarHint((int)b.key, list, 3)
                                                  : Rando_TestCreateStaticHintFromData((int)b.key, list, 3);
        OoT_ComboLogic_TestSetHintingPairedRemainder(0);
        if (built == 0) {
            fprintf(stderr, "[rando-hints] FAIL probe %s: the builder built nothing (P6)\n", b.builder);
            result.failures++;
            ctx->AddHint(b.key, saved);
            continue;
        }
        result.probes++;
        const Rando::Hint* probe = ctx->GetHint(b.key);
        const std::vector<RandomizerArea> areas = probe->GetHintedAreas();
        const std::vector<uint8_t> foreign = probe->GetForeignAreas();
        const std::string live = JoinMessages(probe->GetAllMessageStrings(MF_CLEAN));
        const std::string label = std::string("probe ") + b.builder + " {OoT item " + std::to_string(itemA) +
                                  ", OoT item " + std::to_string(hyruleItem) + " in Hyrule, OoT item " +
                                  std::to_string(itemB) + "}";
        if (areas != expectedAreas) {
            fprintf(stderr, "[rando-hints] FAIL %s: %zu area slot(s), not Termina / area %d / Termina (P6)\n",
                    label.c_str(), areas.size(), (int)hyruleArea);
            result.failures++;
        }
        result.failures += CheckForeignAreaSlots(label, areas, foreign, slotItems, nullptr);
        if (live.find(nameA) == std::string::npos || (b.namesB && live.find(nameB) == std::string::npos)) {
            fprintf(stderr, "[rando-hints] FAIL %s: the hint does not name \"%s\"%s%s%s: \"%s\" (P6)\n",
                    label.c_str(), nameA.c_str(), b.namesB ? " and \"" : "", b.namesB ? nameB.c_str() : "",
                    b.namesB ? "\"" : "", live.c_str());
            result.failures++;
        }
        fprintf(stderr, "[rando-hints] %s: \"%s\"\n", label.c_str(), live.c_str());
        ctx->AddHint(b.key, saved);
    }
    return result;
}

/** (P1, #575 H3b) What an OoT hint must call the MM item OoT check `host` yields,
 *  read straight from the crossing store (not through the production path): the
 *  item's name with " (MM)", after its article when `withArticle`. "" when the
 *  store hosts nothing there or MM's describer does not know the item. */
static std::string ExpectedCrossedItemName(uint16_t host, bool withArticle) {
    const SharedItem* item = Combo_Crossings_Lookup(GAME_OOT, host);
    const char* name = item != nullptr ? Combo_DescribeItemName(*item) : nullptr;
    if (name == nullptr) {
        return "";
    }
    const char* article = withArticle ? Combo_DescribeItemArticle(*item) : nullptr;
    return std::string(article != nullptr ? article : "") + name + " (MM)";
}

/** The English name OoT's own item table gives the item OoT check `rc` physically holds. */
static std::string PlacedItemEnglishName(RandomizerCheck rc) {
    auto ctx = Rando::Context::GetInstance();
    return Rando::StaticData::RetrieveItem(ctx->GetItemLocation(rc)->GetPlacedRandomizerGet()).GetName().GetEnglish();
}

/** (P2, #575 H3b) Whether the "Named Item" filter must count crossing host `host` as
 *  major: MM's own grade of the item it yields, read from the store (not through the
 *  production bridge), on OoT's rule. MM's majors, masks and crossed keys are major,
 *  as OoT's own majors and general-pass keys are; maps and compasses (lesser),
 *  heart pieces and containers (health), tokens, stray fairies and junk are not, as
 *  Item::IsMajorItem excludes OoT's own maps, compasses and hearts. */
static bool ExpectedCrossingMajor(uint16_t host, uint8_t* outGrade) {
    const SharedItem* item = Combo_Crossings_Lookup(GAME_OOT, host);
    const uint8_t grade = item != nullptr ? Combo_ItemChestGrade(*item) : (uint8_t)RSBS_CHEST_GRADE_NONE;
    *outGrade = grade;
    return grade == RSBS_CHEST_GRADE_MAJOR || grade == RSBS_CHEST_GRADE_MASK || grade == RSBS_CHEST_GRADE_SMALL_KEY ||
           grade == RSBS_CHEST_GRADE_BOSS_KEY;
}

/**
 * (P1) Hint `hint`'s text and spoiler entry for its slot `slot`, which names crossing
 * host `host`, read with the remainder's gate open (as the paired creation renders it
 * and writes its spoiler): the crossed item's article, name and " (MM)", and not the
 * cover's name when the hint names one location only.
 */
static size_t CheckCrossingHostHint(const std::string& label, Rando::Hint& hint, size_t slot, uint16_t host) {
    size_t failures = 0;
    const std::string expected = ExpectedCrossedItemName(host, true);
    const std::string expectedBare = ExpectedCrossedItemName(host, false);
    const std::string cover = PlacedItemEnglishName((RandomizerCheck)host);
    OoT_ComboLogic_TestSetHintingPairedRemainder(1);
    const std::string text = JoinMessages(hint.GetAllMessageStrings(MF_CLEAN));
    const nlohmann::ordered_json doc = hint.toJSON();
    OoT_ComboLogic_TestSetHintingPairedRemainder(0);
    if (expected.empty()) {
        fprintf(stderr, "[rando-hints] FAIL %s: crossing host %u has no nameable MM item in the store (P1)\n",
                label.c_str(), (unsigned)host);
        return 1;
    }
    if (text.find(expected) == std::string::npos) {
        fprintf(stderr, "[rando-hints] FAIL %s: slot %zu names crossing host %u but the text does not name \"%s\": "
                        "\"%s\" (P1)\n",
                label.c_str(), slot, (unsigned)host, expected.c_str(), text.c_str());
        failures++;
    }
    if (hint.GetHintedLocations().size() == 1 && text.find(cover) != std::string::npos) {
        fprintf(stderr, "[rando-hints] FAIL %s: the text names crossing host %u's cover \"%s\": \"%s\" (P1)\n",
                label.c_str(), (unsigned)host, cover.c_str(), text.c_str());
        failures++;
    }
    if (doc.contains("item") && hint.GetHintedLocations().size() == 1 &&
        (!doc["item"].is_string() || doc["item"].get<std::string>() != expectedBare)) {
        fprintf(stderr, "[rando-hints] FAIL %s: the spoiler's item for crossing host %u is %s, not \"%s\" (P1)\n",
                label.c_str(), (unsigned)host, doc["item"].dump().c_str(), expectedBare.c_str());
        failures++;
    }
    fprintf(stderr, "[rando-hints] %s names crossing host %u: \"%s\"\n", label.c_str(), (unsigned)host, text.c_str());
    return failures;
}

struct CrossingHostProbeResult {
    int probes = 0;
    int major = 0;    // probed hosts the filter must count as major (MM's grade)
    int notMajor = 0; // probed hosts it must not
    size_t failures = 0;
};

/**
 * (P1, P2, P5 for hosts) For every crossing host the store records in OoT: it is
 * hintable; under the remainder's gate the "Named Item" filter counts it as major
 * exactly when MM grades the item it yields major (ExpectedCrossingMajor), and an
 * item hint and an item-area hint built for it (as CreateHint builds a stone hint)
 * name the crossed item, never the cover, in their text and spoiler entry, and
 * survive the JSON round trip; with only the paired spoiler's gate open (as the
 * creation writes that world's spoiler, after the remainder) the spoiler entry and
 * the text still name the crossed item; with every gate shut (no remainder, no
 * paired spoiler, no file loaded) the filter answers the cover's own majority and
 * the same hints name the cover. The probes are built outside the world's hint table.
 */
static CrossingHostProbeResult ProbeCrossingHostHints() {
    CrossingHostProbeResult result;
    auto ctx = Rando::Context::GetInstance();
    const RandomizerHint probeKey = Rando::StaticData::gossipStoneCheckToHint[RC_KF_GOSSIP_STONE];
    for (int i = 0; i < Combo_Crossings_Count(GAME_OOT); i++) {
        ComboCrossing row;
        if (!Combo_Crossings_At(GAME_OOT, i, &row)) {
            continue;
        }
        const RandomizerCheck host = (RandomizerCheck)row.hostCheck;
        Rando::ItemLocation* itemLoc = ctx->GetItemLocation(host);
        const std::string expected = ExpectedCrossedItemName(row.hostCheck, true);
        const std::string bare = ExpectedCrossedItemName(row.hostCheck, false);
        const std::string label = "probe crossing host " + std::to_string(row.hostCheck);
        result.probes++;
        if (!itemLoc->IsHintable()) {
            fprintf(stderr, "[rando-hints] FAIL %s: not hintable (P2)\n", label.c_str());
            result.failures++;
        }
        const RandomizerArea area = itemLoc->GetAreas().empty() ? RA_NONE : itemLoc->GetFirstArea();
        OoT_ComboLogic_TestSetHintingPairedRemainder(1);
        const bool goodOpen = FilterGoodItems(host);
        Rando::Hint itemHint(probeKey, HINT_TYPE_ITEM, "H3b probe", {}, { host }, { area }, {});
        Rando::Hint areaHint(probeKey, HINT_TYPE_ITEM_AREA, "H3b probe", {}, { host }, { area }, {});
        const std::string itemText = JoinMessages(itemHint.GetAllMessageStrings(MF_CLEAN));
        const std::string areaText = JoinMessages(areaHint.GetAllMessageStrings(MF_CLEAN));
        const nlohmann::json areaDoc = nlohmann::json::parse(areaHint.toJSON().dump());
        const Rando::Hint reloaded(probeKey, areaDoc);
        const std::string reloadedText = JoinMessages(reloaded.GetAllMessageStrings(MF_CLEAN));
        OoT_ComboLogic_TestSetHintingPairedRemainder(0);
        OoT_ComboLogic_TestSetWritingSingleBagSpoiler(1);
        const nlohmann::ordered_json spoilerDoc = areaHint.toJSON();
        const std::string spoilerText = JoinMessages(areaHint.GetAllMessageStrings(MF_CLEAN));
        OoT_ComboLogic_TestSetWritingSingleBagSpoiler(0);
        const bool goodShut = FilterGoodItems(host);
        const std::string shutText = JoinMessages(areaHint.GetAllMessageStrings(MF_CLEAN));
        const bool coverMajor = itemLoc->GetPlacedItem().IsMajorItem();
        const std::string cover = PlacedItemEnglishName(host);
        uint8_t grade = RSBS_CHEST_GRADE_NONE;
        const bool majorExpected = ExpectedCrossingMajor(row.hostCheck, &grade);
        (majorExpected ? result.major : result.notMajor)++;
        if (goodOpen != majorExpected) {
            fprintf(stderr,
                    "[rando-hints] FAIL %s: the Named Item filter answers %d for \"%s\", which MM grades %s (expected "
                    "%d) (P2)\n",
                    label.c_str(), goodOpen ? 1 : 0, bare.c_str(), Combo_ChestGradeName(grade), majorExpected ? 1 : 0);
            result.failures++;
        }
        if (expected.empty()) {
            fprintf(stderr, "[rando-hints] FAIL %s: no nameable MM item in the store (P1)\n", label.c_str());
            result.failures++;
            continue;
        }
        const std::pair<const char*, std::string> texts[] = { { "item hint", itemText },
                                                              { "item-area hint", areaText } };
        for (const auto& t : texts) {
            if (t.second.find(expected) == std::string::npos || t.second.find(cover) != std::string::npos) {
                fprintf(stderr, "[rando-hints] FAIL %s: the %s does not name \"%s\" (or names the cover \"%s\"): "
                                "\"%s\" (P1)\n",
                        label.c_str(), t.first, expected.c_str(), cover.c_str(), t.second.c_str());
                result.failures++;
            }
        }
        if (!areaDoc.contains("item") || !areaDoc["item"].is_string() || areaDoc["item"].get<std::string>() != bare) {
            fprintf(stderr, "[rando-hints] FAIL %s: the spoiler's item is %s, not \"%s\" (P1)\n", label.c_str(),
                    areaDoc.contains("item") ? areaDoc["item"].dump().c_str() : "absent", bare.c_str());
            result.failures++;
        }
        const std::string spoilerItem =
            spoilerDoc.contains("item") && spoilerDoc["item"].is_string() ? spoilerDoc["item"].get<std::string>() : "";
        if (spoilerItem != bare || spoilerText.find(expected) == std::string::npos) {
            fprintf(stderr,
                    "[rando-hints] FAIL %s: while the paired spoiler is written (remainder over) the spoiler's item "
                    "is \"%s\" and the text \"%s\", not \"%s\" (P1)\n",
                    label.c_str(), spoilerItem.c_str(), spoilerText.c_str(), bare.c_str());
            result.failures++;
        }
        if (reloadedText != areaText) {
            fprintf(stderr, "[rando-hints] FAIL %s: the JSON round trip changed the hint\n  live:     \"%s\"\n  "
                            "reloaded: \"%s\" (P1)\n",
                    label.c_str(), areaText.c_str(), reloadedText.c_str());
            result.failures++;
        }
        if (goodShut != coverMajor) {
            fprintf(stderr, "[rando-hints] FAIL %s: outside the remainder the Named Item filter reads the store "
                            "(answers %d, the cover's own answer is %d) (P5)\n",
                    label.c_str(), goodShut ? 1 : 0, coverMajor ? 1 : 0);
            result.failures++;
        }
        if (shutText.find(bare) != std::string::npos) {
            fprintf(stderr, "[rando-hints] FAIL %s: outside the remainder, with no file loaded, the hint still names "
                            "\"%s\": \"%s\" (P5)\n",
                    label.c_str(), bare.c_str(), shutText.c_str());
            result.failures++;
        }
        if (result.probes <= 3) {
            fprintf(stderr, "[rando-hints] %s: \"%s\" / \"%s\"\n", label.c_str(), itemText.c_str(),
                    areaText.c_str());
        }
    }
    return result;
}

struct FoolishCheckResult {
    size_t failures = 0;
    int foolishAreas = 0;
    int candidates = 0;
    int foolishHints = 0;
};

/** (P7) Is the item at `rc` IMPORTANT to the foolish rule? See (P7) above. */
static bool P7HoldsImportant(const std::shared_ptr<Rando::Context>& ctx, RandomizerCheck rc) {
    Rando::ItemLocation* il = ctx->GetItemLocation(rc);
    ComboLogicPlacement placement;
    return il->GetPlacedItem().IsMajorItem() || il->IsWothCandidate() ||
           Combo_Logic_GetPlacement(GAME_OOT, (uint16_t)rc, &placement) || OoT_Combo_CheckHostsCrossing((int)rc) != 0;
}

/** (P7) The foolish rule over OoT's own areas, against the barren flags and the
 *  Foolish hints the remainder's pass left on the live context. */
static FoolishCheckResult CheckPairedFoolish(const std::shared_ptr<Rando::Context>& ctx) {
    FoolishCheckResult r;
    std::vector<bool> hintable(RA_MAX, false);
    std::vector<bool> important(RA_MAX, false);
    for (const RandomizerCheck rc : ctx->allLocations) {
        Rando::ItemLocation* il = ctx->GetItemLocation(rc);
        const bool isImportant = P7HoldsImportant(ctx, rc);
        for (const RandomizerArea ra : il->GetAreas()) {
            hintable[ra] = hintable[ra] || il->IsHintable();
            important[ra] = important[ra] || isImportant;
        }
    }
    std::vector<bool> foolish(RA_MAX, false);
    for (int ra = RA_KOKIRI_FOREST; ra <= RA_GANONS_CASTLE; ra++) {
        foolish[ra] = hintable[ra] && !important[ra];
        r.foolishAreas += foolish[ra] ? 1 : 0;
    }
    for (const RandomizerCheck rc : ctx->allLocations) {
        Rando::ItemLocation* il = ctx->GetItemLocation(rc);
        const std::set<RandomizerArea> areas = il->GetAreas();
        bool expected = !areas.empty();
        for (const RandomizerArea ra : areas) {
            expected = expected && foolish[ra];
        }
        const bool marked = il->IsFoolishCandidate();
        r.candidates += marked ? 1 : 0;
        if (marked != expected) {
            if (r.failures < 5) {
                fprintf(stderr,
                        "[rando-hints] FAIL: %s (first area %d) is %sa foolish candidate, but the rule says it is %s "
                        "(P7)\n",
                        Rando::StaticData::GetLocation(rc)->GetName().c_str(), areas.empty() ? -1 : (int)*areas.begin(),
                        marked ? "" : "not ", expected ? "one" : "not");
            }
            r.failures++;
        }
    }
    if (r.candidates == 0) {
        fprintf(stderr,
                "[rando-hints] FAIL: the paired world marks no foolish candidate (%d area(s) are foolish by the rule): "
                "OoT's Foolish entry has nothing to draw (P7)\n",
                r.foolishAreas);
        r.failures++;
    }
    for (int h = RH_NONE + 1; h < RH_MAX; h++) {
        Rando::Hint* hint = ctx->GetHint(static_cast<RandomizerHint>(h));
        if (hint == nullptr || !hint->IsEnabled() || hint->GetHintType() != HINT_TYPE_FOOLISH) {
            continue;
        }
        r.foolishHints++;
        for (const RandomizerArea ra : hint->GetHintedAreas()) {
            const bool ok = ra >= RA_KOKIRI_FOREST && ra <= RA_GANONS_CASTLE && foolish[ra];
            fprintf(stderr, "[rando-hints] Foolish hint %d names area %d (%s) (P7)\n", h, (int)ra,
                    ok ? "foolish" : "NOT foolish");
            if (!ok) {
                r.failures++;
            }
        }
    }
    return r;
}
#endif

#ifdef RSBS_SINGLE_EXECUTABLE
// hints.cpp (#1162): the junk weight of the distribution the context holds.
uint8_t OoT_Rando_HintDistributionJunkWeight();
#endif

struct StoneCheckResult {
    size_t failures = 0;
    size_t stones = 0;     // enabled gossip-stone hints
    size_t junkStones = 0; // hints whose distribution is "Junk"
    size_t trialHints = 0; // HINT_TYPE_TRIAL hints
};

/**
 * (5) #1162: a gossip stone never reads "No Hint", and a Junk hint is a gag line:
 * exactly one hint key in [RHT_JUNK01, RHT_JUNK71] and no hinted location. The
 * defect drew the Junk row through the location-hint path, which rendered a keyless
 * Hint Key hint as hintTextTable[RHT_NONE].
 * (6) a Trial hint names a trial the count says to hint: a required one under 1-3
 * trials, a skipped one under 4-5.
 */
static void CheckStoneHint(const std::shared_ptr<Rando::Context>& ctx, RandomizerHint hintKey, const Rando::Hint& hint,
                           const std::string& hintName, StoneCheckResult* r) {
    const std::string noHint = Rando::StaticData::hintTextTable[RHT_NONE].GetClear().GetEnglish(MF_CLEAN);
    if (hintKey < RH_GANONDORF_HINT) {
        r->stones++;
        for (size_t m = 0; m < hint.GetNumberOfMessages(); m++) {
            const std::string english = hint.GetHintMessage(MF_CLEAN, m).GetEnglish(MF_CLEAN);
            if (english == noHint) {
                fprintf(stderr, "[rando-hints] FAIL %s: the stone reads \"%s\" (distribution \"%s\") (5)\n",
                        hintName.c_str(), english.c_str(), hint.GetDistribution().c_str());
                r->failures++;
            }
        }
    }
    if (hint.GetDistribution() == "Junk") {
        r->junkStones++;
        const std::vector<RandomizerHintTextKey> keys = hint.GetHintTextKeys();
        const bool gag = keys.size() == 1 && keys[0] >= RHT_JUNK01 && keys[0] <= RHT_JUNK71;
        if (!gag || !hint.GetHintedLocations().empty()) {
            fprintf(stderr,
                    "[rando-hints] FAIL %s: a Junk hint carries %zu key(s) (first %d, gag lines are %d..%d) and %zu "
                    "hinted location(s); it must be one gag line and no location (5)\n",
                    hintName.c_str(), keys.size(), keys.empty() ? -1 : (int)keys[0], (int)RHT_JUNK01,
                    (int)RHT_JUNK71, hint.GetHintedLocations().size());
            r->failures++;
        }
    }
    if (hint.GetHintType() == HINT_TYPE_TRIAL) {
        r->trialHints++;
        const uint8_t count = ctx->GetOption(RSK_TRIAL_COUNT).Get();
        for (const TrialKey trial : hint.GetHintedTrials()) {
            const bool required = ctx->GetTrial(trial)->IsRequired();
            if ((count >= 1 && count <= 3 && !required) || (count >= 4 && count <= 5 && required)) {
                fprintf(stderr,
                        "[rando-hints] FAIL %s: with %u required trial(s) a Trial hint names a %s trial (%d) (6)\n",
                        hintName.c_str(), (unsigned)count, required ? "required" : "skipped", (int)trial);
                r->failures++;
            }
        }
    }
}

static int ValidateGeneratedHints(bool pairedWorld, StoneCheckResult* stonesOut = nullptr) {
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
    size_t terminaHints = 0;
    StoneCheckResult stoneCheck;
#ifdef RSBS_SINGLE_EXECUTABLE
    size_t terminaSlots = 0;
    size_t hostHints = 0;
#endif

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

        const std::vector<RandomizerCheck> hintedLocations = hint->GetHintedLocations();
        const std::vector<RandomizerArea> hintedAreas = hint->GetHintedAreas();
#ifdef RSBS_SINGLE_EXECUTABLE
        if (pairedWorld) {
            // (P6) each Termina slot names the area of its own item's first MM host;
            // any other slot names none.
            failures += CheckForeignAreaSlots(hintName, hintedAreas, hint->GetForeignAreas(),
                                              TerminaSlotItemsOf(hintKey, *hint), &terminaSlots);
        }
#endif
        for (size_t li = 0; li < hintedLocations.size(); li++) {
            const RandomizerCheck hintedCheck = hintedLocations[li];
            if (pairedWorld) {
#ifdef RSBS_SINGLE_EXECUTABLE
                if (OoT_Combo_CheckHostsCrossing((int)hintedCheck) != 0) {
                    hostHints++;
                    failures += CheckCrossingHostHint(hintName, *hint, li, (uint16_t)hintedCheck);
                }
#else
                if (OoT_Combo_CheckHostsCrossing((int)hintedCheck) != 0) {
                    fprintf(stderr, "[rando-hints] FAIL %s: names crossing host %d, which holds a cover item (P1)\n",
                            hintName.c_str(), (int)hintedCheck);
                    failures++;
                    continue;
                }
#endif
                if (hintedCheck == RC_UNKNOWN_CHECK) {
                    if (li < hintedAreas.size() && hintedAreas[li] == RA_TERMINA) {
                        terminaHints++;
                    } else {
                        fprintf(stderr,
                                "[rando-hints] FAIL %s: target %zu is at no OoT location and the hint does not name "
                                "Termina (area %d) (P3)\n",
                                hintName.c_str(), li, li < hintedAreas.size() ? (int)hintedAreas[li] : -1);
                        failures++;
                    }
                    continue;
                }
            }
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

        // (5)/(6) #1162: the stone's text and the Junk and Trial rows.
        CheckStoneHint(ctx, hintKey, *hint, hintName, &stoneCheck);
    }
    failures += stoneCheck.failures;
    if (stonesOut != nullptr) {
        *stonesOut = stoneCheck;
    }

    if (hintsChecked == 0) {
        fprintf(stderr, "[rando-hints] no enabled hints were generated; the lock would vacuously pass\n");
        return 6;
    }

#ifdef RSBS_SINGLE_EXECUTABLE
    if (pairedWorld) {
        int crossingHosts = 0;
        for (int i = 0; i < Combo_Crossings_Count(GAME_OOT); i++) {
            ComboCrossing row;
            if (!Combo_Crossings_At(GAME_OOT, i, &row)) {
                continue;
            }
            crossingHosts++;
            if (!ctx->GetItemLocation((RandomizerCheck)row.hostCheck)->IsHintable()) {
                fprintf(stderr, "[rando-hints] FAIL: crossing host %u is not hintable (P2)\n",
                        (unsigned)row.hostCheck);
                failures++;
            }
        }
        int ootItemsInMM = 0;
        for (int i = 0; i < Combo_Crossings_Count(GAME_MM); i++) {
            ComboCrossing row;
            if (!Combo_Crossings_At(GAME_MM, i, &row) || row.item.originGame != (uint8_t)GAME_OOT) {
                continue;
            }
            ootItemsInMM++;
            if (Rando_HintAreaFromCrossingStore((int)row.item.id) != (int)RA_TERMINA) {
                fprintf(stderr, "[rando-hints] FAIL: OoT item %u is hosted in MM but a hint would not name Termina "
                                "(P4)\n",
                        (unsigned)row.item.id);
                failures++;
            }
            if (Rando_HintAreaForItemOutsideHyrule((int)row.item.id) != (int)RA_NONE) {
                fprintf(stderr, "[rando-hints] FAIL: outside the paired remainder the hint pass still reads the "
                                "crossing store: OoT item %u would be hinted as area %d (P5)\n",
                        (unsigned)row.item.id, Rando_HintAreaForItemOutsideHyrule((int)row.item.id));
                failures++;
            }
            if (Rando_HintForeignAreaForItem((int)row.item.id) != (int)COMBO_AREA_NONE) {
                fprintf(stderr, "[rando-hints] FAIL: outside the paired remainder the hint pass still reads the "
                                "crossing store: OoT item %u would name MM area %d (P5)\n",
                        (unsigned)row.item.id, Rando_HintForeignAreaForItem((int)row.item.id));
                failures++;
            }
            // (P4, #575 H3a) the exact area: the item's FIRST MM host, in store order.
            if (P4FirstHostOf(row.item.id) == row.hostCheck) {
                const ComboArea hostArea = Combo_Area_OfCheck((uint8_t)GAME_MM, row.hostCheck);
                const int answered = Rando_HintForeignAreaFromCrossingStore((int)row.item.id);
                if (hostArea.area == COMBO_AREA_NONE || answered != (int)hostArea.area) {
                    fprintf(stderr,
                            "[rando-hints] FAIL: OoT item %u is hosted on MM check %u in \"%s\" but a hint would "
                            "name MM area %d (P4)\n",
                            (unsigned)row.item.id, (unsigned)row.hostCheck,
                            Combo_Area_Name(hostArea) != nullptr ? Combo_Area_Name(hostArea) : "?", answered);
                    failures++;
                }
            }
        }
        const ForeignAreaProbeResult probe = ProbeForeignAreaHints();
        failures += probe.failures;
        const ForeignAreaProbeResult multiProbe = ProbeMultiSlotForeignAreaHints();
        failures += multiProbe.failures;
        const CrossingHostProbeResult hostProbe = ProbeCrossingHostHints();
        failures += hostProbe.failures;
        const FoolishCheckResult foolishCheck = CheckPairedFoolish(ctx);
        failures += foolishCheck.failures;
        fprintf(stderr,
                "[rando-hints] paired world: %d foolish area(s) by the rule, %d foolish candidate location(s), %d "
                "Foolish hint(s) drawn (P7)\n",
                foolishCheck.foolishAreas, foolishCheck.candidates, foolishCheck.foolishHints);
        if (foolishCheck.foolishHints == 0) {
            fprintf(stderr, "[rando-hints] the pinned seed drew no Foolish hint; P7's hint half is vacuous (its "
                            "candidate half is not)\n");
        }
        fprintf(stderr,
                "[rando-hints] paired world: %d crossing hosts in OoT (each hintable), %d OoT items in MM, "
                "%zu hinted target(s) named Termina\n",
                crossingHosts, ootItemsInMM, terminaHints);
        fprintf(stderr,
                "[rando-hints] paired world: %zu generated hint slot(s) name a crossing host, %d crossing host "
                "probe(s), %d graded major by MM and %d not (P1/P2)\n",
                hostHints, hostProbe.probes, hostProbe.major, hostProbe.notMajor);
        if (hostProbe.major == 0 || hostProbe.notMajor == 0) {
            fprintf(stderr,
                    "[rando-hints] FAIL: the probes reach only one side of the Named Item filter (%d major, %d not), "
                    "so its majority rule is not exercised (P2)\n",
                    hostProbe.major, hostProbe.notMajor);
            failures++;
        }
        fprintf(stderr,
                "[rando-hints] paired world: %zu generated Termina slot(s) checked for their own item's MM area, %d "
                "static item hint probe(s), %d altar / static-from-data probe(s) (P6)\n",
                terminaSlots, probe.probes, multiProbe.probes);
        if (crossingHosts == 0 || ootItemsInMM == 0) {
            fprintf(stderr, "[rando-hints] the paired world crossed nothing in one direction; P2/P4 are vacuous\n");
            return 7;
        }
        if (probe.probes == 0) {
            fprintf(stderr, "[rando-hints] every OoT item in MM also sits in Hyrule; the P6 probe is vacuous\n");
            return 8;
        }
    }
#endif

    fprintf(stderr, "[rando-hints] %zu gossip stone(s), %zu Junk (gag) hint(s), %zu Trial hint(s) (5)/(6)\n",
            stoneCheck.stones, stoneCheck.junkStones, stoneCheck.trialHints);
    fprintf(stderr, "[rando-hints] checked %zu enabled hints, %zu failures\n", hintsChecked, failures);
    if (failures != 0) {
        return 1;
    }
#ifdef RSBS_SINGLE_EXECUTABLE
    if (OoT_Rando_HintDistributionJunkWeight() > 0 && stoneCheck.stones > 0 && stoneCheck.junkStones == 0) {
        fprintf(stderr, "[rando-hints] the distribution weighs Junk but the seed drew no Junk stone; check (5)'s "
                        "Junk half is vacuous\n");
        return 10;
    }
#endif
    return 0;
}

extern "C" int Rando_HeadlessHintValidityTest(const char* seedStr) {
    // OoT's NATIVE general pass (lane K11): this lock validates the hints OoT's
    // own fill tail writes over a world whose general pass it placed, so it asks
    // Fill() not to defer that pass to a paired creation's single bag.
    (void)OoT_ComboLogic_TestSetNativeGeneralPass(1);
    int rc = Rando_HeadlessSeedTest(seedStr);
    if (rc != 0) {
        fprintf(stderr, "[rando-hints] generation failed rc=%d\n", rc);
        return rc;
    }
    return ValidateGeneratedHints(false);
}

/** One more native generation of `seedStr` with one setting CVar overridden, then
 *  the hint checks over it; the CVar is cleared again before returning (never
 *  stored: a stored rando CVar poisons later rows). */
static int HintValidityUnder(const char* seedStr, const char* cvar, int value, StoneCheckResult* stones) {
    CVarSetInteger(cvar, value);
    (void)OoT_ComboLogic_TestSetNativeGeneralPass(1);
    const int gen = Rando_HeadlessSeedTest(seedStr);
    CVarClear(cvar);
    if (gen != 0) {
        fprintf(stderr, "[rando-hints] generation with %s=%d failed rc=%d\n", cvar, value, gen);
        return gen;
    }
    fprintf(stderr, "[rando-hints] --- %s=%d ---\n", cvar, value);
    return ValidateGeneratedHints(false, stones);
}

/**
 * #1162, the seed-independent half of the hint-validity lock:
 *   U. Hint Distribution = Useless: every one of the 40 gossip stones is a Junk
 *      stone reading a gag line (the defect made all 40 read "No Hint").
 *   T. Ganon's Trials count = 2: the Trial hints name exactly the 2 required
 *      trials (the defect discarded the filter and hinted all six).
 * @return 0 when both hold.
 */
extern "C" int Rando_HeadlessHintValidityVariantsTest(const char* seedStr) {
    const size_t kStones = (size_t)RH_GANONDORF_HINT - 1;
    StoneCheckResult useless;
    int rc = HintValidityUnder(seedStr, CVAR_RANDOMIZER_SETTING("HintDistribution"), RO_HINT_DIST_USELESS, &useless);
    if (rc == 0 && (useless.stones != kStones || useless.junkStones != kStones)) {
        fprintf(stderr, "[rando-hints] FAIL Useless: %zu of %zu stones enabled, %zu of them Junk; every stone must be "
                        "a gag line (5)\n",
                useless.stones, kStones, useless.junkStones);
        rc = 1;
    }
    if (rc != 0) {
        return rc;
    }
    StoneCheckResult trials;
    rc = HintValidityUnder(seedStr, CVAR_RANDOMIZER_SETTING("GanonTrialCount"), 2, &trials);
    if (rc == 0 && trials.trialHints != 2) {
        fprintf(stderr, "[rando-hints] FAIL two trials: %zu Trial hint(s); Balanced hints the 2 required trials "
                        "once each (6)\n",
                trials.trialHints);
        rc = 1;
    }
    return rc;
}

/**
 * The same hint validity over the world the SHIPPED paired path produced (PR #743
 * review): the caller has run a paired generation, MM's creation-time half (the
 * single-bag fill) and OoT's remainder, whose tail wrote these hints. The three
 * #441 rows above validate OoT's native general pass, which a paired file no
 * longer ships.
 */
extern "C" int Rando_ValidatePairedWorldHints(void) {
    return ValidateGeneratedHints(true);
}

#ifdef RSBS_SINGLE_EXECUTABLE
/**
 * #575 H3b, ComboSingleBag D2b: the caller has generated a SOLO world (OoT's native
 * general pass) after a paired creation, so the crossing store still holds the
 * paired world's crossings (it outlives its creation) while the hints are this
 * world's. Its hints name no crossed item:
 *   - the #441 checks over its hints;
 *   - no hint's text says " (MM)";
 *   - every OoT check the store records as a crossing host is probed in THIS world
 *     with an item-area hint built for it, as CreateHint builds one: its text and
 *     spoiler entry name this world's own item there, never the paired world's MM
 *     item, and the "Named Item" filter answers this world's own item's majority.
 * Run as production runs it: no remainder, no paired spoiler, no file loaded.
 * @return 0 when all hold; 9 when the store holds no OoT-hosted crossing (vacuous).
 */
extern "C" int Rando_ValidateSoloWorldHintsAfterPairing(void) {
    if (Combo_Crossings_Count(GAME_OOT) == 0) {
        fprintf(stderr, "[rando-hints] solo world: the store holds no OoT-hosted crossing; the check is vacuous\n");
        return 9;
    }
    const int base = ValidateGeneratedHints(false);
    if (base != 0) {
        return base;
    }
    auto ctx = Rando::Context::GetInstance();
    size_t failures = 0;
    for (int h = RH_NONE + 1; h < RH_MAX; h++) {
        Rando::Hint* hint = ctx->GetHint((RandomizerHint)h);
        if (hint == nullptr || !hint->IsEnabled()) {
            continue;
        }
        const std::string text = JoinMessages(hint->GetAllMessageStrings(MF_CLEAN));
        if (text.find(" (MM)") != std::string::npos) {
            fprintf(stderr, "[rando-hints] FAIL solo world: hint %d names a crossed item: \"%s\"\n", h, text.c_str());
            failures++;
        }
    }
    const RandomizerHint probeKey = Rando::StaticData::gossipStoneCheckToHint[RC_KF_GOSSIP_STONE];
    int probes = 0;
    for (int i = 0; i < Combo_Crossings_Count(GAME_OOT); i++) {
        ComboCrossing row;
        if (!Combo_Crossings_At(GAME_OOT, i, &row)) {
            continue;
        }
        const RandomizerCheck rc = (RandomizerCheck)row.hostCheck;
        Rando::ItemLocation* itemLoc = ctx->GetItemLocation(rc);
        const RandomizerGet placed = itemLoc->GetPlacedRandomizerGet();
        if (placed == RG_NONE || placed == RG_ICE_TRAP || itemLoc->GetAreas().empty()) {
            continue; // nothing to name, or a trick name in place of the item's
        }
        probes++;
        const std::string crossed = ExpectedCrossedItemName(row.hostCheck, false);
        const std::string own = PlacedItemEnglishName(rc);
        Rando::Hint probe(probeKey, HINT_TYPE_ITEM_AREA, "H3b solo probe", {}, { rc }, { itemLoc->GetFirstArea() }, {});
        const std::string text = JoinMessages(probe.GetAllMessageStrings(MF_CLEAN));
        const nlohmann::ordered_json doc = probe.toJSON();
        const std::string docItem = doc.contains("item") && doc["item"].is_string() ? doc["item"].get<std::string>() : "";
        if ((!crossed.empty() && text.find(crossed) != std::string::npos) || text.find(own) == std::string::npos ||
            docItem != own) {
            fprintf(stderr, "[rando-hints] FAIL solo world: check %u (the paired world's host of \"%s\") holds \"%s\" "
                            "here, but its hint reads \"%s\" and its spoiler item \"%s\"\n",
                    (unsigned)row.hostCheck, crossed.c_str(), own.c_str(), text.c_str(), docItem.c_str());
            failures++;
        }
        if (FilterGoodItems(rc) != itemLoc->GetPlacedItem().IsMajorItem()) {
            fprintf(stderr, "[rando-hints] FAIL solo world: the Named Item filter reads the paired world's crossing at "
                            "check %u\n",
                    (unsigned)row.hostCheck);
            failures++;
        }
        if (probes <= 2) {
            fprintf(stderr, "[rando-hints] solo world: check %u (the paired world's host of \"%s\"): \"%s\"\n",
                    (unsigned)row.hostCheck, crossed.c_str(), text.c_str());
        }
    }
    fprintf(stderr, "[rando-hints] solo world: %d of the paired world's %d OoT crossing host(s) probed, %zu failures\n",
            probes, Combo_Crossings_Count(GAME_OOT), failures);
    if (probes == 0) {
        fprintf(stderr, "[rando-hints] solo world: no host could be probed; the check is vacuous\n");
        return 9;
    }
    return failures == 0 ? 0 : 1;
}

/** One spoiler hint entry's (location, item) pairs ("location"/"item", or the
 *  parallel "locations"/"items"). */
static std::vector<std::pair<std::string, std::string>> SpoilerHintPairs(const nlohmann::json& entry) {
    std::vector<std::pair<std::string, std::string>> pairs;
    if (entry.contains("location") && entry["location"].is_string() && entry.contains("item") &&
        entry["item"].is_string()) {
        pairs.emplace_back(entry["location"].get<std::string>(), entry["item"].get<std::string>());
    }
    if (entry.contains("locations") && entry["locations"].is_array() && entry.contains("items") &&
        entry["items"].is_array()) {
        for (size_t i = 0; i < entry["locations"].size() && i < entry["items"].size(); i++) {
            if (entry["locations"][i].is_string() && entry["items"][i].is_string()) {
                pairs.emplace_back(entry["locations"][i].get<std::string>(), entry["items"][i].get<std::string>());
            }
        }
    }
    return pairs;
}

/**
 * TEST BRIDGE (#575 H3b, combo-creation-event leg 11): the OoT spoiler the paired
 * creation WROTE (OoT_ComboLogic_FinishGeneralPass, under its paired-spoiler gate),
 * read back from disk: every hint entry that names an OoT crossing host of the store
 * names, as its item, the MM item that host yields ("Elegy of Emptiness (MM)"),
 * never the cover the host physically holds.
 * @param outNamed  the entries naming a crossing host (0 makes the check vacuous).
 * @return the number of failures (each printed); -1 when the file cannot be read.
 */
extern "C" int Rando_TestCheckPairedSpoilerCrossingHints(const char* path, int* outNamed) {
    *outNamed = 0;
    nlohmann::json doc;
    try {
        std::ifstream in(path);
        if (!in.is_open()) {
            return -1;
        }
        in >> doc;
    } catch (...) { return -1; }
    int failures = 0;
    for (const char* section : { "Gossip Stone Hints", "Static Hints" }) {
        if (!doc.contains(section) || !doc[section].is_object()) {
            continue;
        }
        for (const auto& kv : doc[section].items()) {
            for (const auto& pair : SpoilerHintPairs(kv.value())) {
                const auto found = Rando::StaticData::locationNameToEnum.find(pair.first);
                if (found == Rando::StaticData::locationNameToEnum.end() ||
                    Combo_Crossings_Lookup(GAME_OOT, (uint16_t)found->second) == nullptr) {
                    continue;
                }
                (*outNamed)++;
                const std::string expected = ExpectedCrossedItemName((uint16_t)found->second, false);
                if (expected.empty() || pair.second != expected) {
                    fprintf(stderr,
                            "[rando-hints] FAIL paired spoiler: %s \"%s\" names crossing host %u (%s) with the item "
                            "\"%s\", not \"%s\"\n",
                            section, kv.key().c_str(), (unsigned)found->second, pair.first.c_str(),
                            pair.second.c_str(), expected.c_str());
                    failures++;
                } else if (*outNamed <= 3) {
                    fprintf(stderr, "[rando-hints] paired spoiler: %s \"%s\" names crossing host %u: \"%s\"\n",
                            section, kv.key().c_str(), (unsigned)found->second, pair.second.c_str());
                }
            }
        }
    }
    return failures;
}

/**
 * TEST BRIDGE (#575 H3b, IntPairedFirstCrossing): the loaded-file term of the hint
 * text's gate (OoT_Combo_HintTextCrossingAt: a paired file loaded and played),
 * checked in play, where a gossip stone renders its hint. The paired drive calls it
 * with its paired file loaded and OoT in gameplay; neither creation gate (the
 * remainder's, the paired spoiler's) is open there. For every OoT crossing host of
 * the store, an item-area hint built for it (as CreateHint builds a stone hint)
 * names the crossed MM item with its article and " (MM)", never the cover; and every
 * enabled item or item-area hint of the loaded world that names a host (what its
 * stone says when read) names the same.
 * @param outProbes  the hosts probed; @param outStones  the loaded hints naming one.
 * @return the number of failures (each printed with "[H3B]"); -1 when a creation
 *         gate is open, so the check would not be testing the loaded-file term.
 */
extern "C" int Rando_TestCheckLoadedCrossingHostHints(int* outProbes, int* outStones) {
    *outProbes = 0;
    *outStones = 0;
    if (OoT_ComboLogic_HintingPairedRemainder() != 0 || OoT_ComboLogic_WritingSingleBagSpoiler() != 0) {
        fprintf(stderr, "[H3B] a creation gate is open in play; the loaded-file term is not what is tested\n");
        return -1;
    }
    auto ctx = Rando::Context::GetInstance();
    int failures = 0;
    const RandomizerHint probeKey = Rando::StaticData::gossipStoneCheckToHint[RC_KF_GOSSIP_STONE];
    for (int i = 0; i < Combo_Crossings_Count(GAME_OOT); i++) {
        ComboCrossing row;
        if (!Combo_Crossings_At(GAME_OOT, i, &row)) {
            continue;
        }
        const RandomizerCheck host = (RandomizerCheck)row.hostCheck;
        Rando::ItemLocation* itemLoc = ctx->GetItemLocation(host);
        const std::string expected = ExpectedCrossedItemName(row.hostCheck, true);
        if (itemLoc == nullptr || itemLoc->GetAreas().empty() || expected.empty()) {
            continue;
        }
        (*outProbes)++;
        const std::string cover = PlacedItemEnglishName(host);
        Rando::Hint probe(probeKey, HINT_TYPE_ITEM_AREA, "H3b loaded probe", {}, { host }, { itemLoc->GetFirstArea() },
                          {});
        const std::string text = JoinMessages(probe.GetAllMessageStrings(MF_CLEAN));
        if (text.find(expected) == std::string::npos || text.find(cover) != std::string::npos) {
            fprintf(stderr,
                    "[H3B] FAIL: in play, a stone hint for crossing host %u reads \"%s\", not \"%s\" (cover \"%s\")\n",
                    (unsigned)row.hostCheck, text.c_str(), expected.c_str(), cover.c_str());
            failures++;
        } else if (*outProbes <= 2) {
            fprintf(stderr, "[H3B] in play, a stone hint for crossing host %u reads \"%s\"\n", (unsigned)row.hostCheck,
                    text.c_str());
        }
    }
    for (int h = RH_NONE + 1; h < RH_MAX; h++) {
        Rando::Hint* hint = ctx->GetHint((RandomizerHint)h);
        if (hint == nullptr || !hint->IsEnabled() ||
            (hint->GetHintType() != HINT_TYPE_ITEM && hint->GetHintType() != HINT_TYPE_ITEM_AREA)) {
            continue;
        }
        const std::vector<RandomizerCheck> locations = hint->GetHintedLocations();
        for (const RandomizerCheck rc : locations) {
            if (Combo_Crossings_Lookup(GAME_OOT, (uint16_t)rc) == nullptr) {
                continue;
            }
            (*outStones)++;
            const std::string expected = ExpectedCrossedItemName((uint16_t)rc, true);
            const std::string text = JoinMessages(hint->GetAllMessageStrings(MF_CLEAN));
            if (expected.empty() || text.find(expected) == std::string::npos) {
                fprintf(stderr, "[H3B] FAIL: in play, hint %d names crossing host %u but reads \"%s\", not \"%s\"\n", h,
                        (unsigned)rc, text.c_str(), expected.c_str());
                failures++;
            } else {
                fprintf(stderr, "[H3B] in play, hint %d names crossing host %u: \"%s\"\n", h, (unsigned)rc,
                        text.c_str());
            }
        }
    }
    fprintf(stderr, "[H3B] in play: %d crossing host(s) probed, %d loaded hint slot(s) name one, %d failure(s)\n",
            *outProbes, *outStones, failures);
    return failures;
}
#endif

/**
 * TEST BRIDGE (combo-creation-event; PR #743 review): reload the OoT spoiler the
 * paired creation wrote, the way file select does (Context::ParseSpoiler), and the
 * same document with its paired-world markers stripped.
 *
 * @param path              the spoiler the creation wrote (absolute).
 * @param outMarked         1 when the document carries the paired-world markers.
 * @param outPairedLoaded   Randomizer_IsSpoilerLoaded after parsing it (must be 0:
 *                          refused).
 * @param outStrippedLoaded the same after parsing the stripped copy (must be 1: the
 *                          refusal keys on the markers, not on the document).
 * @param outRefusalToast   1 when the toast the paired parse queued is, prefix and
 *                          message, exactly OoT_EmitPairedSpoilerRefusalToast's copy
 *                          (src/common/pairing_refusal_toast.h): what holds the
 *                          refusal SITE to the copy pairing-refusal-toast-fit keeps
 *                          on screen. 0 otherwise, and the queued text is printed.
 * @return 0 when both parses ran; nonzero when the file could not be read or the
 *         stripped copy could not be written.
 */
extern "C" int Rando_TestReloadPairedSpoiler(const char* path, int* outMarked, int* outPairedLoaded,
                                             int* outStrippedLoaded, int* outRefusalToast) {
    nlohmann::json doc;
    try {
        std::ifstream in(path);
        if (!in.is_open()) {
            return 1;
        }
        in >> doc;
    } catch (...) { return 2; }
    *outMarked = (doc.is_object() && doc.contains("combo") && doc.contains("rsbsSingleBagWorld")) ? 1 : 0;

    auto ctx = Rando::Context::GetInstance();
    OoT_Notification_ClearForTest();
    ctx->ParseSpoiler(path);
    *outPairedLoaded = ctx->IsSpoilerLoaded() ? 1 : 0;
    {
        ComboNotification toast;
        memset(&toast, 0, sizeof(toast));
        char expected[128];
        Combo_PairingRefusalToastMessage(RSBS_PAIRING_REFUSAL_OOT_SPOILER, nullptr, expected, sizeof(expected));
        const char* expectedPrefix = Combo_PairingRefusalToastPrefix(RSBS_PAIRING_REFUSAL_OOT_SPOILER);
        const bool queued = OoT_Notification_PeekLastForTest(&toast) == 1;
        const std::string prefix = queued && toast.prefix != nullptr ? toast.prefix : "";
        const std::string message = queued && toast.message != nullptr ? toast.message : "";
        *outRefusalToast = (queued && prefix == expectedPrefix && message == expected) ? 1 : 0;
        if (*outRefusalToast == 0) {
            printf("[TEST] spoiler reload: the refusal queued '%s %s' (queued=%d); the emitter's copy is '%s %s'\n",
                   prefix.c_str(), message.c_str(), queued ? 1 : 0, expectedPrefix, expected);
        }
        OoT_Notification_ClearForTest();
    }

    doc.erase("combo");
    doc.erase("rsbsSingleBagWorld");
    const std::string stripped = std::string(path) + ".stripped-test.json";
    try {
        std::ofstream out(stripped);
        if (!out.is_open()) {
            return 3;
        }
        out << doc.dump();
    } catch (...) { return 3; }
    ctx->ParseSpoiler(stripped.c_str());
    *outStrippedLoaded = ctx->IsSpoilerLoaded() ? 1 : 0;
    std::remove(stripped.c_str());
    return 0;
}

// Implemented in SaveManager.cpp (which owns the private SaveRandomizer /
// LoadRandomizer and the section json cursor). Drives the REAL randomizer
// section through save -> Rando::Context reset -> reload, in memory, exactly as
// loading a saved file does.
extern "C" void Rando_TestRoundTripRandomizerSection(int* outResetCleared, int* outPlacementRestored);

// Implemented in HintAreaOoT.cpp (#575 H1): the OoT area adapter's answer for
// every location, recorded before the reload and compared after it.
extern "C" int OoT_HintArea_TestSnapshot(void);
extern "C" int OoT_HintArea_TestCompareSnapshot(void);

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
    // OoT's NATIVE general pass (lane K11): this lock validates the hints OoT's
    // own fill tail writes over a world whose general pass it placed, so it asks
    // Fill() not to defer that pass to a paired creation's single bag.
    (void)OoT_ComboLogic_TestSetNativeGeneralPass(1);
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

    // #575 H1: the shared area vocabulary's OoT adapter names a location's area
    // from ItemLocation areas, which only generation and the spoiler loader set.
    // Record its answer for every location now; the reload must give them back.
    const int areasBefore = OoT_HintArea_TestSnapshot();
    if (areasBefore == 0) {
        fprintf(stderr, "[hint-reload] no chest has an area before the reload; the area check would be vacuous\n");
        return 8;
    }

#ifdef RSBS_SINGLE_EXECUTABLE
    // #575 H3a: a slot that names an MM area beside RA_TERMINA must survive the
    // save's hint section (SaveManager writes hints field by field, then reads them
    // back through Hint(key, json)). This native world crosses nothing, so the
    // Ganondorf hint's slot is set to one such slot by hand before the save.
    const ComboArea foreignProbeArea = { (uint8_t)GAME_MM, (uint8_t)COMBO_AREA_MM_SOUTHERN_SWAMP };
    const std::string foreignProbeName = Combo_Area_Name(foreignProbeArea);
    std::vector<std::string> foreignProbeBefore;
    size_t foreignProbeFailures = 0;
    {
        const std::vector<RandomizerHintTextKey> probeKeys = { RHT_GANONDORF_HINT_LA_ONLY };
        const std::vector<RandomizerCheck> probeLocations = { RC_UNKNOWN_CHECK };
        const std::vector<RandomizerArea> probeAreas = { RA_TERMINA };
        Rando::Hint probe(RH_GANONDORF_HINT, HINT_TYPE_AREA, probeKeys, probeLocations, probeAreas, {}, true);
        probe.SetForeignAreas({ foreignProbeArea.area });
        ctx->AddHint(RH_GANONDORF_HINT, probe);
        foreignProbeBefore = ctx->GetHint(RH_GANONDORF_HINT)->GetAllMessageStrings(MF_CLEAN);
        bool named = false;
        for (const std::string& m : foreignProbeBefore) {
            named = named || m.find(foreignProbeName) != std::string::npos;
        }
        if (!named) {
            fprintf(stderr, "[hint-reload] FAIL before the reload the Termina slot does not name \"%s\": \"%s\"\n",
                    foreignProbeName.c_str(), foreignProbeBefore.empty() ? "" : foreignProbeBefore[0].c_str());
            foreignProbeFailures++;
        }
    }
#endif

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

#ifdef RSBS_SINGLE_EXECUTABLE
    {
        Rando::Hint* probe = reloaded->GetHint(RH_GANONDORF_HINT);
        const std::vector<std::string> afterProbe =
            probe != nullptr ? probe->GetAllMessageStrings(MF_CLEAN) : std::vector<std::string>{};
        const std::vector<uint8_t> foreignAfter = probe != nullptr ? probe->GetForeignAreas() : std::vector<uint8_t>{};
        if (afterProbe != foreignProbeBefore || foreignAfter != std::vector<uint8_t>{ foreignProbeArea.area }) {
            fprintf(stderr,
                    "[hint-reload] FAIL the Termina slot naming \"%s\" did not survive the reload (%zu MM area(s) "
                    "after)\n  live:     \"%s\"\n  reloaded: \"%s\"\n",
                    foreignProbeName.c_str(), foreignAfter.size(),
                    foreignProbeBefore.empty() ? "" : foreignProbeBefore[0].c_str(),
                    afterProbe.empty() ? "" : afterProbe[0].c_str());
            foreignProbeFailures++;
        } else {
            fprintf(stderr, "[hint-reload] the Termina slot still names \"%s\" after the reload: \"%s\"\n",
                    foreignProbeName.c_str(), afterProbe.empty() ? "" : afterProbe[0].c_str());
        }
        failures += foreignProbeFailures;
    }
#endif

    const int areasChanged = OoT_HintArea_TestCompareSnapshot();
    fprintf(stderr, "[hint-reload] checked %d location areas across a reload, %d changed\n", areasBefore, areasChanged);
    if (areasChanged != 0) {
        failures++;
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
    // OoT's NATIVE general pass (lane K11): this lock validates the hints OoT's
    // own fill tail writes over a world whose general pass it placed, so it asks
    // Fill() not to defer that pass to a paired creation's single bag.
    (void)OoT_ComboLogic_TestSetNativeGeneralPass(1);
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

/**
 * The canonical OoT placement hash: "<rc>:<placedItem>;" for every location in
 * ascending RandomizerCheck order, folded through the project's FNV-1a so a digest
 * file stays small and stable. Shared by the OoT digest above and by the MM half's
 * digest, which folds OoT's FINAL world after the single-bag fill (lane K11).
 */
extern "C" uint32_t Rando_HeadlessPlacementHash(size_t* outPlacedCount) {
    auto ctx = Rando::Context::GetInstance();
    size_t placedCount = 0;
    std::string blob;
    if (ctx) {
        blob.reserve(static_cast<size_t>(RC_MAX) * 8);
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
    }
    if (outPlacedCount != nullptr) {
        *outPlacedCount = placedCount;
    }
    return SohUtils::Hash(blob);
}

/**
 * #949: what a player of OoT's world READS and PAYS and STARTS WITH, beside where
 * its items are: every enabled hint, every priced check, the starting inventory and
 * the entrance table, each folded in fixed enum order through the project's FNV-1a
 * so a golden pins it and a self-diff row compares it. Placement hashes cannot see
 * any of these: a hint drawn in a different order, a price drawn from a different
 * stream position or a starting item granted on one platform only all leave every
 * placement where it was.
 *
 *   ootHintCount / ootHintDigest   each enabled hint in RandomizerHint order: its key,
 *                                  type, hinted checks, hinted areas and every message
 *                                  in English (MF_CLEAN), so a moved area draw, text
 *                                  draw or foolish area all move the digest;
 *   ootPriceCount / ootPriceDigest each check whose GetPrice() is non-zero, in
 *                                  RandomizerCheck order, with its price;
 *   ootStartingItems               <count>/<digest> of the starting inventory in the
 *                                  order generation granted it;
 *   entranceCount / entranceDigest every entrance override the shuffle wrote (count:
 *                                  the non-empty rows), in table order.
 *
 * Call it on a COMPLETE world: a paired generation stops at its general pass, so the
 * MM writers call it after OoT's remainder (which builds the hints) has run.
 */
extern "C" void Rando_HeadlessWriteOoTContentDigest(FILE* out) {
    auto ctx = Rando::Context::GetInstance();
    if (out == nullptr || !ctx) {
        return;
    }
    std::string hintBlob;
    int hintCount = 0;
    for (int h = RH_NONE + 1; h < RH_MAX; h++) {
        Rando::Hint* hint = ctx->GetHint(static_cast<RandomizerHint>(h));
        if (hint == nullptr || !hint->IsEnabled()) {
            continue;
        }
        hintCount++;
        hintBlob += std::to_string(h) + '|' + std::to_string(static_cast<int>(hint->GetHintType())) + '|';
        for (const RandomizerCheck rc : hint->GetHintedLocations()) {
            hintBlob += std::to_string(static_cast<int>(rc)) + ',';
        }
        hintBlob += '|';
        for (const RandomizerArea area : hint->GetHintedAreas()) {
            hintBlob += std::to_string(static_cast<int>(area)) + ',';
        }
        for (size_t m = 0; m < hint->GetNumberOfMessages(); m++) {
            hintBlob += '|' + hint->GetHintMessage(MF_CLEAN, m).GetEnglish(MF_CLEAN);
        }
        hintBlob += '\n';
    }
    std::string priceBlob;
    int priceCount = 0;
    for (int i = 0; i < RC_MAX; i++) {
        const uint16_t price = ctx->GetItemLocation(static_cast<RandomizerCheck>(i))->GetPrice();
        if (price != 0) {
            priceCount++;
            priceBlob += std::to_string(i) + ':' + std::to_string(price) + ';';
        }
    }
    std::string startBlob;
    for (const RandomizerGet item : StartingInventory) {
        startBlob += std::to_string(static_cast<int>(item)) + ';';
    }
    std::string entranceBlob;
    int entranceCount = 0;
    for (const EntranceOverride& e : ctx->GetEntranceShuffler()->entranceOverrides) {
        if (e.type == 0 && e.index == 0 && e.override == 0) {
            continue;
        }
        entranceCount++;
        entranceBlob += std::to_string(e.type) + ':' + std::to_string(e.index) + ':' + std::to_string(e.destination) +
                        ':' + std::to_string(e.override) + ':' + std::to_string(e.overrideDestination) + ';';
    }
    fprintf(out,
            "ootHintCount=%d\n"
            "ootHintDigest=%08X\n"
            "ootPriceCount=%d\n"
            "ootPriceDigest=%08X\n"
            "ootStartingItems=%zu/%08X\n"
            "entranceCount=%d\n"
            "entranceDigest=%08X\n",
            hintCount, SohUtils::Hash(hintBlob), priceCount, SohUtils::Hash(priceBlob), StartingInventory.size(),
            SohUtils::Hash(startBlob), entranceCount, SohUtils::Hash(entranceBlob));
}

#ifdef RSBS_SINGLE_EXECUTABLE
/**
 * TEST BRIDGE (#951, the StaticHintOrder row): every ENABLED static hint of the live
 * world (staticHintInfoMap's keys, in RandomizerHint order), printed one per line
 * with its English text, and folded "<key>=<message>|<message>" through SohUtils::Hash.
 * Returns the digest; `*outCount` is the number of enabled static hints.
 */
extern "C" uint32_t Rando_TestStaticHintTexts(int* outCount) {
    auto ctx = Rando::Context::GetInstance();
    std::vector<RandomizerHint> keys;
    for (const auto& entry : Rando::StaticData::staticHintInfoMap) {
        keys.push_back(entry.first);
    }
    std::sort(keys.begin(), keys.end());
    std::string blob;
    int count = 0;
    for (const RandomizerHint key : keys) {
        Rando::Hint* hint = ctx ? ctx->GetHint(key) : nullptr;
        if (hint == nullptr || !hint->IsEnabled()) {
            continue;
        }
        count++;
        std::string line = std::to_string(static_cast<int>(key)) + '=';
        for (size_t m = 0; m < hint->GetNumberOfMessages(); m++) {
            line += (m == 0 ? "" : "|") + hint->GetHintMessage(MF_CLEAN, m).GetEnglish(MF_CLEAN);
        }
        printf("[static-hints] %s\n", line.c_str());
        blob += line + '\n';
    }
    if (outCount != nullptr) {
        *outCount = count;
    }
    // A static hint draws for an area only when its location spans more than one.
    int multiArea = 0;
    for (int i = 0; ctx && i < RC_MAX; i++) {
        if (ctx->GetItemLocation(static_cast<RandomizerCheck>(i))->GetAreas().size() > 1) {
            multiArea++;
        }
    }
    printf("[static-hints] %d location(s) span more than one area\n", multiArea);
    return SohUtils::Hash(blob);
}
#endif

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
// with the exclude and trick sets a golden profile names in the environment
// (#982; see ReadGoldenProfileSets), EMPTY when it names none, which is every
// profile before #982. The player's own path (the Generate button's worker
// thread, then the creation event) is the GoldenSeedDigestPlayerPath row's, which
// writes its OoT half with Rando_HeadlessWriteDeterminismHead below.
extern "C" int Rando_HeadlessWriteDeterminismHead(const char* outPath);

extern "C" int Rando_HeadlessSeedDeterminismDigest(const char* seedStr, const char* outPath) {
    int rc = Rando_HeadlessSeedTest(seedStr);
    if (rc != 0) {
        fprintf(stderr, "[rando-determinism] generation failed rc=%d\n", rc);
        return rc;
    }
    return Rando_HeadlessWriteDeterminismHead(outPath);
}

/**
 * The OoT half of the determinism digest, over the generation that just ran: the
 * unified-seed fields, the combo record and OoT's placement at the Generate
 * button, and (for a world that did not stop at its general pass) its content.
 * Split out of Rando_HeadlessSeedDeterminismDigest (#982) so the player-path row
 * writes the same lines after the worker thread's generation. Creates (truncates)
 * @p outPath; NULL/empty writes to stdout.
 */
extern "C" int Rando_HeadlessWriteDeterminismHead(const char* outPath) {
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

    // Canonical placement blob (Rando_HeadlessPlacementHash above). Since ADR 0010
    // increment 3 (lane K11) a PAIRED generation stops at its general pass, so
    // this hash describes OoT's world at the Generate button — the restricted
    // passes and the fixed placements — and the MM half's digest appends OoT's
    // FINAL world (ootFinalPlacementHash) once the single-bag fill has run.
    size_t placedCount = 0;
    const uint32_t placementHash = Rando_HeadlessPlacementHash(&placedCount);
#ifdef RSBS_SINGLE_EXECUTABLE
    const int generalPassDeferred = OoT_ComboLogic_GeneralPassDeferred();
#else
    const int generalPassDeferred = 0;
#endif

    // #982: a golden profile's sets must have reached the generation, counted off
    // the context the fill read rather than the sets the harness passed, so a
    // profile that silently lost its keys between the parse and the fill writes no
    // digest. Counted, and written below, only when the profile named a set.
    int goldenExcluded = 0;
    int goldenTricks = 0;
    if (sGoldenExcludesRequested >= 0) {
        // Each requested key, not the context's totals: FinalizeSettings excludes
        // some checks itself (a starting sword's or ocarina's own check).
        for (const RandomizerCheck rc : sGoldenExcludes) {
            if (ctx->GetLocationOption(rc).Is(RO_LOCATION_EXCLUDE)) {
                goldenExcluded++;
            }
        }
        for (const RandomizerTrick rt : sGoldenTricks) {
            if (ctx->GetTrickOption(rt).Is(RO_GENERIC_ON)) {
                goldenTricks++;
            }
        }
        if (goldenExcluded != sGoldenExcludesRequested || goldenTricks != sGoldenTricksRequested) {
            fprintf(stderr,
                    "[rando-determinism] FAIL: the golden profile asked for %d exclude(s) and %d trick(s), the "
                    "generation's context holds %d and %d\n",
                    sGoldenExcludesRequested, sGoldenTricksRequested, goldenExcluded, goldenTricks);
            return 6;
        }
    }
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
            // ADR 0010 increment 3 (lane K11): 1 when this generation stopped at its
            // general pass for the single-bag fill (every paired world); the
            // reverse overlay table that used to follow here is retired with the
            // pass that wrote it.
            "generalPassDeferred=%d\n"
            // ADR 0009 decision 1's consequence, finally payable: fold the
            // combo-level rules into the determinism digest, or "changing a
            // combo setting changes the derived world" stays unassertable —
            // which is exactly what #498 says about this digest today. The
            // RECORD is emitted beside the fingerprint deliberately: a
            // digest-only line would tell a diff that something changed without
            // telling a human what.
            "comboSettingsHash=%08X\n"
            "comboSettings=v%u/d%u/p%u,%u/c%04X,%04X/g%u/r%u\n",
            gComboCtx.sharedRandoSeed, gComboCtx.sharedRandoSettingsHash, gComboCtx.sourceIsRando ? 1 : 0,
            placementHash, placedCount, generalPassDeferred, gComboCtx.comboSettingsHash,
            (unsigned)gComboCtx.comboSettings.formatVersion, (unsigned)gComboCtx.comboSettings.direction,
            (unsigned)gComboCtx.comboSettings.poolSizeOoT, (unsigned)gComboCtx.comboSettings.poolSizeMM,
            (unsigned)gComboCtx.comboSettings.itemClassOoT, (unsigned)gComboCtx.comboSettings.itemClassMM,
            (unsigned)gComboCtx.comboSettings.goal, (unsigned)gComboCtx.comboSettings.logicRung);
    // #949: a world that did NOT stop at its general pass is complete here, so its
    // hints, prices, starting items and entrances are folded here; a paired world's
    // are folded by the MM half's writer once OoT's remainder has run.
    if (sGoldenExcludesRequested >= 0) {
        fprintf(out, "ootExcludedLocations=%d\nootEnabledTricks=%d\n", goldenExcluded, goldenTricks);
    }
    if (!generalPassDeferred) {
        Rando_HeadlessWriteOoTContentDigest(out);
    }
    if (closeOut) {
        fclose(out);
    }
    fprintf(stderr,
            "[rando-determinism] digest: seed=%08X settingsHash=%08X placementHash=%08X placed=%zu "
            "generalPassDeferred=%d\n",
            gComboCtx.sharedRandoSeed, gComboCtx.sharedRandoSettingsHash, placementHash, placedCount,
            generalPassDeferred);
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