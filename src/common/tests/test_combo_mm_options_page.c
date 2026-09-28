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
 *    frozen each get their own sentence; the suspended note shows exactly when
 *    the profile is editable and Majora's Mask is not the running game; the
 *    creation-lock warning goes away once frozen; the tricks headline counts the
 *    settable tricks, and the freeze replaces it.
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
 * Deliberately absent: appearance. That is judged from pixels by the UiSnapshot
 * row's captures of the two pages (docs/ui-style-guide.md section 12).
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++).
 * Entry point is a RunHeadless bridge because Combo_MMOptionsPages_Init brings
 * up the tier-4 settings window's model, which needs the shared bring-up.
 */

#include "../combo_mm_options_page.h"
#include "../combo_mm_options_view.h"
#include "../combo_mm_tricks_view.h"
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
        printf("[TEST] leg 2: every option group has a column (%d rows in column 1, %d in column 2)\n",
               rowsInColumn[0], rowsInColumn[1]);
    }

    // ---- 3. Row states, both directions, and the freeze --------------------
    {
        ComboContext_Init();
        CMOP_ASSERT(!Combo_MMProfileFrozen());
        for (int i = 0; i < optionCount; i++) {
            const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
            const char* reason = NULL;
            const ComboMMRowState state = Combo_MMOptionsPage_OptionState(d, &reason);
            const bool gated = d->liveness == COMBO_MM_LIVENESS_PARTIAL || d->liveness == COMBO_MM_LIVENESS_DORMANT;
            CMOP_ASSERT(reason != NULL);
            if (gated != (state == COMBO_MM_ROW_BLOCKED) || (!gated && state != COMBO_MM_ROW_LIVE)) {
                printf("[TEST] FAIL: option %s (liveness %u) has page state %d\n", d->name, (unsigned)d->liveness,
                       (int)state);
                return TEST_FAIL;
            }
            CMOP_ASSERT(gated ? strcmp(reason, d->disabledReason) == 0 && reason[0] != '\0' : reason[0] == '\0');
        }
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
        CMOP_ASSERT(CmopContains(Combo_MMOptionsPage_StatusNote(), "Loading a paired file restores"));
        CMOP_ASSERT(Combo_MMOptionsPage_Suspended());
        CMOP_ASSERT(Combo_MMOptionsPage_LockWarning() != NULL);
        CMOP_ASSERT(Combo_MMOptionsPage_FallbackWarning() != NULL);
        char expected[64];
        snprintf(expected, sizeof(expected), "%d of %d tricks", Combo_MMOptionsPage_SettableTrickCount(), trickCount);
        CMOP_ASSERT(CmopContains(Combo_MMOptionsPage_TricksNote(tricks, sizeof(tricks)), expected));
        CMOP_ASSERT(CmopContains(tricks, "Loading a paired file restores"));

        Context_SetCurrentGame(GAME_MM);
        CMOP_ASSERT(!Combo_MMOptionsPage_Suspended());

        CmopAuthorPairing(false);
        CMOP_ASSERT(CmopContains(Combo_MMOptionsPage_StatusNote(), "predates saved Majora's Mask options"));

        CmopAuthorPairing(true);
        Context_SetCurrentGame(GAME_OOT);
        CMOP_ASSERT(CmopContains(Combo_MMOptionsPage_StatusNote(), "Already decided when this world was created"));
        CMOP_ASSERT(!Combo_MMOptionsPage_Suspended());
        CMOP_ASSERT(Combo_MMOptionsPage_LockWarning() == NULL);
        CMOP_ASSERT(CmopContains(Combo_MMOptionsPage_TricksNote(tricks, sizeof(tricks)), "Already decided"));
        CMOP_ASSERT(!CmopContains(tricks, expected));
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
                                    Combo_MMOptionsPage_LockWarning(), Combo_MMOptionsPage_FallbackWarning(),
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
                printf("[TEST] FAIL: MM trick %s's disabled reason '%s' is not a Title Case fragment (R-S2)\n",
                       d->name, d->disabledReason);
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

    ComboContext_Init();
    Context_SetCurrentGame(prevGame);

    printf("[TEST] PASS: the MM options pages' model publishes both tables headlessly, maps every group to a "
           "column and every row to its table's state, follows the freeze, and draws through SoH's combo_ui table\n");
    return TEST_PASS;
}
