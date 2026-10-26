/**
 * @file test_combo_tracker_window.c
 * @brief ROM-free lock for the combo tracker WINDOW (#458; ADR 0008).
 *
 * The model has its own lock (test_combo_tracker_view.c). This one covers the
 * window that renders it, in the shape of test_combo_spoiler_window.c:
 *
 * 1. REGISTRATION + IDEMPOTENCE + NAME DE-COLLISION. The window lands on a
 *    bare Ship::Gui under kComboTrackerWindowName, a second registration is a
 *    no-op, and stand-ins already holding SoH/MM tracker names and the other
 *    common-owned window names are undisturbed. Gui::AddGuiWindow rejects
 *    duplicates SILENTLY from the caller's side, so a collision would present
 *    only as a window that never appears.
 *
 * 2. GAME-AGNOSTICISM — the hard tripwire. ADR 0008's claim is that a
 *    common-owned window reads gComboCtx and the adapter surfaces, never
 *    gSaveContext through either game's layout, which is what lets it draw
 *    under GAME_OOT, GAME_MM and GAME_NONE alike. This harness has NO ImGui
 *    context, so any Draw() that reaches ImGui::Begin aborts the process.
 *    Draw()/Update() run under all three GameIds with the visibility CVar
 *    CLEARED (proving the live-CVar early-out holds).
 *
 * 3. VISIBILITY IS THE ONLY THING GATING DRAW. With the CVar SET, Draw()
 *    must reach ImGui — which would abort here — so that case is
 *    deliberately NOT driven; clearing the CVar being sufficient is what is
 *    asserted.
 *
 *    That early-out is also what would make the state/game loop VACUOUS if it
 *    stopped at Draw(): a shut window never touches the model, so the loop
 *    alone proves nothing about the four world states it sets up. So each
 *    iteration additionally drives the exact model reads DrawElement performs
 *    (TrackerDriveModelReads below) — identity, both panels' summary and check
 *    walk, and both directions' crossings, which DrawCrossingList in the same
 *    file reads for the Cross-Game Spoiler since ruling D10 cut the Crossings
 *    section from this window (#1128) — across paired/unpaired x adapters
 *    present/absent. The paired-with-absent-adapters corner is the one no
 *    other lock reaches: a crossing row must still resolve, with a NULL host
 *    check name and a non-NULL item name, when the host game's adapter is not
 *    registered at all.
 *
 * 4. DRAWN THE SoH WAY (UI parity M8), as far as a source scan can hold it:
 *    through the combo_ui seam, with SoH's close-button chrome in the Window
 *    type and none in the Floating type (#1130), SoH's chrome keys each read
 *    under its own name, no settings digest, and check statuses as FontAwesome
 *    glyphs rather than "[x]".
 *
 * 5. THE LAYOUT AND GROUPING DECISIONS, as the pure functions the draw code
 *    calls (#458 U4, #815, #816): a balanced wrap never breaks a word inside
 *    itself and the crossing table's Item column holds its widest word (under
 *    a fake font that wraps the way ImGui does, which reproduces #815's
 *    "Progressiv" / "e Slingshot"); a paired file's panels print no seed of
 *    their own; the Checks list groups by area in key order with SoH's area
 *    totals, and its search matches check, area and revealed item names;
 *    and (#1128) the rows and areas SoH's hide keys leave out under each pane's
 *    own Show Hidden Items, the keys' SoH defaults, and the totals line counting
 *    skipped checks as checked; and (#1129) SoH's reachability: the lock glyph
 *    a row draws (none when its availability is not known), each pane's own
 *    Only Show Available Checks (rows known unavailable and areas with none
 *    available go, whatever Show Hidden Items says; a row of unknown
 *    availability never goes), the "N Available / " totals prefix and the area
 *    header's available count, all behind Enable Available Checks; and (#1130)
 *    the window's chrome from SoH's Check Tracker keys: the Window type a
 *    titled pane with a close button whatever the floating gates say, the
 *    Floating type SoH's overlay flags (input-less unless Enable Dragging) with
 *    no close button, drawn only while paused under Only Enable While Paused
 *    and only while both combo buttons are held under Combo Button Hold, SoH's
 *    combo-button pad bits, and every key's SoH default.
 *
 * Appearance itself is judged from the UiSnapshot row's captures (Combo
 * Tracker @paired, @unpaired and @progress), not here.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as
 * C++) — it drives the C++-linkage ComboGui::RegisterComboTrackerWindow.
 * Entry point is a RunHeadless bridge (GuiWindow ctors read ConsoleVariables
 * off the Ship::Context singleton, so it needs the display-free shared
 * bring-up that lives in test_runner.cpp).
 */

#include "../ComboTrackerWindow.h"
#include "../combo_tracker_view.h"
#include "../combo_ui.h"
#include "../context.h"
#include "../crossing_store.h"
#include "../foreign_items.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <ship/window/gui/IconsFontAwesome4.h>
#include <libultraship/bridge/consolevariablebridge.h>

#define CTW_ASSERT(cond)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return TEST_FAIL;                                              \
        }                                                                  \
    } while (0)

namespace {

// Inert stand-in, matching mm_trackers_gui_test.cpp's: empty cvar (no
// ConsoleVariables traffic in the ctor) and no-op overrides.
class TrackerStandinWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
};

const char* const kTrackerNeighbourNames[] = {
    "Check Tracker",     // SoH-owned
    "MM Check Tracker",  // MM-owned
    "Cross-Game Spoiler" // the other common-owned window (ADR 0008)
};
constexpr int kTrackerNeighbourCount = 3;

/**
 * The model reads ComboTrackerWindow::DrawElement performs, with the ImGui
 * calls this harness cannot make removed. See point 3 in the header: without
 * this, the loop below only re-proves the visibility early-out.
 *
 * Returns false on a contract violation rather than crashing on one — the
 * crash-shaped failures (a missing adapter dereferenced, a row walked past its
 * table) are caught by the process, these are the quiet ones.
 */
bool TrackerDriveModelReads(void) {
    ComboTrackerIdentity identity;
    Combo_TrackerIdentity(&identity);

    const uint8_t games[2] = { (uint8_t)GAME_OOT, (uint8_t)GAME_MM };
    for (int g = 0; g < 2; g++) {
        ComboTrackerGameSummary summary;
        Combo_TrackerGameSummary(games[g], &summary);
        if (Combo_TrackerFreshnessLabel(games[g], summary.freshness) == NULL) {
            return false; // never NULL: it feeds a printf-family format
        }
        if (summary.freshness != COMBO_TRACKER_FRESH_UNAVAILABLE) {
            const int count = Combo_TrackerCheckCount(games[g]);
            for (int i = 0; i < count; i++) {
                ComboTrackerCheckRow row;
                if (!Combo_TrackerCheckAt(games[g], i, &row)) {
                    break; // the renderer's own stop condition
                }
            }
        } else if (Combo_TrackerCheckCount(games[g]) != 0) {
            return false; // "no data" must not hand the renderer rows to walk
        }

        const int crossings = Combo_TrackerForeignCount(games[g]);
        ComboTrackerForeignProgress progress;
        Combo_TrackerForeignProgress(games[g], &progress);
        if (progress.total != crossings || progress.found < 0 || progress.found > progress.total) {
            return false; // the note's counts and the table must agree
        }
        for (int i = 0; i < crossings; i++) {
            ComboTrackerForeignRow foreignRow;
            if (!Combo_TrackerForeignRowAt(games[g], i, &foreignRow)) {
                return false; // the count and the walk must agree
            }
            if (foreignRow.itemName == NULL || foreignRow.itemArticle == NULL) {
                return false; // contractually never NULL (placeholder / "" otherwise)
            }
            if (foreignRow.found > COMBO_TRACKER_FOUND_YES) {
                return false; // the cell prints one of three words
            }
        }
    }
    return true;
}

// ---- 5. The layout and grouping decisions (#458 U4, #815, #816) -------------
//
// A fake font that wraps the way ImGui's CalcWordWrapPositionA does: greedy
// word fill, and a word wider than the wrap width broken inside itself at a
// character. Capitals 10 px, everything else 7 px, a space 4 px, lines 10 px.

float CtwCharWidth(char ch) {
    if (ch == ' ') {
        return 4.0f;
    }
    return (ch >= 'A' && ch <= 'Z') ? 10.0f : 7.0f;
}

float CtwWidth(void*, const char* begin, const char* end) {
    float w = 0.0f;
    for (const char* p = begin; p < end; p++) {
        w += CtwCharWidth(*p);
    }
    return w;
}

int CtwLines(const char* text, float wrap) {
    int lines = 1;
    float lineW = 0.0f;
    const char* p = text;
    while (*p != '\0') {
        while (*p == ' ') {
            p++;
        }
        const char* wordEnd = p;
        while (*wordEnd != '\0' && *wordEnd != ' ') {
            wordEnd++;
        }
        if (p == wordEnd) {
            break;
        }
        const float word = CtwWidth(nullptr, p, wordEnd);
        const float lead = lineW > 0.0f ? CtwCharWidth(' ') : 0.0f;
        if (lineW + lead + word <= wrap) {
            lineW += lead + word;
        } else if (word <= wrap) {
            lines++;
            lineW = word;
        } else {
            // Wider than a whole line: ImGui starts it on a fresh line and
            // breaks it at the character that no longer fits.
            if (lineW > 0.0f) {
                lines++;
            }
            lineW = 0.0f;
            for (const char* c = p; c < wordEnd; c++) {
                if (lineW > 0.0f && lineW + CtwCharWidth(*c) > wrap) {
                    lines++;
                    lineW = 0.0f;
                }
                lineW += CtwCharWidth(*c);
            }
        }
        p = wordEnd;
    }
    return lines;
}

float CtwHeight(void*, const char* text, float wrap) {
    return 10.0f * (float)CtwLines(text, wrap);
}

const ComboGui::ComboTextMeasure kCtwFont = { CtwWidth, CtwHeight, nullptr };

// A synthetic OoT world for the grouping lock: two areas, keyed out of table
// order, one complete (a collected and a skipped check) and one not, plus an
// unshuffled row that must never be listed. The collected row has a short name,
// as an OoT row does ("KF Kokiri Sword Chest" / "Kokiri Sword Chest"), and hosts
// the other game's item, as a crossing host does.
struct CtwRow {
    uint16_t id;
    const char* name;
    const char* shortName; // or NULL
    uint16_t areaKey;
    const char* areaName;
    uint8_t status;
    const char* item; // revealed item, or NULL
    uint8_t itemGame;
    bool shuffled;
};
const CtwRow kCtwRows[] = {
    { 0x10, "A7 Tower Chest", "Tower Chest", 7, "Area Seven", COMBO_TRACKER_CHECK_COLLECTED, "Fairy Bow",
      (uint8_t)GAME_MM, true },
    { 0x11, "Pond Chest", nullptr, 3, "Area Three", COMBO_TRACKER_CHECK_UNCHECKED, nullptr, (uint8_t)GAME_NONE, true },
    { 0x12, "Roof Chest", nullptr, 7, "Area Seven", COMBO_TRACKER_CHECK_SKIPPED, nullptr, (uint8_t)GAME_NONE, true },
    { 0x13, "Well Chest", nullptr, 3, "Area Three", COMBO_TRACKER_CHECK_SEEN, nullptr, (uint8_t)GAME_NONE, true },
    { 0x14, "Hidden Chest", nullptr, 3, "Area Three", COMBO_TRACKER_CHECK_UNCHECKED, nullptr, (uint8_t)GAME_NONE,
      false },
};
constexpr int kCtwRowCount = (int)(sizeof(kCtwRows) / sizeof(kCtwRows[0]));

bool CtwSummary(ComboTrackerGameSummary* out) {
    out->hasWorld = true;
    out->totalChecks = kCtwRowCount;
    out->shuffled = 4;
    return true;
}
int CtwCount(void) {
    return kCtwRowCount;
}
bool CtwCheckAt(int index, ComboTrackerCheckRow* out) {
    if (index < 0 || index >= kCtwRowCount) {
        return false;
    }
    const CtwRow& r = kCtwRows[index];
    out->checkId = r.id;
    out->name = r.name;
    out->shortName = r.shortName;
    out->shuffled = r.shuffled;
    out->status = r.status;
    out->obtained = r.status == COMBO_TRACKER_CHECK_COLLECTED;
    out->skipped = r.status == COMBO_TRACKER_CHECK_SKIPPED;
    out->areaKey = r.areaKey;
    out->areaName = r.areaName;
    out->placedItemName = r.item;
    out->placedItemGame = r.itemGame;
    return true;
}
const char* CtwCheckName(uint16_t id) {
    for (const CtwRow& r : kCtwRows) {
        if (r.id == id) {
            return r.name;
        }
    }
    return nullptr;
}
const ComboOoTTrackerOps kCtwOps = { CtwSummary, CtwCount, CtwCheckAt, CtwCheckName };

/** Row ids of `area`, in order, as one string ("10,12"). */
std::string CtwIds(const ComboGui::ComboTrackerAreaRows& area) {
    std::string s;
    for (const ComboTrackerCheckRow& row : area.rows) {
        char id[8];
        snprintf(id, sizeof(id), "%s%X", s.empty() ? "" : ",", (unsigned)row.checkId);
        s += id;
    }
    return s;
}

int Lock815(void) {
    using namespace ComboGui;

    // #815: no word broken inside itself. "Progressive Slingshot" in a 120 px
    // cell takes two lines; a 77 px wrap also takes two ("Progressiv" /
    // "e Slingshot"), and a search for the narrowest two-line width lands there.
    const char* item = "Progressive Slingshot";
    const float widest = ComboWidestWordWidth(item, kCtwFont);
    const float wrap = ComboBalancedWrapWidth(item, 120.0f, kCtwFont);
    printf("[TEST] combo-tracker-window #815: widest word %.1f px, balanced wrap %.1f px in a 120 px cell\n", widest,
           wrap);
    CTW_ASSERT(widest == 80.0f); // "Progressive"
    CTW_ASSERT(wrap >= widest);  // never inside a word
    CTW_ASSERT(wrap <= 120.0f);
    CTW_ASSERT(CtwLines(item, wrap) == 2);
    // Balancing still happens where words allow it.
    const char* check = "Stone Tower Temple Entrance Small Crate";
    const float checkWrap = ComboBalancedWrapWidth(check, 230.0f, kCtwFont);
    CTW_ASSERT(checkWrap < 230.0f && CtwLines(check, checkWrap) == CtwLines(check, 230.0f));
    CTW_ASSERT(checkWrap >= ComboWidestWordWidth(check, kCtwFont));
    // Text that fits is drawn at the cell width.
    CTW_ASSERT(ComboBalancedWrapWidth("Bow", 120.0f, kCtwFont) == 120.0f);
    // The Item column holds the widest item word whole, within its bounds.
    const float itemCol = ComboCrossingItemColumnWidth(160.0f, widest);
    printf("[TEST] combo-tracker-window #815: Item column %.1f px of 160 px for an 80 px word\n", itemCol);
    CTW_ASSERT(itemCol >= widest);
    CTW_ASSERT(ComboCrossingItemColumnWidth(400.0f, widest) == 160.0f); // the 2/5 share when it fits
    CTW_ASSERT(ComboCrossingItemColumnWidth(100.0f, widest) <= 60.0f);  // never past 3/5
    CTW_ASSERT(ComboCrossingItemColumnWidth(100.0f, widest) >= 40.0f);
    return TEST_PASS;
}

int Lock816(void) {
    using namespace ComboGui;

    // #816: a paired file's panels print no seed of their own; an unpaired
    // world's panel does.
    ComboTrackerIdentity identity = {};
    ComboTrackerGameSummary summary = {};
    identity.paired = true;
    identity.sharedRandoSeed = 2852956488u;
    summary.seed = 2380209646u; // MM's final seed on that file (the #811 playtest)
    printf("[TEST] combo-tracker-window #816: paired file, own seed differs: shows own seed = %d\n",
           (int)ComboPanelShowsOwnSeed(identity, summary));
    CTW_ASSERT(!ComboPanelShowsOwnSeed(identity, summary));
    summary.seed = identity.sharedRandoSeed;
    CTW_ASSERT(!ComboPanelShowsOwnSeed(identity, summary));
    identity.paired = false;
    CTW_ASSERT(ComboPanelShowsOwnSeed(identity, summary));
    return TEST_PASS;
}

int LockAreasBody(void) {
    using namespace ComboGui;

    // #458 U4: area grouping, SoH's area totals, and search.
    std::vector<ComboTrackerAreaRows> areas;
    ComboCollectCheckAreas((uint8_t)GAME_OOT, "", areas);
    printf("[TEST] combo-tracker-window U4: %d areas\n", (int)areas.size());
    CTW_ASSERT(areas.size() == 2);
    CTW_ASSERT(areas[0].key == 3 && strcmp(areas[0].name, "Area Three") == 0); // ascending key, not table order
    CTW_ASSERT(areas[0].total == 2 && areas[0].done == 0 && CtwIds(areas[0]) == "11,13"); // unshuffled 14 never listed
    CTW_ASSERT(areas[1].key == 7 && areas[1].total == 2 && areas[1].done == 2 && CtwIds(areas[1]) == "10,12");
    // Search by a revealed item: one row, its area's totals unchanged.
    ComboCollectCheckAreas((uint8_t)GAME_OOT, "fairy bow", areas);
    CTW_ASSERT(areas.size() == 1 && areas[0].key == 7 && CtwIds(areas[0]) == "10" && areas[0].total == 2);
    // The row prints the other game's item with its game's suffix, and the
    // search matches that same string, as SoH's ShouldShowCheck matches
    // PlacedItemTrackerName ("... (MM)"): typing "MM" finds the crossing host.
    ComboTrackerCheckRow host;
    CTW_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, 0, &host));
    CTW_ASSERT(ComboCheckRowItemText((uint8_t)GAME_OOT, host) == "Fairy Bow (MM)");
    ComboCollectCheckAreas((uint8_t)GAME_OOT, "MM", areas);
    printf("[TEST] combo-tracker-window U4: search \"MM\" leaves %d areas (%s)\n", (int)areas.size(),
           areas.empty() ? "" : CtwIds(areas[0]).c_str());
    CTW_ASSERT(areas.size() == 1 && CtwIds(areas[0]) == "10");
    // Under its area header a row prints its short name when its game's tracker
    // has one (SoH's DrawLocation: GetShortName()), else its full name.
    printf("[TEST] combo-tracker-window U4: list name of \"%s\" is \"%s\"\n", host.name,
           ComboCheckListName(host) != nullptr ? ComboCheckListName(host) : "(null)");
    CTW_ASSERT(ComboCheckListName(host) != nullptr && strcmp(ComboCheckListName(host), "Tower Chest") == 0);
    ComboTrackerCheckRow plain;
    CTW_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, 1, &plain));
    CTW_ASSERT(ComboCheckListName(plain) != nullptr && strcmp(ComboCheckListName(plain), "Pond Chest") == 0);
    plain.name = nullptr;
    CTW_ASSERT(ComboCheckListName(plain) == nullptr);
    // By area name: the whole area.
    ComboCollectCheckAreas((uint8_t)GAME_OOT, "three", areas);
    CTW_ASSERT(areas.size() == 1 && CtwIds(areas[0]) == "11,13");
    // By check name, case-insensitively, and an exclusion term (ImGui's filter
    // tests terms in order, so the exclusion goes first, as in SoH's box).
    ComboCollectCheckAreas((uint8_t)GAME_OOT, "ROOF", areas);
    CTW_ASSERT(areas.size() == 1 && CtwIds(areas[0]) == "12");
    ComboCollectCheckAreas((uint8_t)GAME_OOT, "-tower,chest", areas);
    CTW_ASSERT(areas.size() == 2 && CtwIds(areas[0]) == "11,13" && CtwIds(areas[1]) == "12");
    ComboCollectCheckAreas((uint8_t)GAME_OOT, "nothing matches this", areas);
    CTW_ASSERT(areas.empty());
    // No data: no areas, not an empty-but-present list.
    Combo_Tracker_RegisterOoT(NULL);
    ComboCollectCheckAreas((uint8_t)GAME_OOT, "", areas);
    CTW_ASSERT(areas.empty());
    return TEST_PASS;
}

int LockAreas(void) {
    Combo_Tracker_RegisterOoT(&kCtwOps);
    const int result = LockAreasBody();
    OoT_TrackerAdapter_Register(); // the production adapter, whatever the result
    return result;
}

// ---- #1128 (lane L1a): SoH's hide keys, Show Hidden Items per pane, totals ----

/** "<area key>:<row ids>" per drawn area, in order ("3:11,13 7:10,12"); a filtered area reads "3:-". */
std::string CtwDrawn(const std::vector<ComboGui::ComboTrackerAreaRows>& areas) {
    std::string s;
    for (const ComboGui::ComboTrackerAreaRows& area : areas) {
        char key[8];
        snprintf(key, sizeof(key), "%s%u:", s.empty() ? "" : " ", (unsigned)area.key);
        s += key;
        s += area.filtered ? "-" : CtwIds(area);
    }
    return s;
}

// SoH's Check Tracker keys the pane reads (randomizer_check_tracker.cpp), in
// ComboTrackerCheckStatus order for the six "<State>.Hide" keys.
const char* const kCtwHideKeys[COMBO_TRACKER_CHECK_STATUS_COUNT] = {
    "gTrackers.CheckTracker.Unchecked.Hide", "gTrackers.CheckTracker.Seen.Hide",
    "gTrackers.CheckTracker.Scummed.Hide",   "gTrackers.CheckTracker.Skipped.Hide",
    "gTrackers.CheckTracker.Collected.Hide", "gTrackers.CheckTracker.Saved.Hide",
};
const char* const kCtwOtherKeys[] = {
    "gTrackers.CheckTracker.ShowHidden",
    "gTrackers.CheckTracker.AreaComplete.Hide",
    "gTrackers.CheckTracker.AreaIncomplete.Hide",
    "gTrackers.CheckTracker.HideFilteredAreas",
    "gTrackers.CheckTracker.HiddenItemsToggleVisible",
    "gTrackers.CheckTracker.ExpandCollapseButtonsVisible",
    "gTrackers.CheckTracker.SearchInputVisible",
    "gTrackers.CheckTracker.CheckTotalsVisible",
    "gTrackers.CheckTracker.EnableAvailableChecks", // #1129
    "gTrackers.CheckTracker.AvailableChecksToggleVisible",
    "gTrackers.CheckTracker.OnlyShowAvailable",
};

/** Every key above as it was, put back afterwards (set again or cleared), so no player value is lost or left. */
struct CtwKeyGuard {
    struct Saved {
        const char* key;
        bool existed;
        int32_t value;
    };
    std::vector<Saved> saved;
    CtwKeyGuard() {
        for (const char* key : kCtwHideKeys) {
            Keep(key);
        }
        for (const char* key : kCtwOtherKeys) {
            Keep(key);
        }
    }
    void Keep(const char* key) {
        saved.push_back({ key, CVarExists(key), CVarGetInteger(key, 0) });
        CVarClear(key);
    }
    ~CtwKeyGuard() {
        for (const Saved& k : saved) {
            if (k.existed) {
                CVarSetInteger(k.key, k.value);
            } else {
                CVarClear(k.key);
            }
        }
    }
};

int LockHideBody(void) {
    using namespace ComboGui;
    std::vector<ComboTrackerAreaRows> areas;

    // The row decision, per status: its own Hide hides it, no other status's
    // does, and the pane's Show Hidden Items overrides every one.
    for (int s = 0; s < (int)COMBO_TRACKER_CHECK_STATUS_COUNT; s++) {
        ComboCheckHideOptions opts;
        opts.hideStatus[s] = true;
        for (int r = 0; r < (int)COMBO_TRACKER_CHECK_STATUS_COUNT; r++) {
            ComboTrackerCheckRow row = {};
            row.shuffled = true;
            row.status = (uint8_t)r;
            if (ComboCheckRowHidden(row, opts) != (r == s)) {
                printf("[TEST] FAIL: combo-tracker-window #1128: %s set, a status-%d row hidden = %d\n",
                       kCtwHideKeys[s], r, (int)ComboCheckRowHidden(row, opts));
                return TEST_FAIL;
            }
        }
        ComboCheckHideOptions shown = opts;
        shown.showHidden = true;
        ComboTrackerCheckRow row = {};
        row.status = (uint8_t)s;
        CTW_ASSERT(!ComboCheckRowHidden(row, shown));
    }

    // The list as drawn. Defaults (SoH's: everything shown) draw what
    // ComboCollectCheckAreas lists.
    ComboCheckHideOptions opts;
    ComboCollectVisibleCheckAreas((uint8_t)GAME_OOT, "", opts, areas);
    printf("[TEST] combo-tracker-window #1128: defaults draw \"%s\"\n", CtwDrawn(areas).c_str());
    CTW_ASSERT(CtwDrawn(areas) == "3:11,13 7:10,12");
    // Collected hidden: its row goes, its area's totals stay whole.
    opts.hideStatus[COMBO_TRACKER_CHECK_COLLECTED] = true;
    ComboCollectVisibleCheckAreas((uint8_t)GAME_OOT, "", opts, areas);
    printf("[TEST] combo-tracker-window #1128: Collected.Hide draws \"%s\"\n", CtwDrawn(areas).c_str());
    CTW_ASSERT(CtwDrawn(areas) == "3:11,13 7:12");
    CTW_ASSERT(areas[1].total == 2 && areas[1].done == 2);
    // Every row of an area hidden by status: its header stays (SoH draws it).
    opts.hideStatus[COMBO_TRACKER_CHECK_SKIPPED] = true;
    ComboCollectVisibleCheckAreas((uint8_t)GAME_OOT, "", opts, areas);
    CTW_ASSERT(CtwDrawn(areas) == "3:11,13 7:");
    // The pane's Show Hidden Items brings both back.
    opts.showHidden = true;
    ComboCollectVisibleCheckAreas((uint8_t)GAME_OOT, "", opts, areas);
    CTW_ASSERT(CtwDrawn(areas) == "3:11,13 7:10,12");

    // Area complete / incomplete hide, and Show Hidden Items over both.
    opts = ComboCheckHideOptions();
    opts.hideAreaComplete = true;
    ComboCollectVisibleCheckAreas((uint8_t)GAME_OOT, "", opts, areas);
    printf("[TEST] combo-tracker-window #1128: AreaComplete.Hide draws \"%s\"\n", CtwDrawn(areas).c_str());
    CTW_ASSERT(CtwDrawn(areas) == "3:11,13");
    opts.showHidden = true;
    ComboCollectVisibleCheckAreas((uint8_t)GAME_OOT, "", opts, areas);
    CTW_ASSERT(CtwDrawn(areas) == "3:11,13 7:10,12");
    opts = ComboCheckHideOptions();
    opts.hideAreaIncomplete = true;
    ComboCollectVisibleCheckAreas((uint8_t)GAME_OOT, "", opts, areas);
    CTW_ASSERT(CtwDrawn(areas) == "7:10,12");

    // Filter Empty Areas: on (SoH's default), an area the search empties goes,
    // whatever Show Hidden Items says; off, its header stays with no rows.
    opts = ComboCheckHideOptions();
    ComboCollectVisibleCheckAreas((uint8_t)GAME_OOT, "roof", opts, areas);
    CTW_ASSERT(CtwDrawn(areas) == "7:12");
    opts.showHidden = true;
    ComboCollectVisibleCheckAreas((uint8_t)GAME_OOT, "roof", opts, areas);
    CTW_ASSERT(CtwDrawn(areas) == "7:12");
    opts.hideFilteredAreas = false;
    ComboCollectVisibleCheckAreas((uint8_t)GAME_OOT, "roof", opts, areas);
    printf("[TEST] combo-tracker-window #1128: search \"roof\" with Filter Empty Areas off draws \"%s\"\n",
           CtwDrawn(areas).c_str());
    CTW_ASSERT(CtwDrawn(areas) == "3:- 7:12");
    CTW_ASSERT(areas[0].total == 2 && areas[0].rows.empty());

    // The keys, with SoH's defaults: nothing set reads as everything shown and
    // Filter Empty Areas on; each key reaches its own field.
    {
        CtwKeyGuard guard;
        ComboCheckHideOptions read = ComboReadCheckHideOptions(false);
        for (int s = 0; s < (int)COMBO_TRACKER_CHECK_STATUS_COUNT; s++) {
            CTW_ASSERT(!read.hideStatus[s]);
        }
        CTW_ASSERT(!read.hideAreaComplete && !read.hideAreaIncomplete && read.hideFilteredAreas && !read.showHidden);
        CTW_ASSERT(ComboReadCheckHideOptions(true).showHidden);
        for (int s = 0; s < (int)COMBO_TRACKER_CHECK_STATUS_COUNT; s++) {
            CVarSetInteger(kCtwHideKeys[s], 1);
            read = ComboReadCheckHideOptions(false);
            CVarClear(kCtwHideKeys[s]);
            if (!read.hideStatus[s]) {
                printf("[TEST] FAIL: combo-tracker-window #1128: %s does not reach the pane\n", kCtwHideKeys[s]);
                return TEST_FAIL;
            }
        }
        CVarSetInteger("gTrackers.CheckTracker.AreaComplete.Hide", 1);
        CVarSetInteger("gTrackers.CheckTracker.AreaIncomplete.Hide", 1);
        CVarSetInteger("gTrackers.CheckTracker.HideFilteredAreas", 0);
        read = ComboReadCheckHideOptions(false);
        CTW_ASSERT(read.hideAreaComplete && read.hideAreaIncomplete && !read.hideFilteredAreas);

        ComboCheckHeaderOptions header = ComboReadCheckHeaderOptions();
        CTW_ASSERT(header.hiddenItemsToggle && !header.expandCollapseButtons && header.searchInput &&
                   header.checkTotals);
        CVarSetInteger("gTrackers.CheckTracker.HiddenItemsToggleVisible", 0);
        CVarSetInteger("gTrackers.CheckTracker.ExpandCollapseButtonsVisible", 1);
        CVarSetInteger("gTrackers.CheckTracker.SearchInputVisible", 0);
        CVarSetInteger("gTrackers.CheckTracker.CheckTotalsVisible", 0);
        header = ComboReadCheckHeaderOptions();
        printf("[TEST] combo-tracker-window #1128: header keys flipped read toggle %d, expand/collapse %d, search %d, "
               "totals %d\n",
               (int)header.hiddenItemsToggle, (int)header.expandCollapseButtons, (int)header.searchInput,
               (int)header.checkTotals);
        CTW_ASSERT(!header.hiddenItemsToggle && header.expandCollapseButtons && !header.searchInput &&
                   !header.checkTotals);
    }

    // The totals line counts a skipped check as checked, as SoH's Checked, MM's
    // collected count and the area headers above do.
    ComboTrackerGameSummary summary = {};
    summary.obtained = 5;
    summary.skipped = 2;
    summary.shuffled = 100;
    const std::string totals = ComboCheckTotalsText(summary);
    printf("[TEST] combo-tracker-window #1128: obtained 5, skipped 2, shuffled 100 prints \"%s\"\n", totals.c_str());
    CTW_ASSERT(totals == "Checks: 7 / 100");
    return TEST_PASS;
}

int LockHide(void) {
    Combo_Tracker_RegisterOoT(&kCtwOps);
    const int result = LockHideBody();
    OoT_TrackerAdapter_Register(); // the production adapter, whatever the result
    return result;
}

// ---- #1129 (lane L3): reachability ---------------------------------------------

// The grouping world with each row's availability known: the ids in
// sCtwAvailableIds are available, every other row is not.
std::vector<uint16_t> sCtwAvailableIds;

bool CtwAvailCheckAt(int index, ComboTrackerCheckRow* out) {
    if (!CtwCheckAt(index, out)) {
        return false;
    }
    out->availabilityKnown = true;
    out->available =
        std::find(sCtwAvailableIds.begin(), sCtwAvailableIds.end(), out->checkId) != sCtwAvailableIds.end();
    return true;
}
const ComboOoTTrackerOps kCtwAvailOps = { CtwSummary, CtwCount, CtwAvailCheckAt, CtwCheckName };

int LockAvailBody(void) {
    using namespace ComboGui;
    std::vector<ComboTrackerAreaRows> areas;

    // The glyph: SoH's unlock / lock, and none at all when not known.
    ComboTrackerCheckRow row = {};
    CTW_ASSERT(ComboAvailabilityGlyph(row) == nullptr);
    row.available = true; // without availabilityKnown it is still no answer
    CTW_ASSERT(ComboAvailabilityGlyph(row) == nullptr);
    row.availabilityKnown = true;
    const char* unlock = ComboAvailabilityGlyph(row);
    row.available = false;
    const char* lock = ComboAvailabilityGlyph(row);
    printf("[TEST] combo-tracker-window #1129: glyphs available \"%s\", unavailable \"%s\"\n",
           unlock != nullptr ? unlock : "(none)", lock != nullptr ? lock : "(none)");
    CTW_ASSERT(unlock != nullptr && strcmp(unlock, ICON_FA_UNLOCK) == 0);
    CTW_ASSERT(lock != nullptr && strcmp(lock, ICON_FA_LOCK) == 0);

    // The row decision: Only Show Available leaves out a row known unavailable,
    // whatever Show Hidden Items says (SoH's DrawLocation returns before its hide
    // test), and never a row whose availability is not known.
    ComboCheckHideOptions opts;
    opts.onlyAvailable = true;
    row = {};
    row.shuffled = true;
    CTW_ASSERT(!ComboCheckRowHidden(row, opts)); // not known
    row.availabilityKnown = true;
    CTW_ASSERT(ComboCheckRowHidden(row, opts)); // known unavailable
    opts.showHidden = true;
    CTW_ASSERT(ComboCheckRowHidden(row, opts));
    row.available = true;
    CTW_ASSERT(!ComboCheckRowHidden(row, opts));
    opts.onlyAvailable = false;
    row.available = false;
    CTW_ASSERT(!ComboCheckRowHidden(row, opts));

    // The list: 0x11 (area 3) and 0x12 (area 7) available.
    sCtwAvailableIds = { 0x11, 0x12 };
    Combo_Tracker_RegisterOoT(&kCtwAvailOps);
    opts = ComboCheckHideOptions();
    ComboCollectVisibleCheckAreas((uint8_t)GAME_OOT, "", opts, areas);
    CTW_ASSERT(CtwDrawn(areas) == "3:11,13 7:10,12");
    CTW_ASSERT(areas.size() == 2 && areas[0].availabilityKnown && areas[0].available == 1 &&
               areas[1].availabilityKnown && areas[1].available == 1);
    opts.onlyAvailable = true;
    ComboCollectVisibleCheckAreas((uint8_t)GAME_OOT, "", opts, areas);
    printf("[TEST] combo-tracker-window #1129: Only Show Available draws \"%s\"\n", CtwDrawn(areas).c_str());
    CTW_ASSERT(CtwDrawn(areas) == "3:11 7:12");
    CTW_ASSERT(areas[1].total == 2 && areas[1].done == 2); // the area's totals stay whole
    // An area with none available goes (SoH's area loop), even under Show Hidden Items.
    sCtwAvailableIds = { 0x11 };
    opts.showHidden = true;
    ComboCollectVisibleCheckAreas((uint8_t)GAME_OOT, "", opts, areas);
    CTW_ASSERT(CtwDrawn(areas) == "3:11");
    // A world whose availability is not known: nothing goes.
    Combo_Tracker_RegisterOoT(&kCtwOps);
    ComboCollectVisibleCheckAreas((uint8_t)GAME_OOT, "", opts, areas);
    CTW_ASSERT(CtwDrawn(areas) == "3:11,13 7:10,12");
    CTW_ASSERT(!areas[0].availabilityKnown && areas[0].available == 0);

    // The totals line and the area header: SoH's Available count in front, only
    // with Enable Available Checks on and the count known.
    ComboTrackerGameSummary summary = {};
    summary.obtained = 5;
    summary.skipped = 2;
    summary.shuffled = 100;
    summary.available = 12;
    CTW_ASSERT(ComboCheckTotalsText(summary, true) == "Checks: 7 / 100"); // not known
    summary.availabilityKnown = true;
    const std::string withAvailable = ComboCheckTotalsText(summary, true);
    printf("[TEST] combo-tracker-window #1129: 12 available prints \"%s\"\n", withAvailable.c_str());
    CTW_ASSERT(withAvailable == "12 Available / Checks: 7 / 100");
    CTW_ASSERT(ComboCheckTotalsText(summary, false) == "Checks: 7 / 100");
    ComboTrackerAreaRows area;
    area.total = 4;
    area.done = 1;
    area.available = 2;
    CTW_ASSERT(ComboAreaTotalsText(area, true) == "(1 / 4)"); // not known
    area.availabilityKnown = true;
    CTW_ASSERT(ComboAreaTotalsText(area, true) == "(2 / 1 / 4)");
    CTW_ASSERT(ComboAreaTotalsText(area, false) == "(1 / 4)");

    // The keys, with SoH's defaults: Enable Available Checks off, its toggle shown.
    {
        CtwKeyGuard guard;
        ComboCheckHeaderOptions header = ComboReadCheckHeaderOptions();
        CTW_ASSERT(!header.availableChecks && header.availableChecksToggle);
        CVarSetInteger("gTrackers.CheckTracker.EnableAvailableChecks", 1);
        CVarSetInteger("gTrackers.CheckTracker.AvailableChecksToggleVisible", 0);
        header = ComboReadCheckHeaderOptions();
        printf("[TEST] combo-tracker-window #1129: available keys flipped read enabled %d, toggle %d\n",
               (int)header.availableChecks, (int)header.availableChecksToggle);
        CTW_ASSERT(header.availableChecks && !header.availableChecksToggle);
    }
    return TEST_PASS;
}

int LockAvail(void) {
    // OoT is played: the synthetic registrant's rows are the LIVE panel's (while
    // MM is played the view shows the departure capture instead, #1129).
    const GameId prevGame = Context_GetCurrentGame();
    Context_SetCurrentGame(GAME_OOT);
    const int result = LockAvailBody();
    Context_SetCurrentGame(prevGame);
    sCtwAvailableIds.clear();
    OoT_TrackerAdapter_Register(); // the production adapter, whatever the result
    return result;
}

// ---- #1166 (lane T1): a seen check's item and price ---------------------------

/** What a seen OoT row prints after its name (SoH's DrawLocation), from `seen`'s facts. */
std::string CtwSeenText(const ComboTrackerSeenItem& seen) {
    ComboTrackerCheckRow row = {};
    row.status = COMBO_TRACKER_CHECK_SEEN;
    Combo_TrackerNameSeenItem(&seen, &row);
    return ComboGui::ComboCheckRowItemText(seen.game, row);
}

/** A listed Kokiri Shop slot holding a Hookshot at 40 rupees, seen and not identified. */
ComboTrackerSeenItem CtwShopSlot(void) {
    ComboTrackerSeenItem seen = {};
    seen.game = (uint8_t)GAME_OOT;
    seen.saleKind = COMBO_TRACKER_SALE_SHOP;
    seen.listed = true;
    seen.canBePurchased = true;
    seen.itemName = "Hookshot";
    seen.itemGame = (uint8_t)GAME_OOT;
    seen.price = 40;
    return seen;
}

int LockSeen(void) {
    // A seen shop slot names its item (SoH's DrawLocation, RCSHOW_SEEN).
    ComboTrackerSeenItem shop = CtwShopSlot();
    const std::string seenText = CtwSeenText(shop);
    printf("[TEST] combo-tracker-window #1166: a seen shop slot prints \"%s\"\n", seenText.c_str());
    CTW_ASSERT(seenText == "Hookshot");
    // Identified: the price follows, in SoH's "{} - {}" shape.
    shop.identified = true;
    const std::string identifiedText = CtwSeenText(shop);
    printf("[TEST] combo-tracker-window #1166: an identified shop slot prints \"%s\"\n", identifiedText.c_str());
    CTW_ASSERT(identifiedText == "Hookshot - 40");
    // Mysterious Shuffle hides a randomized item, and the price prints alone.
    shop.mystery = true;
    shop.mysteryShopItem = true;
    CTW_ASSERT(CtwSeenText(shop) == "40");
    shop.identified = false;
    CTW_ASSERT(CtwSeenText(shop).empty());
    // ...but not a slot that sells its vanilla item.
    shop.mysteryShopItem = false;
    CTW_ASSERT(CtwSeenText(shop) == "Hookshot");
    // A slot SoH does not list prints no price.
    shop = CtwShopSlot();
    shop.identified = true;
    shop.listed = false;
    CTW_ASSERT(CtwSeenText(shop) == "Hookshot");
    // A crossing host's crossed item carries its game's suffix.
    shop = CtwShopSlot();
    shop.itemName = "Lens of Truth";
    shop.itemGame = (uint8_t)GAME_MM;
    CTW_ASSERT(CtwSeenText(shop) == "Lens of Truth (MM)");

    // CanBePurchased false (SoH 2ba368e7e): an identified check that sells
    // nothing, such as a Skulltula House reward, prints no price ("- 0" before).
    ComboTrackerSeenItem reward = CtwShopSlot();
    reward.saleKind = COMBO_TRACKER_SALE_NONE;
    reward.canBePurchased = false;
    reward.identified = true;
    reward.price = 0;
    const std::string rewardText = CtwSeenText(reward);
    printf("[TEST] combo-tracker-window #1166: an identified Skulltula reward prints \"%s\"\n", rewardText.c_str());
    CTW_ASSERT(rewardText == "Hookshot");
    reward.itemName = nullptr;
    CTW_ASSERT(CtwSeenText(reward).empty());

    // A merchant's item shows only with Merchant Hint Text, a scrub's only with
    // Scrub Hint Text, and neither under Mysterious Shuffle.
    ComboTrackerSeenItem merchant = CtwShopSlot();
    merchant.saleKind = COMBO_TRACKER_SALE_MERCHANT;
    CTW_ASSERT(CtwSeenText(merchant).empty());
    merchant.merchantTextHint = true;
    CTW_ASSERT(CtwSeenText(merchant) == "Hookshot");
    merchant.scrubTextHint = true;
    merchant.mystery = true;
    CTW_ASSERT(CtwSeenText(merchant).empty());
    ComboTrackerSeenItem scrub = CtwShopSlot();
    scrub.saleKind = COMBO_TRACKER_SALE_SCRUB;
    scrub.merchantTextHint = true;
    scrub.identified = true;
    CTW_ASSERT(CtwSeenText(scrub) == "40");
    scrub.scrubTextHint = true;
    CTW_ASSERT(CtwSeenText(scrub) == "Hookshot - 40");
    scrub.mystery = true;
    CTW_ASSERT(CtwSeenText(scrub) == "40");
    return TEST_PASS;
}

int LockIceTrap(void) {
    // SoH's ice-trap naming: its disguise once seen, its trick name once
    // identified, and only where the item would be named at all.
    ComboTrackerSeenItem trap = CtwShopSlot();
    trap.iceTrap = true;
    trap.itemName = "Ice Trap"; // never printed for a trap
    trap.looksLikeName = "Fairy Bow";
    trap.trickName = "Fary Bow";
    trap.price = 25;
    const std::string seenText = CtwSeenText(trap);
    trap.identified = true;
    const std::string identifiedText = CtwSeenText(trap);
    printf("[TEST] combo-tracker-window #1166: a seen ice trap prints \"%s\", identified \"%s\"\n", seenText.c_str(),
           identifiedText.c_str());
    CTW_ASSERT(seenText == "Fairy Bow");
    CTW_ASSERT(identifiedText == "Fary Bow - 25");
    trap.mystery = true;
    trap.mysteryShopItem = true;
    CTW_ASSERT(CtwSeenText(trap) == "25");
    // A trap with no override names nothing.
    trap = CtwShopSlot();
    trap.iceTrap = true;
    trap.itemName = "Ice Trap";
    CTW_ASSERT(CtwSeenText(trap).empty());
    return TEST_PASS;
}

// ---- #1161 (lane T1): a check's logic on hover --------------------------------

// A registrant's logic op: a condition for 0x10, SoH's "true" for 0x11, none else.
const char* CtwLogic(uint16_t id) {
    if (id == 0x10) {
        return "CanUse(RG_FAIRY_BOW) && IsAdult";
    }
    return id == 0x11 ? "true" : nullptr;
}
const ComboOoTTrackerOps kCtwLogicOps = { CtwSummary, CtwCount, CtwCheckAt, CtwCheckName, nullptr, nullptr, CtwLogic };

int LockLogicBody(void) {
    using namespace ComboGui;
    Combo_Tracker_RegisterOoT(&kCtwLogicOps);
    ComboTrackerCheckRow row;
    CTW_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, 0, &row) && row.checkId == 0x10);
    const std::string on = ComboCheckLogicTooltip((uint8_t)GAME_OOT, row, true);
    const std::string off = ComboCheckLogicTooltip((uint8_t)GAME_OOT, row, false);
    printf("[TEST] combo-tracker-window #1161: OoT logic tooltip with Show Logic \"%s\", without \"%s\"\n", on.c_str(),
           off.c_str());
    CTW_ASSERT(on == "CanUse(RG_FAIRY_BOW) && IsAdult");
    CTW_ASSERT(off.empty());
    // SoH draws none for a condition of "true", nor where there is no condition.
    CTW_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, 1, &row) && row.checkId == 0x11);
    CTW_ASSERT(ComboCheckLogicTooltip((uint8_t)GAME_OOT, row, true).empty());
    CTW_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, 2, &row) && row.checkId == 0x12);
    CTW_ASSERT(ComboCheckLogicTooltip((uint8_t)GAME_OOT, row, true).empty());
    // A registrant with no logic op shows none.
    Combo_Tracker_RegisterOoT(&kCtwOps);
    CTW_ASSERT(Combo_TrackerCheckAt((uint8_t)GAME_OOT, 0, &row));
    CTW_ASSERT(ComboCheckLogicTooltip((uint8_t)GAME_OOT, row, true).empty());

    // MM's panel, through its own descriptor's op, under the same key. The state
    // loop above ends with no adapter registered, so MM's production descriptor
    // is registered first and copied.
    MM_TrackerAdapter_Register();
    const ComboMMTrackerDesc* real = Combo_Tracker_GetMMDesc();
    CTW_ASSERT(real != nullptr);
    ComboMMTrackerDesc desc = *real;
    desc.logicText = CtwLogic;
    Combo_Tracker_RegisterMM(&desc);
    row = {};
    row.checkId = 0x10;
    const std::string mm = ComboCheckLogicTooltip((uint8_t)GAME_MM, row, true);
    printf("[TEST] combo-tracker-window #1161: MM logic tooltip with Show Logic \"%s\"\n", mm.c_str());
    CTW_ASSERT(mm == "CanUse(RG_FAIRY_BOW) && IsAdult");
    CTW_ASSERT(ComboCheckLogicTooltip((uint8_t)GAME_MM, row, false).empty());
    desc.logicText = nullptr;
    Combo_Tracker_RegisterMM(&desc);
    CTW_ASSERT(ComboCheckLogicTooltip((uint8_t)GAME_MM, row, true).empty());

    // The key: SoH's Show Logic, off by default, one key for both panes.
    {
        CtwKeyGuard guard;
        guard.Keep("gTrackers.CheckTracker.ShowLogic");
        CTW_ASSERT(!ComboReadCheckHeaderOptions().showLogic);
        CVarSetInteger("gTrackers.CheckTracker.ShowLogic", 1);
        const bool read = ComboReadCheckHeaderOptions().showLogic;
        printf("[TEST] combo-tracker-window #1161: Show Logic set reads %d\n", (int)read);
        CTW_ASSERT(read);
    }
    return TEST_PASS;
}

int LockLogic(void) {
    const int result = LockLogicBody();
    OoT_TrackerAdapter_Register(); // the production adapters, whatever the result
    MM_TrackerAdapter_Register();
    return result;
}

// ---- #1130 (lane L1b): the window's chrome ----------------------------------------

// SoH's chrome keys (randomizer_check_tracker.cpp, DrawElement and the Check
// Tracker Settings table), as the pane reads them.
const char* const kCtwChromeIntKeys[] = {
    "gTrackers.CheckTracker.WindowType",     "gTrackers.CheckTracker.Draggable",
    "gTrackers.CheckTracker.ShowOnlyPaused", "gTrackers.CheckTracker.DisplayType",
    "gTrackers.CheckTracker.ComboButton1",   "gTrackers.CheckTracker.ComboButton2",
};
constexpr size_t kCtwChromeIntKeyCount = sizeof(kCtwChromeIntKeys) / sizeof(kCtwChromeIntKeys[0]);
constexpr const char* kCtwBgKey = "gTrackers.CheckTracker.BgColor.Value";
constexpr const char* kCtwFontKey = "gTrackers.CheckTracker.FontSize";

/** The chrome keys as they were, cleared meanwhile and put back afterwards (set again or cleared). */
struct CtwChromeKeyGuard {
    bool intExisted[kCtwChromeIntKeyCount] = {};
    int32_t intValue[kCtwChromeIntKeyCount] = {};
    bool bgExisted = false;
    Color_RGBA8 bg = {};
    bool fontExisted = false;
    float font = 0.0f;
    CtwChromeKeyGuard() {
        for (size_t i = 0; i < kCtwChromeIntKeyCount; i++) {
            intExisted[i] = CVarExists(kCtwChromeIntKeys[i]);
            intValue[i] = CVarGetInteger(kCtwChromeIntKeys[i], 0);
            CVarClear(kCtwChromeIntKeys[i]);
        }
        bgExisted = CVarExists(kCtwBgKey);
        bg = CVarGetColor(kCtwBgKey, Color_RGBA8{ 0, 0, 0, 255 });
        CVarClear(kCtwBgKey);
        fontExisted = CVarExists(kCtwFontKey);
        font = CVarGetFloat(kCtwFontKey, 1.0f);
        CVarClear(kCtwFontKey);
    }
    ~CtwChromeKeyGuard() {
        for (size_t i = 0; i < kCtwChromeIntKeyCount; i++) {
            if (intExisted[i]) {
                CVarSetInteger(kCtwChromeIntKeys[i], intValue[i]);
            } else {
                CVarClear(kCtwChromeIntKeys[i]);
            }
        }
        if (bgExisted) {
            CVarSetColor(kCtwBgKey, bg);
        } else {
            CVarClear(kCtwBgKey);
        }
        if (fontExisted) {
            CVarSetFloat(kCtwFontKey, font);
        } else {
            CVarClear(kCtwFontKey);
        }
    }
};

int LockChrome(void) {
    using namespace ComboGui;

    // SoH's `buttons` table (randomizer_entrance_tracker.h), in TrackerComboButton
    // order: A, B, C-Up, C-Down, C-Left, C-Right, L, Z, R, Start, D-Up, D-Down,
    // D-Left, D-Right (libultra's BTN_* bits).
    const uint32_t kSohButtons[kComboTrackerComboButtonCount] = { 0x8000, 0x4000, 0x0008, 0x0004, 0x0002,
                                                                  0x0001, 0x0020, 0x2000, 0x0010, 0x1000,
                                                                  0x0800, 0x0400, 0x0200, 0x0100 };
    for (int b = 0; b < kComboTrackerComboButtonCount; b++) {
        if (ComboTrackerComboButtonMask(b) != kSohButtons[b]) {
            printf("[TEST] FAIL: combo-tracker-window #1130: Combo Button %d is pad bit 0x%04X, SoH's is 0x%04X\n", b,
                   (unsigned)ComboTrackerComboButtonMask(b), (unsigned)kSohButtons[b]);
            return TEST_FAIL;
        }
    }
    CTW_ASSERT(ComboTrackerComboButtonMask(-1) == 0 && ComboTrackerComboButtonMask(kComboTrackerComboButtonCount) == 0);
    const uint32_t padL = 0x0020;
    const uint32_t padR = 0x0010;
    const uint32_t padZ = 0x2000;
    const uint32_t padA = 0x8000;

    const int floatingAdds = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoTitleBar;
    const int fixedAdds = ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;

    // The Window type: a titled pane with a close button, whatever the floating
    // gates say (SoH applies them to the floating type only); any value but
    // Floating is the window type, as SoH's == TRACKER_WINDOW_FLOATING tests read it.
    for (int type : { kComboTrackerWindowWindow, 7 }) {
        for (int bits = 0; bits < 16; bits++) {
            ComboTrackerChromeKeys keys;
            keys.windowType = type;
            keys.draggable = (bits & 1) != 0;
            keys.showOnlyPaused = (bits & 2) != 0;
            keys.displayType = (bits & 4) != 0 ? kComboTrackerDisplayComboButton : kComboTrackerDisplayAlways;
            const bool paused = (bits & 8) != 0;
            const ComboTrackerChrome c = ComboTrackerChromeFor(keys, paused, 0);
            if (!c.draws || !c.closeButton || c.mainViewport || c.flags != ImGuiWindowFlags_NoFocusOnAppearing) {
                printf("[TEST] FAIL: combo-tracker-window #1130: window type %d, keys %d: draws %d, close %d, main "
                       "viewport %d, flags 0x%X\n",
                       type, bits, (int)c.draws, (int)c.closeButton, (int)c.mainViewport, (unsigned)c.flags);
                return TEST_FAIL;
            }
        }
    }

    // The Floating type: SoH's overlay flags, input-less and fixed unless
    // Draggable, on the main viewport, with no close button.
    {
        ComboTrackerChromeKeys keys;
        keys.windowType = kComboTrackerWindowFloating;
        ComboTrackerChrome c = ComboTrackerChromeFor(keys, false, 0);
        printf("[TEST] combo-tracker-window #1130: floating, draggable: draws %d, close %d, main viewport %d, flags "
               "0x%X\n",
               (int)c.draws, (int)c.closeButton, (int)c.mainViewport, (unsigned)c.flags);
        CTW_ASSERT(c.draws && !c.closeButton && c.mainViewport);
        CTW_ASSERT(c.flags == (ImGuiWindowFlags_NoFocusOnAppearing | floatingAdds));
        keys.draggable = false;
        c = ComboTrackerChromeFor(keys, false, 0);
        CTW_ASSERT(c.draws && !c.closeButton && c.mainViewport);
        CTW_ASSERT(c.flags == (ImGuiWindowFlags_NoFocusOnAppearing | floatingAdds | fixedAdds));
        // The pane scrolls as a whole: SoH's scroll flags are for its outer
        // window over an inner scrolling table, which this pane does not have.
        CTW_ASSERT((c.flags & (ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) == 0);
    }

    // Only Enable While Paused: the floating overlay draws only while the
    // active game is paused.
    {
        ComboTrackerChromeKeys keys;
        keys.windowType = kComboTrackerWindowFloating;
        keys.showOnlyPaused = true;
        CTW_ASSERT(!ComboTrackerChromeFor(keys, false, 0).draws);
        CTW_ASSERT(ComboTrackerChromeFor(keys, true, 0).draws);
        keys.showOnlyPaused = false;
        CTW_ASSERT(ComboTrackerChromeFor(keys, false, 0).draws);
    }

    // Combo Button Hold: only while both of its buttons are held on the first
    // pad, by default L and R; other buttons held alongside do not matter.
    {
        ComboTrackerChromeKeys keys;
        keys.windowType = kComboTrackerWindowFloating;
        keys.displayType = kComboTrackerDisplayComboButton;
        CTW_ASSERT(!ComboTrackerChromeFor(keys, false, 0).draws);
        CTW_ASSERT(!ComboTrackerChromeFor(keys, false, padL).draws);
        CTW_ASSERT(!ComboTrackerChromeFor(keys, false, padR).draws);
        CTW_ASSERT(ComboTrackerChromeFor(keys, false, padL | padR).draws);
        CTW_ASSERT(ComboTrackerChromeFor(keys, false, padL | padR | padA).draws);
        keys.comboButton1 = 0; // A
        keys.comboButton2 = 7; // Z
        CTW_ASSERT(!ComboTrackerChromeFor(keys, false, padL | padR).draws);
        CTW_ASSERT(ComboTrackerChromeFor(keys, false, padA | padZ).draws);
        // Both gates at once: held but not paused stays hidden.
        keys.showOnlyPaused = true;
        CTW_ASSERT(!ComboTrackerChromeFor(keys, false, padA | padZ).draws);
        CTW_ASSERT(ComboTrackerChromeFor(keys, true, padA | padZ).draws);
        CTW_ASSERT(!ComboTrackerChromeFor(keys, true, padA).draws);
    }

    // The keys, under SoH's names with SoH's defaults (the hold's: Combo Button
    // 1 L, Combo Button 2 R), each reaching its own field.
    {
        CtwChromeKeyGuard guard;
        ComboTrackerChromeKeys read = ComboReadTrackerChromeKeys();
        CTW_ASSERT(read.windowType == kComboTrackerWindowWindow && read.draggable && !read.showOnlyPaused &&
                   read.displayType == kComboTrackerDisplayAlways && read.comboButton1 == kComboTrackerComboButtonL &&
                   read.comboButton2 == kComboTrackerComboButtonR);
        CTW_ASSERT(read.bg[0] == 0 && read.bg[1] == 0 && read.bg[2] == 0 && read.bg[3] == 255);
        CTW_ASSERT(read.fontSize == 1.0f);
        CVarSetInteger("gTrackers.CheckTracker.WindowType", kComboTrackerWindowFloating);
        CVarSetInteger("gTrackers.CheckTracker.Draggable", 0);
        CVarSetInteger("gTrackers.CheckTracker.ShowOnlyPaused", 1);
        CVarSetInteger("gTrackers.CheckTracker.DisplayType", kComboTrackerDisplayComboButton);
        CVarSetInteger("gTrackers.CheckTracker.ComboButton1", 3);
        CVarSetInteger("gTrackers.CheckTracker.ComboButton2", 11);
        CVarSetColor(kCtwBgKey, Color_RGBA8{ 10, 20, 30, 40 });
        CVarSetFloat(kCtwFontKey, 1.5f);
        read = ComboReadTrackerChromeKeys();
        printf("[TEST] combo-tracker-window #1130: chrome keys flipped read type %d, draggable %d, paused-only %d, "
               "display %d, buttons %d/%d, bg %d,%d,%d,%d, font %.2f\n",
               read.windowType, (int)read.draggable, (int)read.showOnlyPaused, read.displayType, read.comboButton1,
               read.comboButton2, read.bg[0], read.bg[1], read.bg[2], read.bg[3], read.fontSize);
        CTW_ASSERT(read.windowType == kComboTrackerWindowFloating && !read.draggable && read.showOnlyPaused &&
                   read.displayType == kComboTrackerDisplayComboButton && read.comboButton1 == 3 &&
                   read.comboButton2 == 11);
        CTW_ASSERT(read.bg[0] == 10 && read.bg[1] == 20 && read.bg[2] == 30 && read.bg[3] == 40);
        CTW_ASSERT(read.fontSize == 1.5f);
    }
    return TEST_PASS;
}

/** Every section runs, so one red section does not hide another's state. */
int TrackerLayoutLocks(void) {
    const int r815 = Lock815();
    const int r816 = Lock816();
    const int rAreas = LockAreas();
    const int rHide = LockHide();
    const int rAvail = LockAvail();
    const int rChrome = LockChrome();
    const int rSeen = LockSeen();
    const int rIce = LockIceTrap();
    const int rLogic = LockLogic();
    printf("[TEST] combo-tracker-window layout locks: #815 %s, #816 %s, U4 areas+search %s, #1128 hide+totals %s, "
           "#1129 reachability %s, #1130 chrome %s, #1166 seen item+price %s, #1166 ice trap %s, #1161 logic %s\n",
           r815 == TEST_PASS ? "PASS" : "FAIL", r816 == TEST_PASS ? "PASS" : "FAIL",
           rAreas == TEST_PASS ? "PASS" : "FAIL", rHide == TEST_PASS ? "PASS" : "FAIL",
           rAvail == TEST_PASS ? "PASS" : "FAIL", rChrome == TEST_PASS ? "PASS" : "FAIL",
           rSeen == TEST_PASS ? "PASS" : "FAIL", rIce == TEST_PASS ? "PASS" : "FAIL",
           rLogic == TEST_PASS ? "PASS" : "FAIL");
    return (r815 == TEST_PASS && r816 == TEST_PASS && rAreas == TEST_PASS && rHide == TEST_PASS &&
            rAvail == TEST_PASS && rChrome == TEST_PASS && rSeen == TEST_PASS && rIce == TEST_PASS &&
            rLogic == TEST_PASS)
               ? TEST_PASS
               : TEST_FAIL;
}

} // namespace

extern "C" int Combo_TrackerWindow_RunHeadless(void) {
    printf("[TEST] combo-tracker-window: the combo tracker window registers de-collided and is inert under every "
           "active game (#458, ADR 0008)\n");

    // Production entry point must be headless-safe: with no window on the
    // shared context it registers the adapters and returns without touching a
    // Gui.
    Combo_TrackerWindow_Init();

    auto gui = std::make_shared<Ship::Gui>();

    std::shared_ptr<Ship::GuiWindow> neighbours[kTrackerNeighbourCount];
    for (int i = 0; i < kTrackerNeighbourCount; i++) {
        neighbours[i] = std::make_shared<TrackerStandinWindow>("", kTrackerNeighbourNames[i]);
        gui->AddGuiWindow(neighbours[i]);
    }

    // Keep the window shut for every Draw() below: the live-CVar early-out is
    // the only thing between Draw() and ImGui::Begin in a process with no
    // ImGui context.
    CVarClear(ComboGui::kComboTrackerVisibilityCVar);

    ComboGui::RegisterComboTrackerWindow(gui);
    auto window = gui->GetGuiWindow(ComboGui::kComboTrackerWindowName);
    CTW_ASSERT(window != nullptr);

    // Idempotence: a second registration must not replace the instance.
    ComboGui::RegisterComboTrackerWindow(gui);
    CTW_ASSERT(gui->GetGuiWindow(ComboGui::kComboTrackerWindowName) == window);

    // De-collision: no neighbour's name was disturbed or taken.
    for (int i = 0; i < kTrackerNeighbourCount; i++) {
        CTW_ASSERT(gui->GetGuiWindow(kTrackerNeighbourNames[i]) == neighbours[i]);
        CTW_ASSERT(gui->GetGuiWindow(kTrackerNeighbourNames[i]) != window);
    }

    // ---- Game-agnosticism tripwire ---------------------------------------
    const GameId prevGame = Context_GetCurrentGame();
    const GameId allGames[] = { GAME_OOT, GAME_MM, GAME_NONE };

    // Four model states: paired x adapters-present, both ways. State 3
    // (paired with both adapters UN-registered) is the corner no other lock
    // reaches — a crossing row whose HOST GAME cannot resolve a check name.
    for (int state = 0; state < 4; state++) {
        const bool paired = state >= 2;
        const bool adapters = (state % 2) == 0;

        ComboContext_Init();
        Combo_Crossings_Clear();
        if (adapters) {
            MM_TrackerAdapter_Register();
            OoT_TrackerAdapter_Register();
        } else {
            Combo_Tracker_RegisterMM(NULL);
            Combo_Tracker_RegisterOoT(NULL);
        }
        if (paired) {
            gComboCtx.sourceIsRando = true;
            gComboCtx.sharedRandoSeed = 0xC0FFEE97u;
            gComboCtx.sharedRandoSettingsHash = 0x5EED0497u;
            SharedItem item;
            CTW_ASSERT(TestNamedItem((uint8_t)GAME_OOT, "Lens of Truth", &item));
            CTW_ASSERT(Combo_SetForeignPlacement(0x0401, item) >= 0);
            CTW_ASSERT(Combo_TrackerForeignCount((uint8_t)GAME_MM) == 1);
            // ...and a crossing-store row in the other direction, the single
            // bag's only source (#755).
            SharedItem mmItem;
            CTW_ASSERT(TestNamedItem((uint8_t)GAME_MM, "Lens of Truth", &mmItem));
            ComboCrossing ootHosted;
            ootHosted.hostCheck = 0x0123;
            ootHosted.itemClass = 0x0001;
            ootHosted.item = mmItem;
            CTW_ASSERT(Combo_Crossings_Replace(&ootHosted, 1, nullptr, 0) == 1);
            CTW_ASSERT(Combo_TrackerForeignCount((uint8_t)GAME_OOT) == 1);
        } else {
            CTW_ASSERT(Combo_TrackerForeignCount((uint8_t)GAME_MM) == 0);
        }

        for (GameId game : allGames) {
            Context_SetCurrentGame(game);
            // Surviving these IS the assertion: an ungated path reaches
            // ImGui::Begin with no ImGui context and aborts the process.
            window->Draw();
            window->Update();
            // ...and the shut window never touched the model, so drive the
            // reads its open draw path would make (header point 3).
            CTW_ASSERT(TrackerDriveModelReads());
        }
    }

    // ---- 5. Layout and grouping decisions (#458 U4, #815, #816) ------------
    if (TrackerLayoutLocks() != TEST_PASS) {
        return TEST_FAIL;
    }

#ifdef RSBS_SOURCE_DIR
    // ---- 4. Drawn the SoH way (UI parity M8) -------------------------------
    // Appearance itself is judged from the UiSnapshot captures; what a source
    // scan can hold is the shape: the pane draws its notes, spacing and themed
    // headers through the combo_ui seam (whose SoH table is installed), passes
    // an open flag to ImGui::Begin and clears its visibility through
    // SetVisibility when closed (SoH's pane chrome), and prints no settings
    // digest and no TextDisabled/hand-spacing call.
    {
        CTW_ASSERT(ComboUi_IsInstalled());
        std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/src/common/ComboTrackerWindow.cpp", std::ios::binary);
        CTW_ASSERT(in.good());
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CTW_ASSERT(text.find("ComboUi_Get()") != std::string::npos);
        // #1130 (lane L1b), style guide section 10b: the Window type keeps the
        // close button (an open flag to ImGui::Begin, SetVisibility(false) when
        // closed); the Floating type passes none, as SoH's overlays and the
        // Combo Item Tracker do. Both through the decision section 5 locks.
        CTW_ASSERT(text.find("ComboTrackerChromeFor(keys, paused, pad)") != std::string::npos);
        CTW_ASSERT(text.find("chrome.closeButton ? &open : nullptr") != std::string::npos);
        CTW_ASSERT(text.find("SetVisibility(false)") != std::string::npos);
        CTW_ASSERT(text.find("ImGui::TextDisabled") == std::string::npos);
        CTW_ASSERT(text.find("ImGui::Spacing") == std::string::npos);
        CTW_ASSERT(text.find("digest") == std::string::npos);
        // Each styled element through the seam, not just a ComboUi_Get() helper
        // that nothing calls.
        CTW_ASSERT(text.find("Ui().NoteText(") != std::string::npos);
        CTW_ASSERT(text.find("Ui().SeparatorText(") != std::string::npos);
        CTW_ASSERT(text.find("Ui().PushTheme()") != std::string::npos);
        CTW_ASSERT(text.find("Ui().Spacer(") != std::string::npos);
        // Every status as a glyph, and none of the three ASCII marks it replaced.
        // Whole tokens: ICON_FA_SQUARE_O is also a substring of the other two.
        auto hasToken = [&text](const char* tok) {
            const size_t n = strlen(tok);
            for (size_t at = text.find(tok); at != std::string::npos; at = text.find(tok, at + 1)) {
                auto word = [](char ch) { return isalnum((unsigned char)ch) || ch == '_'; };
                const bool leftOk = at == 0 || !word(text[at - 1]);
                const bool rightOk = at + n >= text.size() || !word(text[at + n]);
                if (leftOk && rightOk) {
                    return true;
                }
            }
            return false;
        };
        CTW_ASSERT(hasToken("ICON_FA_CHECK_SQUARE_O"));
        CTW_ASSERT(hasToken("ICON_FA_MINUS_SQUARE_O"));
        CTW_ASSERT(hasToken("ICON_FA_SQUARE_O"));
        // One notation for a found state (#766 review): a crossing row leads its
        // check name with the same glyph, with no Yes/No "Collected" column, and
        // the section says "Crossings" as the notes and the spoiler JSON do.
        CTW_ASSERT(text.find("FoundGlyph(row.found)") != std::string::npos);
        // (SoH's "Collected" colour key, which the Checks lists read, is not a column.)
        CTW_ASSERT(text.find("TableSetupColumn(\"Collected\"") == std::string::npos);
        CTW_ASSERT(text.find("\"Cross-Game Placements\"") == std::string::npos);
        CTW_ASSERT(text.find("[x]") == std::string::npos);
        CTW_ASSERT(text.find("[s]") == std::string::npos);
        CTW_ASSERT(text.find("[ ]") == std::string::npos);
        // SoH's table shape (style guide section 10): 8x8 cells, both borders, a
        // header row, and no ScrollY inside a pane that scrolls as a whole.
        CTW_ASSERT(text.find("ImGuiStyleVar_CellPadding, ImVec2(8.0f, 8.0f)") != std::string::npos);
        CTW_ASSERT(text.find("ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV") != std::string::npos);
        CTW_ASSERT(text.find("TableHeadersRow()") != std::string::npos);
        CTW_ASSERT(text.find("ImGuiTableFlags_ScrollY") == std::string::npos);
        // Section headers carry no count (R-N4), and the pane's paired seed is
        // told apart from a game's own seed, which a paired file's panels do not
        // print (#816; the decision itself is section 5's ComboPanelShowsOwnSeed).
        CTW_ASSERT(text.find("(%d)") == std::string::npos);
        CTW_ASSERT(text.find("\"Paired Seed: %s\"") != std::string::npos);
        CTW_ASSERT(text.find("if (ComboPanelShowsOwnSeed(identity, summary))") != std::string::npos);
        // The Checks list draws what section 5 locks (#458 U4): the grouping
        // through ComboCollectCheckAreas and SoH's search box through the seam.
        // #1128: through the hide decision section 5 locks, with the pane's own
        // Show Hidden Items.
        CTW_ASSERT(text.find("ComboCollectVisibleCheckAreas(game, search, hide, areas)") != std::string::npos);
        CTW_ASSERT(text.find("Ui().SearchInput(") != std::string::npos);
        // SoH's area loop runs under FramePadding (4, 3)
        // (randomizer_check_tracker.cpp, DrawElement), which puts an area's
        // tree arrow level with its rows; the theme's larger padding pushes the
        // header past its own rows.
        if (text.find("PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f))") == std::string::npos) {
            printf("[TEST] FAIL: ComboTrackerWindow.cpp draws its area loop without SoH's FramePadding (4, 3)\n");
            return TEST_FAIL;
        }
        // The skip toggle (#458 U5): SoH's skip button through the seam, offered
        // only where the view says the row is skippable (the LIVE panel, a check
        // of the seed not found), flipping the row's own flag through the view's
        // one write, and with SoH's glyphs (DrawLocation: plus to unskip, times
        // to skip). ComboTrackerView locks what those calls do.
        CTW_ASSERT(text.find("Combo_TrackerSkipWritable(game)") != std::string::npos);
        CTW_ASSERT(text.find("Combo_TrackerRowSkippable(game, &row)") != std::string::npos);
        CTW_ASSERT(text.find("Combo_TrackerSetSkipped(game, row.checkId, !row.skipped)") != std::string::npos);
        CTW_ASSERT(text.find("Ui().IconButton(") != std::string::npos);
        CTW_ASSERT(text.find("row.skipped ? ICON_FA_PLUS : ICON_FA_TIMES") != std::string::npos);
        // The crossing table sizes its Item column and wraps through the locked
        // decisions (#815).
        CTW_ASSERT(text.find("ComboCrossingItemColumnWidth(columnsWidth, widestItemWord)") != std::string::npos);
        CTW_ASSERT(text.find("ComboBalancedWrapWidth(text, ImGui::GetContentRegionAvail().x") != std::string::npos);

        // #1128 (lane L1a): every SoH Check Tracker key the pane reads, read
        // under its own name with SoH's default, and none written: Show Hidden
        // Items is the pane's own state (ruling D12), seeded from SoH's key.
        for (const char* key : kCtwHideKeys) {
            if (text.find(std::string("\"") + key + "\"") == std::string::npos) {
                printf("[TEST] FAIL: ComboTrackerWindow.cpp never reads %s\n", key);
                return TEST_FAIL;
            }
        }
        const char* const kDefaultedReads[] = {
            "CVarGetInteger(\"gTrackers.CheckTracker.ShowHidden\", 0)",
            "CVarGetInteger(\"gTrackers.CheckTracker.AreaComplete.Hide\", 0)",
            "CVarGetInteger(\"gTrackers.CheckTracker.AreaIncomplete.Hide\", 0)",
            "CVarGetInteger(\"gTrackers.CheckTracker.HideFilteredAreas\", 1)",
            "CVarGetInteger(\"gTrackers.CheckTracker.HiddenItemsToggleVisible\", 1)",
            "CVarGetInteger(\"gTrackers.CheckTracker.ExpandCollapseButtonsVisible\", 0)",
            "CVarGetInteger(\"gTrackers.CheckTracker.SearchInputVisible\", 1)",
            "CVarGetInteger(\"gTrackers.CheckTracker.CheckTotalsVisible\", 1)",
            // #1129: SoH's reachability keys, with SoH's defaults.
            "CVarGetInteger(\"gTrackers.CheckTracker.EnableAvailableChecks\", 0)",
            "CVarGetInteger(\"gTrackers.CheckTracker.AvailableChecksToggleVisible\", 1)",
            "CVarGetInteger(\"gTrackers.CheckTracker.OnlyShowAvailable\", 0)",
        };
        for (const char* read : kDefaultedReads) {
            if (text.find(read) == std::string::npos) {
                printf("[TEST] FAIL: ComboTrackerWindow.cpp does not read %s\n", read);
                return TEST_FAIL;
            }
        }
        CTW_ASSERT(text.find("CVarSet") == std::string::npos); // clear-never-store: the pane writes no key
        // #1130 (lane L1b): SoH's chrome keys (DrawElement, BeginFloatWindows),
        // each read under its own name with SoH's default, and drawn as SoH
        // draws them: the floating gates on the active game's pause and the
        // first pad's held buttons (SoH's own ControlDeck read), the main
        // viewport, the background colour opaque while docked in the menu's
        // deck, a transparent border, rounding 4, and the font scale.
        const char* const kChromeReads[] = {
            "CVarGetInteger(\"gTrackers.CheckTracker.WindowType\", kComboTrackerWindowWindow)",
            "CVarGetInteger(\"gTrackers.CheckTracker.Draggable\", 1)",
            "CVarGetInteger(\"gTrackers.CheckTracker.ShowOnlyPaused\", 0)",
            "CVarGetInteger(\"gTrackers.CheckTracker.DisplayType\", kComboTrackerDisplayAlways)",
            "CVarGetInteger(\"gTrackers.CheckTracker.ComboButton1\", kComboTrackerComboButtonL)",
            "CVarGetInteger(\"gTrackers.CheckTracker.ComboButton2\", kComboTrackerComboButtonR)",
            "CVarGetColor(\"gTrackers.CheckTracker.BgColor.Value\", kSohBgDefault)",
            "CVarGetFloat(\"gTrackers.CheckTracker.FontSize\", 1.0f)",
            "Combo_ItemActiveGamePaused()",
            "->GetPads()",
            "ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID)",
            "\"Main - Deck\"",
            "ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f)",
            "ImGui::SetWindowFontScale(keys.fontSize)",
        };
        for (const char* read : kChromeReads) {
            if (text.find(read) == std::string::npos) {
                printf("[TEST] FAIL: ComboTrackerWindow.cpp does not draw its chrome through %s\n", read);
                return TEST_FAIL;
            }
        }
        // SoH's header controls, through the seam, and the totals line through
        // section 5's text.
        CTW_ASSERT(text.find("Ui().Checkbox(\"Show Hidden Items\"") != std::string::npos);
        // Ruling D12: each pane's Show Hidden Items is its own. The legs above
        // take `showHidden` as a parameter, so they would pass over one bool
        // or one key shared by both panes: the window keeps a PaneState per
        // game, the checkbox flips that pane's, and that pane's list reads it.
        CTW_ASSERT(text.find("static PaneState sPanes[") != std::string::npos);
        CTW_ASSERT(text.find("PaneState& pane = Pane(game);") != std::string::npos);
        CTW_ASSERT(text.find("Ui().Checkbox(\"Show Hidden Items\", &pane.showHidden") != std::string::npos);
        CTW_ASSERT(text.find("ComboReadCheckHideOptions(pane.showHidden)") != std::string::npos);
        // "No checks." is for a game (or a search) with none: when the hides
        // leave nothing, the list asks the unhidden grouping before the note,
        // and otherwise draws empty, as SoH's does.
        CTW_ASSERT(text.find("ComboCollectCheckAreas(game, search, unhidden);") != std::string::npos);
        CTW_ASSERT(text.find("Ui().Button(\"Expand All\"") != std::string::npos);
        CTW_ASSERT(text.find("Ui().Button(\"Collapse All\"") != std::string::npos);
        // #1129: the totals line with SoH's "N Available / " behind Enable Available Checks.
        CTW_ASSERT(text.find("ComboCheckTotalsText(summary, header.availableChecks)") != std::string::npos);
        // #1129 (lane L3): SoH's reachability glyph (DrawLocation), whole tokens,
        // through the decision section 5 locks; the area header's count through
        // its text; and Only Show Available Checks as each pane's own toggle, in
        // the shape Show Hidden Items has (ruling D12): the pane's state, seeded
        // from SoH's key, flipped by the pane's checkbox, read by that pane's list.
        CTW_ASSERT(hasToken("ICON_FA_UNLOCK") && hasToken("ICON_FA_LOCK"));
        CTW_ASSERT(text.find("available ? ICON_FA_UNLOCK : ICON_FA_LOCK") != std::string::npos);
        CTW_ASSERT(text.find("ComboAvailabilityGlyph(row)") != std::string::npos);
        CTW_ASSERT(text.find("ComboAreaTotalsText(area, availableChecks)") != std::string::npos);
        CTW_ASSERT(text.find("Ui().Checkbox(\"Only Show Available Checks\", &pane.onlyAvailable") != std::string::npos);
        CTW_ASSERT(text.find("hide.onlyAvailable = availableChecks && pane.onlyAvailable;") != std::string::npos);
        // SoH's list is always drawn: no default-closed "Checks" node over it.
        CTW_ASSERT(text.find("TreeNode(\"Checks\")") == std::string::npos);
        // Ruling D10: the Crossings section is cut from this window. Its table
        // (DrawCrossingList) stays in this file for the Cross-Game Spoiler, so
        // the scan reads the window's own DrawElement body.
        const size_t bodyAt = text.find("void ComboTrackerWindow::DrawElement()");
        CTW_ASSERT(bodyAt != std::string::npos);
        const size_t bodyEnd = text.find("\n}\n", bodyAt);
        CTW_ASSERT(bodyEnd != std::string::npos);
        const std::string body = text.substr(bodyAt, bodyEnd - bodyAt);
        if (body.find("DrawCrossingList") != std::string::npos || body.find("\"Crossings\"") != std::string::npos) {
            printf("[TEST] FAIL: ComboTrackerWindow::DrawElement still draws the Crossings section (ruling D10)\n");
            return TEST_FAIL;
        }
    }
    // #1166 (lane T1): OoT's adapter names a seen check's item through section
    // 5's rule (Combo_TrackerNameSeenItem), from the facts SoH's DrawLocation
    // reads: the seed's merchant and scrub hint settings, Mysterious Shuffle,
    // SoH's own IsMysteryShopItem (not a copy), CanBePurchased (SoH 2ba368e7e)
    // and the ice trap's override.
    {
        std::ifstream in(std::string(RSBS_SOURCE_DIR) +
                             "/games/oot/soh/Enhancements/randomizer/TrackerAdapterSingleExe.cpp",
                         std::ios::binary);
        CTW_ASSERT(in.good());
        const std::string oot((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const char* const kSeenReads[] = {
            "RSK_MERCHANT_TEXT_HINT", "RSK_SCRUB_TEXT_HINT",
            "\"MysteriousShuffle\"",  "CheckTracker::IsMysteryShopItem(",
            "->CanBePurchased()",     "overrides.find(",
            "GetTrickName()",         "Combo_TrackerNameSeenItem(&seen, out)",
        };
        for (const char* read : kSeenReads) {
            if (oot.find(read) == std::string::npos) {
                printf("[TEST] FAIL: OoT's tracker adapter does not name a seen item through %s\n", read);
                return TEST_FAIL;
            }
        }
        // #1161 (lane T1): both adapters implement the logic op from their own
        // tracker's source (OoT: SoH's GetConditionStr of the check in its parent
        // region; MM: 2Ship's accessLogicFuncs, with its enemy-drop omission),
        // and the window shows it as each row's tooltip behind SoH's Show Logic.
        const size_t opsAt = oot.find("kOps = {");
        const size_t opsEnd = opsAt == std::string::npos ? std::string::npos : oot.find("};", opsAt);
        const bool ootOp = opsEnd != std::string::npos &&
                           oot.substr(opsAt, opsEnd - opsAt).find("OoTTrackerCheckLogic") != std::string::npos &&
                           oot.find("GetConditionStr()") != std::string::npos &&
                           oot.find("GetParentRegionKey()") != std::string::npos;
        std::ifstream mmIn(std::string(RSBS_SOURCE_DIR) + "/games/mm/2s2h/Rando/TrackerAdapterSingleExe.cpp",
                           std::ios::binary);
        CTW_ASSERT(mmIn.good());
        const std::string mm((std::istreambuf_iterator<char>(mmIn)), std::istreambuf_iterator<char>());
        const bool mmOp = mm.find("desc.logicText = MMTrackerCheckLogic;") != std::string::npos &&
                          mm.find("accessLogicFuncs") != std::string::npos &&
                          mm.find("RC_ENEMY_DROP_WOLFOS") != std::string::npos;
        std::ifstream winIn(std::string(RSBS_SOURCE_DIR) + "/src/common/ComboTrackerWindow.cpp", std::ios::binary);
        CTW_ASSERT(winIn.good());
        const std::string win((std::istreambuf_iterator<char>(winIn)), std::istreambuf_iterator<char>());
        const bool winTip = win.find("CVarGetInteger(\"gTrackers.CheckTracker.ShowLogic\", 0)") != std::string::npos &&
                            win.find("ComboCheckLogicTooltip(game, row, showLogic)") != std::string::npos &&
                            win.find("Combo_TrackerCheckLogic(game, row.checkId)") != std::string::npos;
        printf("[TEST] combo-tracker-window #1161: logic op in OoT's adapter %d, in MM's %d, drawn by the window %d\n",
               (int)ootOp, (int)mmOp, (int)winTip);
        CTW_ASSERT(ootOp && mmOp && winTip);
    }
#endif

    Context_SetCurrentGame(prevGame);

    // Leave global state clean: adapters registered (the production state),
    // combo context reset, CVar cleared.
    MM_TrackerAdapter_Register();
    OoT_TrackerAdapter_Register();
    CVarClear(ComboGui::kComboTrackerVisibilityCVar);
    ComboContext_Init();
    Combo_Crossings_Clear();

    printf("[TEST] PASS: tracker window registers de-collided and idempotently, and its draw path is inert under "
           "GAME_OOT/GAME_MM/GAME_NONE with adapters present, absent, and a paired world\n");
    return TEST_PASS;
}
