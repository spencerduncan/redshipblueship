/**
 * @file test_mm_get_item_presentation.c
 * @brief MmGetItemPresentation: SoH's "Skip Get Item Animations" decides
 *        Majora's Mask's get-item presentation too (#1060).
 *
 * ROM-free, redship tier. MM decides whether a randomizer check plays the full
 * get-item presentation (Link holds the item overhead, textbox) from one tier
 * (Items.cpp ShouldShowGetItemCutscene; CheckQueue.cpp's trap-text colour strip;
 * SkipGiantsChamber.cpp and the story-cutscene gives). Upstream that tier is
 * gEnhancements.Cutscenes.SkipGetItemCutscenes, whose only widget and preset are
 * not in the single exe, so it sat at 0 (never skip) while the combo's control,
 * SoH's three-tier gRandoEnhancements.TimeSavers.SkipGetItemAnimation, reached
 * OoT only. Every MM reader now goes through Rando::StaticData::
 * SkipGetItemCutsceneTier(), which translates SoH's value:
 *
 *   SoH Off (0)        -> MM 0 "Never"           every check presents
 *   SoH Junk Items (1) -> MM 1 "Junk Items Only" junk (and repeats) skip
 *   SoH All Items (2)  -> MM 3 "Always"          nothing presents
 *   unset              -> SoH's default, Junk Items
 *   anything else      -> MM 0, as OoT's reader shows on any other value
 *
 * The CVarClassification row's translated-key block locks that every MM reader
 * calls the helper (no linked MM file reads MM's own key).
 *
 *   1. the pure map: each SoH value lands on the named MM tier;
 *   2. through the CVar store: with MM's own key holding a stale value (the
 *      #1060 workaround), each SoH value gives the mapped tier and the
 *      presentation per item class, junk, a re-looted check (which converts to
 *      junk), health, lesser (a trap) and major;
 *   3. the two read sites that consume the tier directly: the trap text loses
 *      its colour codes only when health and lesser skip too (tier >= 2), and
 *      the non-rando story gives present only at tier 0.
 *
 * Linkage note: #included into test_runner.cpp at file scope (compiled as C++).
 */

#include "../test_runner.h"

#include <cstdio>

extern "C" {
// games/mm/2s2h/Rando/StaticData/Items.cpp (#1060)
int MM_Rando_GetItemPresentation_TestTierFromSoh(int sohSkipGetItemAnimation);
int MM_Rando_GetItemPresentation_TestTier(void);
int MM_Rando_GetItemPresentation_TestShows(int sample);
}

namespace {

constexpr const char* kGipSohKey = "gRandoEnhancements.TimeSavers.SkipGetItemAnimation";
constexpr const char* kGipMmKey = "gEnhancements.Cutscenes.SkipGetItemCutscenes";

constexpr int kGipSampleCount = 6;
const char* const kGipSampleNames[kGipSampleCount] = {
    "green rupee (junk)", "RI_JUNK", "heart piece (health)", "trap (lesser)", "bow (major)", "re-looted heart piece",
};

struct GipCase {
    const char* name;
    int sohValue; // -1: unset
    int mmTier;
    int shows[kGipSampleCount];
};

const GipCase kGipCases[] = {
    { "unset (SoH default Junk Items)", -1, 1, { 0, 0, 1, 1, 1, 0 } },
    { "Off", 0, 0, { 1, 1, 1, 1, 1, 1 } },
    { "Junk Items", 1, 1, { 0, 0, 1, 1, 1, 0 } },
    { "All Items", 2, 3, { 0, 0, 0, 0, 0, 0 } },
    { "out of range (7)", 7, 0, { 1, 1, 1, 1, 1, 1 } },
};

} // namespace

// The row's body. Test_MmGetItemPresentation (test_runner.cpp) brings up the
// display-free Ship::Context first: the CVars live on it.
static TestResult MmGetItemPresentation_Run(void) {
    printf("[TEST] mm-get-item-presentation: SoH's Skip Get Item Animations decides MM's get-item presentation "
           "(#1060)\n");
    bool ok = true;

    // (1) The pure map.
    struct {
        int soh;
        int mm;
    } const kPure[] = { { 0, 0 }, { 1, 1 }, { 2, 3 }, { -1, 0 }, { 3, 0 }, { 7, 0 } };
    for (const auto& row : kPure) {
        const int got = MM_Rando_GetItemPresentation_TestTierFromSoh(row.soh);
        if (got != row.mm) {
            printf("[TEST] FAIL: SoH Skip Get Item Animations %d maps to MM tier %d, expected %d\n", row.soh, got,
                   row.mm);
            ok = false;
        }
    }

    // (2) Through the CVar store, with MM's own key holding a stale value that
    // must not reach the decision (one control for one game).
    CVarSetInteger(kGipMmKey, 2);
    for (const GipCase& c : kGipCases) {
        if (c.sohValue < 0) {
            CVarClear(kGipSohKey);
        } else {
            CVarSetInteger(kGipSohKey, c.sohValue);
        }
        const int tier = MM_Rando_GetItemPresentation_TestTier();
        if (tier != c.mmTier) {
            printf("[TEST] FAIL: SoH %s: MM's readers see tier %d, expected %d\n", c.name, tier, c.mmTier);
            ok = false;
        }
        for (int s = 0; s < kGipSampleCount; s++) {
            const int shows = MM_Rando_GetItemPresentation_TestShows(s);
            if (shows < 0) {
                printf("[TEST] FAIL: sample %s: fixture error %d\n", kGipSampleNames[s], shows);
                ok = false;
                continue;
            }
            if (shows != c.shows[s]) {
                printf("[TEST] FAIL: SoH %s: %s %s the get-item presentation, expected it %s\n", c.name,
                       kGipSampleNames[s], shows ? "plays" : "skips", c.shows[s] ? "to play" : "to skip");
                ok = false;
            }
        }

        // (3) The read sites that consume the tier directly.
        const bool stripsTrapColours = tier >= 2;     // CheckQueue.cpp
        const bool storyGivePresents = tier == 0;     // SkipGiantsChamber.cpp and the story gives
        const bool wantStrips = c.sohValue == 2;      // health and lesser skip only at All Items
        const bool wantStoryPresents = c.mmTier == 0; // Off, or a value OoT treats as Off
        if (stripsTrapColours != wantStrips || storyGivePresents != wantStoryPresents) {
            printf("[TEST] FAIL: SoH %s: trap colour strip %d (want %d), story give presents %d (want %d)\n", c.name,
                   stripsTrapColours, wantStrips, storyGivePresents, wantStoryPresents);
            ok = false;
        }
    }
    CVarClear(kGipSohKey);
    CVarClear(kGipMmKey);

    if (!ok) {
        return TEST_FAIL;
    }
    printf("[TEST] PASS: mm-get-item-presentation\n");
    return TEST_PASS;
}
