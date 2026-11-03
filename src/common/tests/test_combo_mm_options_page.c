/**
 * @file test_combo_mm_options_page.c
 * @brief ROM-free lock for the view model of Combo > MM Randomizer and Combo >
 *        MM Tricks (src/common/combo_mm_options_page.h; ADR 0004's 2026-09-27
 *        host amendment). It replaces the retired pop-out window's lock
 *        (test_combo_mm_options_window.c), keeping every leg that was about the
 *        MODEL and the writers and dropping the ones that were about a window.
 *
 * The descriptor tables have their own locks, MM-side (mm_rando_options_test.cpp,
 * mm_trick_table_test.cpp), because that is the only place both tables are in
 * scope. The registered menu rows have theirs OoT-side (MenuMmRandomizerPages),
 * because that is where SohMenu is. This one is the model between them:
 *
 * 1. THE BRING-UP IS HEADLESS-SAFE AND PUBLISHES BOTH TABLES.
 *    Combo_MMOptionsPages_Init() needs no Gui and no window, and afterwards both
 *    descriptor tables are registered; a second call changes nothing. The page
 *    registrar depends on this: the menu is built during OoT's start-up, before
 *    rsbs/src/main.cpp's own call.
 *
 * 2. EVERY OPTION GROUP HAS A COLUMN, and both columns hold rows. A group with
 *    no column would register rows past the page's two columns, which
 *    Menu::DrawElement never draws (#640's failure mode with the halves swapped).
 *    The columns read SoH's headers in SoH's order (#1101, option C): Logic,
 *    Win Condition, Area Access and Shuffle Items on the left; Shuffle Shops &
 *    Merchants, Additional Items, Traps, Hints and Starting Items on the right.
 *
 * 3. THE ROW STATES ARE THE TABLES' AND THE FREEZE'S, in both directions. An
 *    option is BLOCKED exactly when its liveness is PARTIAL or DORMANT, with the
 *    table's own reason; a trick is BLOCKED exactly when it is reserved or
 *    unbound, with its reason; everything else is LIVE with no reason. Once a
 *    creation event stamps the profile, EVERY row is FROZEN with "Already
 *    Decided" and never a capability reason (ADR 0004 section 6). There must be
 *    at least one live and one blocked trick, or the split is untested.
 *
 * 4. THE NOTES SAY WHICH STATE THE PAGE IS IN. Unpaired, legacy-paired and
 *    frozen each get their own sentence; unpaired and frozen are the one pair of
 *    sentences every randomizer page of ours shares (#1101 decision 7: "These
 *    apply to the next world you generate. Loading a paired file shows its
 *    own." and "Already decided ... (fingerprint XXXXXXXX)"), on the options
 *    page and the tricks page alike; the suspended note shows exactly when
 *    the profile is editable and Majora's Mask is not the running game; the
 *    orange creation-lock warning is gone (the gray note says it); the tricks
 *    headline counts the settable tricks, and the freeze replaces it.
 *
 * 5. RESET CLEARS OPTIONS AND TRICKS, and a frozen profile refuses it.
 *
 * 6. THE FROZEN-PROFILE WRITE GATE (#498/#564 V5), unchanged from the window's
 *    lock: once a creation event has stamped gComboCtx.mmProfileDigest, the two
 *    option writers REJECT. The rows are the gate's face, never the gate.
 *
 * 7. THE combo_ui SEAM IS SoH's. The trick rows still draw their chips and
 *    names through it (and the Combo Tracker and Cross-Game Spoiler panes
 *    everything), so the shipped binary must install SoH's table; the disabled
 *    tooltip keeps SoH's shape; no player-facing reason carries a tracker number;
 *    every reason is a Title Case fragment (R-S2).
 *
 * 8. THE WINDOW IS GONE and the model reads no menu-index key. The retired
 *    window's source is absent from the tree, and neither the model's TU nor the
 *    seam's fallback names a gSettings.Menu.* key (ADR 0004 section 4.1a(ii)'s
 *    invariant, kept for the model after the move).
 *
 * 9. A PRESET CARRIES THE WHOLE PAIRED PROFILE (#1368), through the writers
 *    these pages use. A captured profile (MM options, trap types, tricks,
 *    excluded checks and the Cross-Game rules), written out as text and read
 *    back, restores a bound MM trick and the Cross-Game rules, [Both Games]
 *    Logic among them (#1367), over a scrambled profile; MM's own Logic, which
 *    that rule sets, is never captured and is refused; overwrite clears what it
 *    does not hold, merge keeps it; a
 *    reserved or unbound trick and an out-of-space rule are refused (and a
 *    stale unbound key goes back to never chosen), while the rest applies; a
 *    key that is not a randomizer setting is never touched; a decided world
 *    (either freeze) refuses the profile whole, a reset included; and the
 *    shipped "Rando Seed Settings - Reset to Default" preset resets every MM
 *    and Cross-Game key beside gRandoSettings.
 *
 * Deliberately absent: appearance. That is judged from pixels by the UiSnapshot
 * row's captures of the two pages (docs/ui-style-guide.md section 12).
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++).
 * Entry point is a RunHeadless bridge because Combo_MMOptionsPages_Init brings
 * up the tier-4 settings window's model, which needs the shared bring-up.
 */

#include "../combo_mm_locations_view.h"
#include "../combo_mm_options_page.h"
#include "../combo_mm_options_view.h"
#include "../combo_mm_tricks_view.h"
#include "../combo_preset_profile.h"
#include "../combo_settings_view.h" // COMBO_LOGIC_SET_BY_COMBO_REASON (#1367)
#include "../combo_ui.h"
#include "../context.h"
#include "../foreign_items.h"
#include "../test_runner.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>

#define CMOP_ASSERT(cond)                                                  \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return TEST_FAIL;                                              \
        }                                                                  \
    } while (0)

namespace {

void CmopAuthorPairing(bool frozen) {
    ComboContext_Init();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSeed = 0xC0FFEE97u;
    gComboCtx.sharedRandoSettingsHash = 0x5EED0497u;
    gComboCtx.mmProfileDigest = frozen ? 0x4D4D0001u : 0u;
}

/** The value a preset profile holds for a dotted CVar name, or nullptr. */
const nlohmann::json* CmopProfileAt(const nlohmann::json& profile, const std::string& cvar) {
    const nlohmann::json* node = &profile;
    size_t start = 0;
    while (start <= cvar.size()) {
        const size_t dot = cvar.find('.', start);
        const std::string part = cvar.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
        if (!node->is_object() || !node->contains(part)) {
            return nullptr;
        }
        node = &(*node)[part];
        if (dot == std::string::npos) {
            break;
        }
        start = dot + 1;
    }
    return node;
}

/** Put @p value at a dotted CVar name in a preset profile, as a hand-written
 *  preset would. */
void CmopProfileSet(nlohmann::json& profile, const std::string& cvar, const nlohmann::json& value) {
    nlohmann::json* node = &profile;
    size_t start = 0;
    while (true) {
        const size_t dot = cvar.find('.', start);
        node = &(*node)[cvar.substr(start, dot == std::string::npos ? std::string::npos : dot - start)];
        if (dot == std::string::npos) {
            break;
        }
        start = dot + 1;
    }
    *node = value;
}

/** Every paired-profile key back to "never chosen". */
void CmopClearPairedProfile() {
    Combo_MMOptionsPage_ResetAll();
    Combo_MMLocationsClearExcluded();
    for (int id = 0; id < (int)COMBO_SETTING_COUNT; id++) {
        Combo_ComboSettingClear((ComboSettingId)id);
    }
}

bool CmopContains(const char* text, const char* needle) {
    return text != NULL && strstr(text, needle) != NULL;
}

} // namespace

extern "C" int Combo_MMOptionsPage_RunHeadless(void) {
    printf("[TEST] combo-mm-options-page: the MM options pages' model publishes both tables headlessly, gives every "
           "group a column and every row its table's state, and keeps the freeze gate on the writers (#497)\n");

    const GameId prevGame = Context_GetCurrentGame();

    // ---- 1. Headless bring-up publishes both tables, idempotently ----------
    Combo_MMOptionsPages_Init();
    CMOP_ASSERT(Combo_MMOptionCount() > 0);
    CMOP_ASSERT(Combo_MMTrickCount() > 0);
    const ComboMMOptionDesc* firstOption = Combo_MMOptionAt(0);
    const ComboMMTrickDesc* firstTrick = Combo_MMTrickAt(0);
    const int optionCount = Combo_MMOptionCount();
    const int trickCount = Combo_MMTrickCount();
    Combo_MMOptionsPages_Init();
    CMOP_ASSERT(Combo_MMOptionAt(0) == firstOption);
    CMOP_ASSERT(Combo_MMTrickAt(0) == firstTrick);
    CMOP_ASSERT(Combo_MMOptionCount() == optionCount);
    CMOP_ASSERT(Combo_MMTrickCount() == trickCount);
    CMOP_ASSERT(Combo_MMOptionAt(optionCount) == NULL);

    // ---- 2. Every group has a column, both columns hold rows ---------------
    {
        int rowsInColumn[COMBO_MM_OPTIONS_PAGE_COLUMNS] = { 0, 0 };
        for (int i = 0; i < optionCount; i++) {
            const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
            CMOP_ASSERT(d != NULL);
            const int column = Combo_MMOptionsPage_GroupColumn(d->group);
            if (column < 0 || column >= COMBO_MM_OPTIONS_PAGE_COLUMNS) {
                printf("[TEST] FAIL: option %s's group %u has column %d, outside the page's %d; its row would be "
                       "registered and never drawn\n",
                       d->name, (unsigned)d->group, column, COMBO_MM_OPTIONS_PAGE_COLUMNS);
                return TEST_FAIL;
            }
            rowsInColumn[column]++;
        }
        for (int c = 0; c < COMBO_MM_OPTIONS_PAGE_COLUMNS; c++) {
            CMOP_ASSERT(rowsInColumn[c] > 0);
        }
        CMOP_ASSERT(Combo_MMOptionsPage_GroupColumn((uint8_t)COMBO_MM_GROUP_COUNT) == -1);
        // SoH's headers, by name, in reading order: down the left column, then
        // down the right (#1101, option C).
        static const char* const kLeft[] = { "Logic", "Win Condition", "Area Access", "Shuffle Items" };
        static const char* const kRight[] = { "Shuffle Shops & Merchants", "Additional Items", "Traps", "Hints",
                                              "Starting Items" };
        const int left = (int)(sizeof(kLeft) / sizeof(kLeft[0]));
        const int right = (int)(sizeof(kRight) / sizeof(kRight[0]));
        if ((int)COMBO_MM_GROUP_COUNT != left + right) {
            printf("[TEST] FAIL: the options page has %d groups, SoH's vocabulary %d\n", (int)COMBO_MM_GROUP_COUNT,
                   left + right);
            return TEST_FAIL;
        }
        for (int g = 0; g < (int)COMBO_MM_GROUP_COUNT; g++) {
            const char* want = g < left ? kLeft[g] : kRight[g - left];
            const int wantColumn = g < left ? 0 : 1;
            if (strcmp(Combo_MMOptionGroupName((uint8_t)g), want) != 0 ||
                Combo_MMOptionsPage_GroupColumn((uint8_t)g) != wantColumn) {
                printf("[TEST] FAIL: group %d is '%s' in column %d; SoH's vocabulary puts '%s' in column %d\n", g,
                       Combo_MMOptionGroupName((uint8_t)g), Combo_MMOptionsPage_GroupColumn((uint8_t)g), want,
                       wantColumn);
                return TEST_FAIL;
            }
        }
        printf("[TEST] leg 2: every option group has a column (%d rows in column 1, %d in column 2)\n", rowsInColumn[0],
               rowsInColumn[1]);
    }

    // ---- 3. Row states, both directions, and the freeze --------------------
    {
        ComboContext_Init();
        CMOP_ASSERT(!Combo_MMProfileFrozen());
        int setByCombo = 0;
        for (int i = 0; i < optionCount; i++) {
            const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
            const char* reason = NULL;
            const ComboMMRowState state = Combo_MMOptionsPage_OptionState(d, &reason);
            const bool gated = d->liveness == COMBO_MM_LIVENESS_PARTIAL || d->liveness == COMBO_MM_LIVENESS_DORMANT;
            CMOP_ASSERT(reason != NULL);
            // #1367: MM's Logic follows [Both Games] Logic, read-only with the
            // rule as its reason; it is no capability block.
            if (strcmp(d->name, COMBO_MM_OPTION_NAME_LOGIC) == 0) {
                setByCombo++;
                if (!Combo_MMOptionsPage_OptionSetByCombo(d) || state != COMBO_MM_ROW_SET_BY_COMBO ||
                    strcmp(reason, COMBO_LOGIC_SET_BY_COMBO_REASON) != 0) {
                    printf("[TEST] FAIL: MM's Logic row has page state %d with reason \"%s\", expected SET_BY_COMBO "
                           "with \"%s\" (#1367)\n",
                           (int)state, reason, COMBO_LOGIC_SET_BY_COMBO_REASON);
                    return TEST_FAIL;
                }
                continue;
            }
            CMOP_ASSERT(!Combo_MMOptionsPage_OptionSetByCombo(d));
            if (gated != (state == COMBO_MM_ROW_BLOCKED) || (!gated && state != COMBO_MM_ROW_LIVE)) {
                printf("[TEST] FAIL: option %s (liveness %u) has page state %d\n", d->name, (unsigned)d->liveness,
                       (int)state);
                return TEST_FAIL;
            }
            CMOP_ASSERT(gated ? strcmp(reason, d->disabledReason) == 0 && reason[0] != '\0' : reason[0] == '\0');
        }
        CMOP_ASSERT(setByCombo == 1);
        int live = 0;
        int blockedReserved = 0;
        int blockedUnbound = 0;
        for (int i = 0; i < trickCount; i++) {
            const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
            const char* reason = NULL;
            const ComboMMRowState state = Combo_MMOptionsPage_TrickState(d, &reason);
            const bool gated = d->reserved || !d->bound;
            if (gated != (state == COMBO_MM_ROW_BLOCKED) || (!gated && state != COMBO_MM_ROW_LIVE)) {
                printf("[TEST] FAIL: trick %s (reserved %d, bound %d) has page state %d\n", d->name,
                       d->reserved ? 1 : 0, d->bound ? 1 : 0, (int)state);
                return TEST_FAIL;
            }
            CMOP_ASSERT(gated ? strcmp(reason, d->disabledReason) == 0 && reason[0] != '\0' : reason[0] == '\0');
            live += gated ? 0 : 1;
            blockedReserved += d->reserved ? 1 : 0;
            blockedUnbound += (!d->reserved && !d->bound) ? 1 : 0;
        }
        CMOP_ASSERT(live == Combo_MMOptionsPage_SettableTrickCount());
        CMOP_ASSERT(live > 0);
        CMOP_ASSERT(blockedReserved > 0);
        CMOP_ASSERT(blockedUnbound > 0);

        CmopAuthorPairing(true);
        CMOP_ASSERT(Combo_MMProfileFrozen());
        for (int i = 0; i < optionCount; i++) {
            const char* reason = NULL;
            if (Combo_MMOptionsPage_OptionState(Combo_MMOptionAt(i), &reason) != COMBO_MM_ROW_FROZEN ||
                strcmp(reason, COMBO_MM_OPTIONS_FROZEN_REASON) != 0) {
                printf("[TEST] FAIL: option %s is not FROZEN with \"%s\" after the creation stamp\n",
                       Combo_MMOptionAt(i)->name, COMBO_MM_OPTIONS_FROZEN_REASON);
                return TEST_FAIL;
            }
        }
        for (int i = 0; i < trickCount; i++) {
            const char* reason = NULL;
            if (Combo_MMOptionsPage_TrickState(Combo_MMTrickAt(i), &reason) != COMBO_MM_ROW_FROZEN ||
                strcmp(reason, COMBO_MM_OPTIONS_FROZEN_REASON) != 0) {
                printf("[TEST] FAIL: trick %s is not FROZEN with \"%s\" after the creation stamp\n",
                       Combo_MMTrickAt(i)->name, COMBO_MM_OPTIONS_FROZEN_REASON);
                return TEST_FAIL;
            }
        }
        ComboContext_Init();
        printf("[TEST] leg 3: %d options and %d tricks take their table's state (%d live tricks, %d reserved, %d "
               "unbound), and all of them freeze with the stamp\n",
               optionCount, trickCount, live, blockedReserved, blockedUnbound);
    }

    // ---- 4. The notes name the state ----------------------------------------
    {
        char tricks[256];
        ComboContext_Init();
        Context_SetCurrentGame(GAME_OOT);
        // The shared sentence, spelled here independently of the model.
        static const char* const kNextWorld =
            "These apply to the next world you generate. Loading a paired file shows its own.";
        CMOP_ASSERT(strcmp(Combo_MMOptionsPage_StatusNote(), kNextWorld) == 0);
        CMOP_ASSERT(Combo_MMOptionsPage_Suspended());
        CMOP_ASSERT(Combo_MMOptionsPage_FallbackWarning() != NULL);
        char expected[64];
        snprintf(expected, sizeof(expected), "%d of %d tricks", Combo_MMOptionsPage_SettableTrickCount(), trickCount);
        // The tricks page's note IS the shared sentence, nothing after it: the
        // sentence is already two sentences, a note's whole budget (UiSnapshot's
        // R6), so the trick headline is a note row of its own, which
        // MenuMmRandomizerPages leg 7 holds.
        CMOP_ASSERT(strcmp(Combo_MMOptionsPage_TricksNote(tricks, sizeof(tricks)), kNextWorld) == 0);
        CMOP_ASSERT(!CmopContains(tricks, expected));

        Context_SetCurrentGame(GAME_MM);
        CMOP_ASSERT(!Combo_MMOptionsPage_Suspended());

        CmopAuthorPairing(false);
        CMOP_ASSERT(CmopContains(Combo_MMOptionsPage_StatusNote(), "predates saved Majora's Mask options"));

        CmopAuthorPairing(true);
        Context_SetCurrentGame(GAME_OOT);
        // The frozen sentence names the world's fingerprint, as the Cross-Game
        // page's does: one sentence for the three pages (#1101 decision 7).
        gComboCtx.comboSettingsHash = 0xC0DE1101u;
        static const char* const kFrozen = "Already decided when this world was created (fingerprint C0DE1101). "
                                           "Return to the title screen to choose settings for a new world.";
        CMOP_ASSERT(strcmp(Combo_MMOptionsPage_StatusNote(), kFrozen) == 0);
        CMOP_ASSERT(!Combo_MMOptionsPage_Suspended());
        CMOP_ASSERT(strcmp(Combo_MMOptionsPage_TricksNote(tricks, sizeof(tricks)), kFrozen) == 0);
        CMOP_ASSERT(!CmopContains(tricks, expected));
        // A frozen profile with no fingerprint yet (a legacy pair's rules) says
        // the same without the parenthesis rather than printing 00000000.
        gComboCtx.comboSettingsHash = 0u;
        CMOP_ASSERT(strcmp(Combo_MMOptionsPage_StatusNote(),
                           "Already decided when this world was created. Return to the title screen to choose "
                           "settings for a new world.") == 0);
        ComboContext_Init();
        Context_SetCurrentGame(prevGame);
        printf("[TEST] leg 4: the status, suspended, warning and tricks notes each follow the state\n");
    }

    // ---- 5. Reset clears options and tricks; frozen refuses ---------------
    {
        const ComboMMOptionDesc* option = Combo_MMOptionAt(0);
        const ComboMMTrickDesc* trick = NULL;
        for (int i = 0; i < trickCount && trick == NULL; i++) {
            const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
            if (d->bound && !d->reserved) {
                trick = d;
            }
        }
        CMOP_ASSERT(trick != NULL);
        ComboContext_Init();
        Combo_MMOptionSetValue(option, Combo_MMOptionGetValue(option));
        Combo_MMTrickSetValue(trick, true);
        CMOP_ASSERT(Combo_MMOptionIsExplicit(option));
        CMOP_ASSERT(Combo_MMTrickGetValue(trick));

        CmopAuthorPairing(true);
        Combo_MMOptionsPage_ResetAll();
        CMOP_ASSERT(Combo_MMOptionIsExplicit(option));
        CMOP_ASSERT(Combo_MMTrickGetValue(trick));

        ComboContext_Init();
        Combo_MMOptionsPage_ResetAll();
        CMOP_ASSERT(!Combo_MMOptionIsExplicit(option));
        CMOP_ASSERT(!Combo_CVarIsExplicitInt(trick->cvar));
        printf("[TEST] leg 5: Reset clears an option and a trick, and a frozen profile refuses it\n");
    }

    // ---- 6. Frozen-profile write gate (#498/#564 V5) ------------------------
    {
        // Any checkbox row will do: 0/1 are always legal values for it, so a
        // rejected write is distinguishable from a clamped one.
        const ComboMMOptionDesc* checkbox = NULL;
        for (int i = 0; i < optionCount; i++) {
            const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
            if (d != NULL && d->widget == COMBO_MM_WIDGET_CHECKBOX) {
                checkbox = d;
                break;
            }
        }
        CMOP_ASSERT(checkbox != NULL);

        ComboContext_Init();
        CMOP_ASSERT(!Combo_MMProfileFrozen());
        Combo_MMOptionClear(checkbox);
        Combo_MMOptionSetValue(checkbox, 1);
        CMOP_ASSERT(Combo_MMOptionGetValue(checkbox) == 1);
        CMOP_ASSERT(Combo_MMOptionIsExplicit(checkbox));

        gComboCtx.mmProfileDigest = 0x4D4D0001u;
        CMOP_ASSERT(Combo_MMProfileFrozen());
        Combo_MMOptionSetValue(checkbox, 0);
        CMOP_ASSERT(Combo_MMOptionGetValue(checkbox) == 1);
        Combo_MMOptionClear(checkbox);
        CMOP_ASSERT(Combo_MMOptionIsExplicit(checkbox));
        CMOP_ASSERT(Combo_MMOptionGetValue(checkbox) == 1);

        gComboCtx.mmProfileDigest = 0;
        CMOP_ASSERT(!Combo_MMProfileFrozen());
        Combo_MMOptionClear(checkbox);
        CMOP_ASSERT(!Combo_MMOptionIsExplicit(checkbox));
    }

    // ---- 7. The combo_ui seam (UI parity M6) --------------------------------
    {
        CMOP_ASSERT(ComboUi_IsInstalled());
        CMOP_ASSERT(ComboUi_Get() != NULL);
        CMOP_ASSERT(ComboUi_Get()->TagChip != NULL);
        CMOP_ASSERT(ComboUi_Get()->RowText != NULL);

        CMOP_ASSERT(strcmp(ComboUi_DisabledTooltip("Already Decided", NULL),
                           "This setting is disabled because: \n\n- Already Decided") == 0);
        CMOP_ASSERT(strcmp(ComboUi_DisabledTooltip("Already Decided", "Retired"),
                           "This setting is disabled because: \n\n- Already Decided\n- Retired") == 0);
        CMOP_ASSERT(ComboUi_DisabledTooltip(NULL, "")[0] == '\0');
        CMOP_ASSERT(ComboUi_DisabledTooltip("Already Decided", NULL) ==
                    ComboUi_DisabledTooltip(NULL, "Already Decided"));

        ComboUiWidgetOpts o;
        o.tooltip = "description";
        o.disabled = true;
        o.disabledTooltip = "reason";
        CMOP_ASSERT(strcmp(ComboUi_ShownTooltip(&o), "reason") == 0);
        o.disabled = false;
        CMOP_ASSERT(strcmp(ComboUi_ShownTooltip(&o), "description") == 0);
        o.tooltip = "";
        CMOP_ASSERT(ComboUi_ShownTooltip(&o) == NULL);

        // No player-facing text in either table carries a tracker number or an
        // ADR reference (docs/ui-style-guide.md R-N4, R-TT5).
        auto cites = [](const char* text) {
            if (text == NULL) {
                return false;
            }
            for (const char* c = text; *c != '\0'; c++) {
                if (c[0] == '#' && c[1] >= '0' && c[1] <= '9') {
                    return true;
                }
                if (strncmp(c, "ADR ", 4) == 0) {
                    return true;
                }
            }
            return false;
        };
        for (int i = 0; i < optionCount; i++) {
            const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
            if (cites(d->label) || cites(d->tooltip) || cites(d->disabledReason)) {
                printf("[TEST] FAIL: MM option %s cites a tracker or ADR in player-facing text\n", d->name);
                return TEST_FAIL;
            }
        }
        for (int i = 0; i < trickCount; i++) {
            const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
            if (cites(d->label) || cites(d->tooltip) || cites(d->disabledReason)) {
                printf("[TEST] FAIL: MM trick %s cites a tracker or ADR in player-facing text\n", d->name);
                return TEST_FAIL;
            }
        }
        {
            char tricks[256];
            ComboContext_Init();
            const char* notes[] = { Combo_MMOptionsPage_StatusNote(), Combo_MMOptionsPage_SuspendedNote(),
                                    Combo_MMOptionsPage_FallbackWarning(),
                                    Combo_MMOptionsPage_TricksNote(tricks, sizeof(tricks)) };
            for (const char* note : notes) {
                CMOP_ASSERT(!cites(note));
            }
        }

        // Every reason in a disabled tooltip is a fragment in SoH's disabledMap
        // style (R-S2: "Save Not Loaded"): upper case first, every word of four
        // or more letters capitalised, no colon, no closing punctuation.
        auto fragment = [](const char* text) {
            if (text == NULL || text[0] == '\0') {
                return true;
            }
            if (!(text[0] >= 'A' && text[0] <= 'Z')) {
                return false;
            }
            const size_t n = strlen(text);
            if (text[n - 1] == '.' || text[n - 1] == '!' || text[n - 1] == '?' || strchr(text, ':') != NULL) {
                return false;
            }
            for (const char* w = text; *w != '\0';) {
                while (*w == ' ') {
                    w++;
                }
                const char* e = w;
                while (*e != '\0' && *e != ' ') {
                    e++;
                }
                if (e - w >= 4 && *w >= 'a' && *w <= 'z') {
                    return false;
                }
                w = e;
            }
            return true;
        };
        CMOP_ASSERT(fragment(COMBO_MM_OPTIONS_FROZEN_REASON));
        for (int i = 0; i < optionCount; i++) {
            const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
            if (!fragment(d->disabledReason)) {
                printf("[TEST] FAIL: MM option %s's disabled reason '%s' is not a Title Case fragment (R-S2)\n",
                       d->name, d->disabledReason);
                return TEST_FAIL;
            }
        }
        for (int i = 0; i < trickCount; i++) {
            const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
            if (!fragment(d->disabledReason)) {
                printf("[TEST] FAIL: MM trick %s's disabled reason '%s' is not a Title Case fragment (R-S2)\n", d->name,
                       d->disabledReason);
                return TEST_FAIL;
            }
        }
    }

    // ---- 8. The window is gone; the model reads no menu-index key ----------
#ifdef RSBS_SOURCE_DIR
    {
        for (const char* rel : { "src/common/ComboMmOptionsWindow.cpp", "src/common/ComboMmOptionsWindow.h" }) {
            std::ifstream retired(std::string(RSBS_SOURCE_DIR) + "/" + rel, std::ios::binary);
            if (retired.good()) {
                printf("[TEST] FAIL: %s is back: MM's randomizer options are Combo pages, and a second surface over "
                       "the same keys is a way for the two to disagree\n",
                       rel);
                return TEST_FAIL;
            }
        }
        for (const char* rel : { "src/common/combo_mm_options_page.c", "src/common/combo_ui.cpp" }) {
            std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + rel, std::ios::binary);
            CMOP_ASSERT(in.good());
            const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            CMOP_ASSERT(!text.empty());
            CMOP_ASSERT(text.find("gSettings.Menu") == std::string::npos);
            CMOP_ASSERT(text.find("CVAR_SETTING(\"Menu") == std::string::npos);
        }
        printf("[TEST] leg 8: the retired window's source is absent, and the model reads no menu-index key\n");
    }
#endif

    // ---- 9. A preset carries the whole paired profile (#1368) ---------------
    {
        // One row of each kind, from the real tables.
        const ComboMMOptionDesc* option = NULL;  // a checkbox the page lets a player set
        const ComboMMOptionDesc* mmLogic = NULL; // #1367: read-only, set by [Both Games] Logic
        for (int i = 0; i < optionCount; i++) {
            const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
            if (Combo_MMOptionsPage_OptionSetByCombo(d)) {
                mmLogic = mmLogic != NULL ? mmLogic : d;
            } else if (option == NULL && d->widget == COMBO_MM_WIDGET_CHECKBOX &&
                       d->liveness != COMBO_MM_LIVENESS_PARTIAL && d->liveness != COMBO_MM_LIVENESS_DORMANT) {
                option = d;
            }
        }
        CMOP_ASSERT(mmLogic != NULL);
        const ComboMMTrickDesc* bound = NULL;
        const ComboMMTrickDesc* unbound = NULL;
        const ComboMMTrickDesc* reserved = NULL;
        for (int i = 0; i < trickCount; i++) {
            const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
            if (d->reserved) {
                reserved = reserved != NULL ? reserved : d;
            } else if (!d->bound) {
                unbound = unbound != NULL ? unbound : d;
            } else {
                bound = bound != NULL ? bound : d;
            }
        }
        CMOP_ASSERT(option != NULL && bound != NULL && unbound != NULL && reserved != NULL);
        CMOP_ASSERT(Combo_MMTrapTypeCount() > 0);
        const ComboMMTrapTypeDesc* trap = Combo_MMTrapTypeAt(0);
        CMOP_ASSERT(Combo_MMLocationCount() > 0);
        const uint16_t check = Combo_MMLocationAt(0)->id;
        // A key under MM's own root that is not a randomizer setting (the check
        // tracker's namespace): no preset may touch it.
        static const char* const kProbeKey = "gRando.CheckTracker.RsbsPresetProbe";

        // 9a. The round trip: author, capture through text, scramble, apply.
        ComboContext_Init();
        CMOP_ASSERT(ComboPresetProfile_RefusalNote().empty());
        CmopClearPairedProfile();
        const int32_t chosen = Combo_MMOptionGetValue(option) != 0 ? 0 : 1; // away from the default
        Combo_MMOptionSetValue(option, chosen);
        Combo_MMTrickSetValue(bound, true);
        CMOP_ASSERT(Combo_MMTrapTypeSetValue(trap, true));
        CMOP_ASSERT(Combo_MMLocationSetExcluded(check, true));
        CMOP_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_SHARED_LENS, 1) == 1);
        // #1367: the pair's Logic is a Cross-Game rule the preset carries; MM's
        // own Logic key, which that rule sets, is never carried.
        CMOP_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_LOGIC, (int32_t)RSBS_COMBO_RUNG_ALL_REACHABLE) == 1);
        Combo_MMOptionSetValue(mmLogic, Combo_MMOptionGetValue(mmLogic) != 0 ? 0 : 1);
        CMOP_ASSERT(Combo_MMOptionIsExplicit(mmLogic));
        CVarSetInteger(kProbeKey, 7);
        const nlohmann::json saved = nlohmann::json::parse(ComboPresetProfile_Capture().dump(4));
        CMOP_ASSERT(saved.is_object());
        // What was chosen, and only that: no unchosen rule, no trick left off.
        CMOP_ASSERT(CmopProfileAt(saved, bound->cvar) != nullptr);
        CMOP_ASSERT(CmopProfileAt(saved, unbound->cvar) == nullptr);
        CMOP_ASSERT(CmopProfileAt(saved, Combo_ComboSettingKey(COMBO_SETTING_SHARED_LENS)) != nullptr);
        CMOP_ASSERT(CmopProfileAt(saved, Combo_ComboSettingKey(COMBO_SETTING_SHARED_SHIELDS)) == nullptr);
        CMOP_ASSERT(CmopProfileAt(saved, Combo_ComboSettingKey(COMBO_SETTING_LOGIC)) != nullptr);
        CMOP_ASSERT(CmopProfileAt(saved, mmLogic->cvar) == nullptr);
        CMOP_ASSERT(CmopProfileAt(saved, kProbeKey) == nullptr);

        CmopClearPairedProfile();
        CVarSetInteger(kProbeKey, 9);
        CMOP_ASSERT(!Combo_MMTrickGetValue(bound) && !Combo_ComboSettingIsExplicit(COMBO_SETTING_SHARED_LENS));
        CMOP_ASSERT(!Combo_ComboSettingIsExplicit(COMBO_SETTING_LOGIC));
        ComboPresetProfileReport report = ComboPresetProfile_Apply(saved, false);
        CMOP_ASSERT(report.applied && report.refused == 0 && report.ignored == 0 && report.written == 6);
        CMOP_ASSERT(Combo_MMOptionIsExplicit(option) && Combo_MMOptionGetValue(option) == chosen);
        CMOP_ASSERT(Combo_MMTrickGetValue(bound));
        CMOP_ASSERT(Combo_MMTrapTypeGetValue(trap));
        CMOP_ASSERT(Combo_MMLocationIsExcluded(check));
        int32_t lens = 0;
        CMOP_ASSERT(Combo_ComboSettingReadStore(COMBO_SETTING_SHARED_LENS, &lens) && lens == 1);
        int32_t rung = 0;
        CMOP_ASSERT(Combo_ComboSettingReadStore(COMBO_SETTING_LOGIC, &rung) &&
                    rung == (int32_t)RSBS_COMBO_RUNG_ALL_REACHABLE);
        CMOP_ASSERT(!Combo_MMOptionIsExplicit(mmLogic));
        CMOP_ASSERT(CVarGetInteger(kProbeKey, 0) == 9);

        // Merge keeps what the profile does not hold; overwrite clears it.
        CMOP_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_SHARED_SHIELDS, 1) == 1);
        report = ComboPresetProfile_Apply(saved, true);
        CMOP_ASSERT(report.applied && Combo_ComboSettingIsExplicit(COMBO_SETTING_SHARED_SHIELDS));
        report = ComboPresetProfile_Apply(saved, false);
        CMOP_ASSERT(report.applied && !Combo_ComboSettingIsExplicit(COMBO_SETTING_SHARED_SHIELDS));
        CMOP_ASSERT(Combo_MMTrickGetValue(bound));

        // 9b. Refused keys: a reserved and an unbound trick (the writer's own
        // gate), an out-of-space rule, and MM's own Logic (a preset made before
        // #1367; [Both Games] Logic sets it); the rest still applies, and a key
        // that is not ours is ignored. A STALE unbound key (a hand-edited
        // config) is put back to never chosen: off is the only value it can hold.
        nlohmann::json bad = saved;
        CmopProfileSet(bad, unbound->cvar, 1);
        CmopProfileSet(bad, reserved->cvar, 1);
        CmopProfileSet(bad, Combo_ComboSettingKey(COMBO_SETTING_GOAL), 99);
        CmopProfileSet(bad, mmLogic->cvar, 1);
        CmopProfileSet(bad, kProbeKey, 1);
        CmopClearPairedProfile();
        CVarSetInteger(unbound->cvar, 1);
        report = ComboPresetProfile_Apply(bad, false);
        CMOP_ASSERT(report.applied && report.refused == 4 && report.ignored == 1);
        CMOP_ASSERT(CVarGetInteger(unbound->cvar, 0) == 0 && CVarGetInteger(reserved->cvar, 0) == 0);
        CMOP_ASSERT(!Combo_ComboSettingIsExplicit(COMBO_SETTING_GOAL));
        CMOP_ASSERT(!Combo_MMOptionIsExplicit(mmLogic));
        CMOP_ASSERT(Combo_MMTrickGetValue(bound));
        CMOP_ASSERT(Combo_ComboSettingReadStore(COMBO_SETTING_SHARED_LENS, &lens) && lens == 1);
        CMOP_ASSERT(CVarGetInteger(kProbeKey, 0) == 9);

        // 9c. A decided world refuses the profile WHOLE, a reset included:
        // first the MM freeze, then the Cross-Game record's.
        CmopAuthorPairing(true);
        CMOP_ASSERT(!ComboPresetProfile_RefusalNote().empty());
        report = ComboPresetProfile_Apply(nlohmann::json(), false);
        CMOP_ASSERT(!report.applied && report.written == 0 && report.cleared == 0);
        CMOP_ASSERT(Combo_MMTrickGetValue(bound) && Combo_MMLocationIsExcluded(check));
        CMOP_ASSERT(Combo_ComboSettingIsExplicit(COMBO_SETTING_SHARED_LENS));
        ComboContext_Init();
        {
            ComboSettingsRecord rec;
            Combo_ComboSettingsDefaults(&rec);
            Combo_FreezeComboSettings(&rec);
        }
        CMOP_ASSERT(Combo_ComboSettingsFrozen() && !Combo_MMProfileFrozen());
        CMOP_ASSERT(!ComboPresetProfile_RefusalNote().empty());
        report = ComboPresetProfile_Apply(bad, false);
        CMOP_ASSERT(!report.applied && report.written == 0 && report.cleared == 0 && report.refused == 0);
        CMOP_ASSERT(Combo_MMTrickGetValue(bound) && Combo_MMOptionIsExplicit(option));

        // 9d. The reset: the shipped "Rando Seed Settings - Reset to Default"
        // preset's paired profile beside gRandoSettings clears every MM and
        // Cross-Game key of the profile, and nothing else.
        ComboContext_Init();
        nlohmann::json reset; // null: what the shipped preset carries
#ifdef RSBS_SOURCE_DIR
        {
            std::ifstream in(std::string(RSBS_SOURCE_DIR) +
                             "/games/oot/assets/custom/presets/Rando Seed Settings - Reset to Default.json");
            CMOP_ASSERT(in.good());
            const nlohmann::json preset = nlohmann::json::parse(in, nullptr, false);
            CMOP_ASSERT(!preset.is_discarded() && preset.contains("blocks") && preset["blocks"].contains("rando"));
            const nlohmann::json& rando = preset["blocks"]["rando"];
            CMOP_ASSERT(rando.contains("gRandoSettings") && rando["gRandoSettings"].is_null());
            CMOP_ASSERT(rando.contains(COMBO_PRESET_PAIRED_PROFILE_KEY));
            reset = rando[COMBO_PRESET_PAIRED_PROFILE_KEY];
        }
#endif
        CMOP_ASSERT(reset.is_null());
        CMOP_ASSERT(Combo_ComboSettingSet(COMBO_SETTING_SHARED_SHIELDS, 1) == 1);
        report = ComboPresetProfile_Apply(reset, false);
        CMOP_ASSERT(report.applied && report.written == 0 && report.refused == 0 && report.cleared > 0);
        CMOP_ASSERT(!Combo_MMOptionIsExplicit(option) && !Combo_CVarIsExplicitInt(bound->cvar));
        CMOP_ASSERT(!Combo_CVarIsExplicitInt(trap->cvar) && Combo_MMLocationsExcluded(NULL, 0) == 0);
        for (int id = 0; id < (int)COMBO_SETTING_COUNT; id++) {
            if (Combo_ComboSettingIsExplicit((ComboSettingId)id)) {
                printf("[TEST] FAIL: the reset preset left the Cross-Game rule '%s' chosen\n",
                       Combo_ComboSettingKey((ComboSettingId)id));
                return TEST_FAIL;
            }
        }
        CMOP_ASSERT(CVarGetInteger(kProbeKey, 0) == 9);

        CmopClearPairedProfile();
        CVarClear(kProbeKey);
        printf("[TEST] leg 9: a preset restores the MM trick and Cross-Game rules it carries ([Both Games] Logic "
               "among them), refuses reserved, unbound and out-of-space keys and MM's own Logic, never touches a "
               "non-randomizer key, is refused whole by a decided world, and the shipped reset preset clears the "
               "paired profile\n");
    }

    ComboContext_Init();
    Context_SetCurrentGame(prevGame);

    printf("[TEST] PASS: the MM options pages' model publishes both tables headlessly, maps every group to a "
           "column and every row to its table's state, follows the freeze, and draws through SoH's combo_ui table\n");
    return TEST_PASS;
}
