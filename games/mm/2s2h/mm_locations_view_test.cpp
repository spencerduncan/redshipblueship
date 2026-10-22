/**
 * @file mm_locations_view_test.cpp
 * @brief ROM-free lock for Randomizer > MM Locations' model (#1101 lane R3):
 *        MM's excluded-check list (`gRando.ExcludedChecks`) as a page of the one
 *        randomizer menu. CTest row MMLocations (label `redship`), dispatch
 *        `mm-locations` in src/common/test_runner.cpp.
 *
 * MM-side for the reason mm_trick_table_test.cpp is: this is the one place
 * where both tables are in scope, MM's `Rando::StaticData::Checks` and the flat
 * descriptor table the page draws (src/common/combo_mm_locations_view.h).
 *
 *   leg 1  THE TABLE MIRRORS MM'S CHECKS. One row per check MM's pool can hold
 *          (every static check but RC_UNKNOWN and the two Stone Tower pots pool
 *          generation never adds), each id once, named as MM's check tracker
 *          names it, under a non-empty area, in area order then id order, and
 *          found again by id.
 *   leg 2  THE ONE SPELLING. The writers emit MM's own form (ascending ids,
 *          each followed by a comma), whatever order the edits came in; a
 *          duplicate or an id the table does not hold changes nothing; taking
 *          the last id out clears the key; a hand-spelled list reads back as
 *          its set.
 *   leg 3  FROZEN REFUSES. With the profile stamped, adding, removing and
 *          clearing are refused and the key is left exactly as it was.
 *   leg 4  VANILLA REFUSES. Under Vanilla logic MM's pool generation never
 *          reads the list, so the writers refuse an edit there (ADR 0004
 *          section 5); clearing stays allowed.
 *   leg 5  THE DIGEST FOLLOWS AN EDIT. One check excluded through the writer
 *          moves MM_Rando_ComputeProfileStamp, taking it out moves it back, and
 *          two spellings of one set (a hand edit's "B,A" and the writers' "A,B,")
 *          give one digest: the identity is the set, so a load can restore it.
 *   leg 6  THE NOTE. Unpaired, the page says the sentence every randomizer page
 *          of ours shares; frozen, the shared frozen sentence; under Vanilla
 *          logic, why the table is disabled.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>

#include "Rando/Rando.h"
#include "Rando/Types.h"
#include "Rando/StaticData/StaticData.h"

// src/common, outside any extern "C" block: these headers manage their own
// linkage (matching mm_trick_table_test.cpp).
#include "combo_mm_locations_view.h"
#include "combo_mm_options_page.h"
#include "combo_mm_options_view.h"
#include "foreign_items.h"

namespace {

int Fail(int code, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[MM-LOCATIONS] FAIL(%d): ", code);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    fflush(stderr);
    return code;
}

std::string Key() {
    const char* raw = CVarGetString(COMBO_MM_EXCLUDED_CHECKS_CVAR, "");
    return raw != nullptr ? raw : "";
}

bool KeyUnset() {
    const char* raw = CVarGetString(COMBO_MM_EXCLUDED_CHECKS_CVAR, "<unset>");
    return raw != nullptr && std::strcmp(raw, "<unset>") == 0;
}

/** The rule leg 1 holds the table to, spelled here independently of the table. */
bool PoolCanHold(const Rando::StaticData::RandoStaticCheck& check) {
    return check.randoCheckId != RC_UNKNOWN && check.sceneId != SCENE_LAST_BS;
}

int LegTable() {
    std::map<uint16_t, const Rando::StaticData::RandoStaticCheck*> expected;
    for (auto& [id, check] : Rando::StaticData::Checks) {
        if (PoolCanHold(check)) {
            expected[(uint16_t)id] = &check;
        }
    }
    const int count = Combo_MMLocationCount();
    printf("[TEST] leg 1 OBSERVED: %d rows, %zu checks MM's pool can hold\n", count, expected.size());
    if (count == 0) {
        return Fail(1, "no MM check table is registered: Randomizer > MM Locations has nothing to list, and nothing "
                       "authors gRando.ExcludedChecks");
    }
    if (count != (int)expected.size()) {
        return Fail(2, "the table holds %d rows, MM's pool can hold %zu checks", count, expected.size());
    }
    std::vector<bool> seen(65536, false);
    const ComboMMLocationDesc* prev = nullptr;
    for (int i = 0; i < count; i++) {
        const ComboMMLocationDesc* d = Combo_MMLocationAt(i);
        if (d == nullptr || d->name == nullptr || d->areaName == nullptr) {
            return Fail(3, "row %d is NULL or has a NULL name or area", i);
        }
        if (seen[d->id]) {
            return Fail(4, "check %u is listed twice", (unsigned)d->id);
        }
        seen[d->id] = true;
        auto it = expected.find(d->id);
        if (it == expected.end()) {
            return Fail(5, "row %d names check %u, which MM's pool cannot hold", i, (unsigned)d->id);
        }
        if (d->name[0] == '\0' || Rando::StaticData::CheckNames[d->id] != d->name) {
            return Fail(6, "check %u is named '%s', MM's check tracker names it '%s'", (unsigned)d->id, d->name,
                        Rando::StaticData::CheckNames[d->id].c_str());
        }
        if (d->areaName[0] == '\0') {
            return Fail(7, "check %u ('%s') has no area", (unsigned)d->id, d->name);
        }
        if (prev != nullptr && (d->areaKey < prev->areaKey || (d->areaKey == prev->areaKey && d->id < prev->id))) {
            return Fail(8, "row %d (%u, area %04X) comes after (%u, area %04X): not in area order, then id order", i,
                        (unsigned)d->id, (unsigned)d->areaKey, (unsigned)prev->id, (unsigned)prev->areaKey);
        }
        if (prev != nullptr && d->areaKey == prev->areaKey && std::strcmp(d->areaName, prev->areaName) != 0) {
            return Fail(9, "one area key %04X carries two names ('%s', '%s')", (unsigned)d->areaKey, prev->areaName,
                        d->areaName);
        }
        if (Combo_MMLocationById(d->id) != d) {
            return Fail(10, "Combo_MMLocationById(%u) does not find its own row", (unsigned)d->id);
        }
        prev = d;
    }
    if (Combo_MMLocationById((uint16_t)RC_UNKNOWN) != nullptr) {
        return Fail(11, "RC_UNKNOWN is listed");
    }
    printf("[TEST] leg 1: %d checks, each once, named and placed as MM's check tracker lists them\n", count);
    return 0;
}

/** Two checks in different areas, A < B by id. */
bool PickTwo(uint16_t* a, uint16_t* b) {
    const int count = Combo_MMLocationCount();
    if (count < 2) {
        return false;
    }
    const ComboMMLocationDesc* first = Combo_MMLocationAt(0);
    const ComboMMLocationDesc* last = Combo_MMLocationAt(count - 1);
    if (first == nullptr || last == nullptr || first->id == last->id) {
        return false;
    }
    *a = std::min(first->id, last->id);
    *b = std::max(first->id, last->id);
    return true;
}

std::string Spell(std::initializer_list<uint16_t> ids) {
    std::string s;
    for (uint16_t id : ids) {
        s += std::to_string(id) + ",";
    }
    return s;
}

int LegSpelling(uint16_t a, uint16_t b) {
    CVarClear(COMBO_MM_EXCLUDED_CHECKS_CVAR);
    const bool okB = Combo_MMLocationSetExcluded(b, true);
    const bool okA = Combo_MMLocationSetExcluded(a, true);
    printf("[TEST] leg 2 OBSERVED: after adding %u then %u the key reads '%s'\n", (unsigned)b, (unsigned)a,
           Key().c_str());
    if (!okA || !okB || Key() != Spell({ a, b })) {
        return Fail(20, "adding %u then %u wrote '%s', expected '%s' (MM's own spelling)", (unsigned)b, (unsigned)a,
                    Key().c_str(), Spell({ a, b }).c_str());
    }
    if (!Combo_MMLocationIsExcluded(a) || !Combo_MMLocationIsExcluded(b)) {
        return Fail(21, "an id the key holds does not read as excluded");
    }
    uint16_t ids[4] = {};
    if (Combo_MMLocationsExcluded(ids, 4) != 2 || ids[0] != a || ids[1] != b) {
        return Fail(22, "Combo_MMLocationsExcluded does not read the key back as [%u, %u]", (unsigned)a, (unsigned)b);
    }
    const uint16_t batch[] = { b, a, a, 0xFFFF };
    if (!Combo_MMLocationsSetExcluded(batch, 4, true) || Key() != Spell({ a, b })) {
        return Fail(23, "a batch repeating both ids plus an id the table does not hold changed the key to '%s'",
                    Key().c_str());
    }
    if (!Combo_MMLocationSetExcluded(a, false) || Key() != Spell({ b })) {
        return Fail(24, "taking %u out left '%s', expected '%s'", (unsigned)a, Key().c_str(), Spell({ b }).c_str());
    }
    if (!Combo_MMLocationSetExcluded(b, false) || !KeyUnset()) {
        return Fail(25, "taking the last id out left the key set ('%s'); an empty list clears it", Key().c_str());
    }
    const std::string hand = std::to_string(b) + "," + std::to_string(a);
    CVarSetString(COMBO_MM_EXCLUDED_CHECKS_CVAR, hand.c_str());
    if (Combo_MMLocationsExcluded(ids, 4) != 2 || ids[0] != a || ids[1] != b) {
        return Fail(26, "the hand-spelled list '%s' does not read back as its set", hand.c_str());
    }
    CVarClear(COMBO_MM_EXCLUDED_CHECKS_CVAR);
    printf("[TEST] leg 2: the writers spell the list as MM's menu did, in any edit order\n");
    return 0;
}

int LegFrozen(uint16_t a, uint16_t b) {
    CVarClear(COMBO_MM_EXCLUDED_CHECKS_CVAR);
    if (!Combo_MMLocationSetExcluded(a, true)) {
        return Fail(30, "leg 3 setup: an unfrozen write was refused");
    }
    const std::string before = Key();
    gComboCtx.mmProfileDigest = 0x4D4D1101u;
    const bool frozenPredicate = Combo_MMProfileFrozen();
    const bool added = Combo_MMLocationSetExcluded(b, true);
    const bool removed = Combo_MMLocationSetExcluded(a, false);
    const bool cleared = Combo_MMLocationsClearExcluded();
    const std::string after = Key();
    gComboCtx.mmProfileDigest = 0;
    printf("[TEST] leg 3 OBSERVED: frozen=%d add=%d remove=%d clear=%d key '%s' -> '%s'\n", frozenPredicate ? 1 : 0,
           added ? 1 : 0, removed ? 1 : 0, cleared ? 1 : 0, before.c_str(), after.c_str());
    if (!frozenPredicate) {
        return Fail(31, "leg 3 setup: a nonzero mmProfileDigest does not read as frozen");
    }
    if (added || removed || cleared || after != before) {
        return Fail(32,
                    "a writer accepted an edit while the profile is frozen (key '%s' -> '%s'): the paired world's "
                    "identity moved after its creation",
                    before.c_str(), after.c_str());
    }
    CVarClear(COMBO_MM_EXCLUDED_CHECKS_CVAR);
    printf("[TEST] leg 3: every writer refuses while the profile is frozen\n");
    return 0;
}

int LegVanilla(uint16_t a) {
    const char* logic = Rando::StaticData::Options[RO_LOGIC].cvar;
    CVarClear(COMBO_MM_EXCLUDED_CHECKS_CVAR);
    if (Combo_MMLocationsBlockedByLogic()) {
        return Fail(40, "leg 4 setup: the list reads as blocked with no logic chosen");
    }
    CVarSetInteger(logic, RO_LOGIC_VANILLA);
    const bool blocked = Combo_MMLocationsBlockedByLogic();
    const bool added = Combo_MMLocationSetExcluded(a, true);
    const std::string afterAdd = Key();
    CVarSetString(COMBO_MM_EXCLUDED_CHECKS_CVAR, Spell({ a }).c_str());
    const bool cleared = Combo_MMLocationsClearExcluded();
    const bool clearedKey = KeyUnset();
    CVarClear(logic);
    CVarClear(COMBO_MM_EXCLUDED_CHECKS_CVAR);
    printf("[TEST] leg 4 OBSERVED: vanilla blocked=%d add=%d key-after-add '%s' clear=%d cleared=%d\n", blocked ? 1 : 0,
           added ? 1 : 0, afterAdd.c_str(), cleared ? 1 : 0, clearedKey ? 1 : 0);
    if (!blocked || added || !afterAdd.empty()) {
        return Fail(41,
                    "under Vanilla logic the list is not blocked, or an edit reached the key ('%s'): a control "
                    "that moves the identity and not the world",
                    afterAdd.c_str());
    }
    if (!cleared || !clearedKey) {
        return Fail(42, "under Vanilla logic clearing the list was refused");
    }
    printf("[TEST] leg 4: under Vanilla logic an edit is refused and clearing is allowed\n");
    return 0;
}

int LegDigest(uint16_t a, uint16_t b) {
    CVarClear(COMBO_MM_EXCLUDED_CHECKS_CVAR);
    const uint32_t none = MM_Rando_ComputeProfileStamp();
    Combo_MMLocationSetExcluded(a, true);
    const uint32_t one = MM_Rando_ComputeProfileStamp();
    Combo_MMLocationSetExcluded(a, false);
    const uint32_t back = MM_Rando_ComputeProfileStamp();
    Combo_MMLocationSetExcluded(b, true);
    Combo_MMLocationSetExcluded(a, true);
    const uint32_t written = MM_Rando_ComputeProfileStamp();
    const std::string hand = std::to_string(b) + "," + std::to_string(a);
    CVarSetString(COMBO_MM_EXCLUDED_CHECKS_CVAR, hand.c_str());
    const uint32_t handSpelled = MM_Rando_ComputeProfileStamp();
    CVarClear(COMBO_MM_EXCLUDED_CHECKS_CVAR);
    printf("[TEST] leg 5 OBSERVED: none %08X, one %08X, back %08X, writers' '%s' %08X, hand '%s' %08X\n",
           (unsigned)none, (unsigned)one, (unsigned)back, Spell({ a, b }).c_str(), (unsigned)written, hand.c_str(),
           (unsigned)handSpelled);
    if (one == none) {
        return Fail(50, "excluding a check through the page's writer did not move the profile digest");
    }
    if (back != none) {
        return Fail(51, "taking the check back out did not return the digest to its value");
    }
    if (handSpelled != written) {
        return Fail(52,
                    "two spellings of one excluded set give two digests (%08X, %08X): the identity is the "
                    "spelling, so a load cannot put a file's list back",
                    (unsigned)written, (unsigned)handSpelled);
    }
    printf("[TEST] leg 5: the digest follows the excluded set, not its spelling\n");
    return 0;
}

int LegNote(uint16_t a) {
    CVarClear(COMBO_MM_EXCLUDED_CHECKS_CVAR);
    const std::string unpaired = Combo_MMLocationsPage_StatusNote();
    gComboCtx.mmProfileDigest = 0x4D4D1101u;
    const std::string frozen = Combo_MMLocationsPage_StatusNote();
    gComboCtx.mmProfileDigest = 0;
    CVarSetInteger(Rando::StaticData::Options[RO_LOGIC].cvar, RO_LOGIC_VANILLA);
    const std::string vanilla = Combo_MMLocationsPage_StatusNote();
    CVarClear(Rando::StaticData::Options[RO_LOGIC].cvar);
    (void)a;
    printf("[TEST] leg 6 OBSERVED: unpaired '%s' | frozen '%s' | vanilla '%s'\n", unpaired.c_str(), frozen.c_str(),
           vanilla.c_str());
    if (unpaired != COMBO_RANDO_PAGES_NEXT_WORLD_NOTE) {
        return Fail(60, "the unpaired note is not the sentence every randomizer page of ours shares");
    }
    if (frozen.rfind("Already decided when this world was created", 0) != 0) {
        return Fail(61, "the frozen note is not the shared frozen sentence");
    }
    if (vanilla.find("Vanilla") == std::string::npos || vanilla == unpaired) {
        return Fail(62, "under Vanilla logic the note does not say why the table is disabled");
    }
    printf("[TEST] leg 6: the note says each state\n");
    return 0;
}

} // namespace

extern "C" int MM_LocationsView_RunHeadless(void) {
    printf("[TEST] mm-locations: Randomizer > MM Locations lists MM's checks and writes gRando.ExcludedChecks only "
           "through gated writers whose edits the profile digest follows (#1101)\n");
    gComboCtx.mmProfileDigest = 0;
    CVarClear(Rando::StaticData::Options[RO_LOGIC].cvar);
    const std::string saved = Key();
    const bool savedUnset = KeyUnset();

    // The real registrar, the one Combo_MMOptionsPages_Init runs.
    Combo_MMOptionsPages_Init();
    MM_RandoLocationsUi_Register();

    int rc = LegTable();
    uint16_t a = 0;
    uint16_t b = 0;
    if (rc == 0 && !PickTwo(&a, &b)) {
        rc = Fail(12, "the table holds fewer than two distinct checks");
    }
    if (rc == 0) {
        // Every leg runs even after one fails, so one run reports each leg.
        int (*const legs[])(uint16_t, uint16_t) = {
            LegSpelling,
            LegFrozen,
            [](uint16_t x, uint16_t) { return LegVanilla(x); },
            LegDigest,
            [](uint16_t x, uint16_t) { return LegNote(x); },
        };
        for (auto leg : legs) {
            const int legRc = leg(a, b);
            if (rc == 0) {
                rc = legRc;
            }
        }
    }

    gComboCtx.mmProfileDigest = 0;
    CVarClear(Rando::StaticData::Options[RO_LOGIC].cvar);
    if (savedUnset) {
        CVarClear(COMBO_MM_EXCLUDED_CHECKS_CVAR);
    } else {
        CVarSetString(COMBO_MM_EXCLUDED_CHECKS_CVAR, saved.c_str());
    }
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] PASS: MM Locations' table, writers and digest term agree with MM's own check filter\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
