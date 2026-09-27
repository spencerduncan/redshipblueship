/**
 * @file combo_mm_options_page.h
 * @brief The page view model for MM's randomizer options and tricks, hosted as
 *        two pages of the Combo section: Combo > MM Randomizer and Combo > MM
 *        Tricks (ADR 0004's 2026-09-27 host amendment; #497, #578, #682).
 *
 * WHAT MOVED. Until this file, MM's 47 randomizer options and its trick table
 * were drawn by a common-owned pop-out window (the retired
 * ComboMmOptionsWindow.cpp), opened from a "Toggle MM Randomizer Options" row on
 * Combo > Windows. That inverted SoH's convention: SoH keeps settings chosen
 * BEFORE a session on menu pages (Randomizer > General, Logic/Access,
 * Tricks/Glitches) and keeps floating windows for tools used DURING play (Item
 * Tracker, Check Tracker, the Cosmetics and Audio editors). These options freeze
 * into the paired world at the creation event, so they are page material. The
 * window was kept in 2026-07 only because src/common could not draw with SoH's
 * widgets; the pages are registered by an OoT TU under SohGui/, which can.
 *
 * WHAT THIS FILE IS. Everything about the pages that is a DECISION rather than
 * a draw call, in src/common so it is lockable without a window and without an
 * SoH header (ADR 0002): which column each option group sits in, what each row's
 * presentation state is (live, disabled by capability with its reason, frozen at
 * creation), what the state notes and warnings say, the trick page's headline,
 * and what Reset clears. The OoT page TU
 * (games/oot/soh/SohGui/SohMenuComboMmRandomizer.cpp) only turns these answers
 * into SohMenu rows. The model reads gComboCtx, the MM descriptor tables and the
 * active game. It reads no gSettings.Menu.* key (ADR 0004 section 4.1a(ii)'s
 * invariant, kept for the model; the pages persist their sidebar selection in
 * the Combo section's own gSettings.Menu.ComboSidebarSection, which no MM-side
 * code reads) and no game's gSaveContext (ADR 0008 rule 5).
 *
 * NO OPTION SEMANTICS CHANGE. The rows read and write through the same gated
 * accessors the window used (combo_mm_options_view.h, combo_mm_tricks_view.h):
 * the writers still refuse while Combo_MMProfileFrozen(), and the descriptor
 * tables are still the one source of labels, tooltips, ranges and reasons.
 *
 * Locked by ComboMMOptionsPage (this model, display-free) and
 * MenuMmRandomizerPages (the registered SohMenu rows: the row set equals the
 * descriptor table, the columns, the three presentation states, the gated writes,
 * and the Windows page without the retired toggle).
 */

#ifndef RSBS_COMMON_COMBO_MM_OPTIONS_PAGE_H
#define RSBS_COMMON_COMBO_MM_OPTIONS_PAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "combo_mm_options_view.h"
#include "combo_mm_tricks_view.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The two Combo section sidebars. Selection persists BY DISPLAY NAME in
 *  gSettings.Menu.ComboSidebarSection, so these are the one spelling. */
#define COMBO_MM_OPTIONS_PAGE_NAME "MM Randomizer"
#define COMBO_MM_TRICKS_PAGE_NAME "MM Tricks"

/**
 * The ImGui id suffix every checkbox and combobox row on the options page
 * carries (docs/ui-style-guide.md R-N5). Several MM labels are also OoT
 * randomizer labels ("Logic", "Shuffle Cows", "Triforce Hunt", ...), and the
 * menu search draws matching rows from every page into one window, where two
 * rows with one name would share an ImGui id. Sliders take SoH's "Name: %d"
 * shape instead, which is distinct on its own (UIWidgets::SliderInt prints its
 * label raw, so a "##" suffix would be visible there).
 */
#define COMBO_MM_OPTIONS_ROW_ID_SUFFIX "##MMRando"

/** The Reset confirm's title (a SohGui::RegisterPopup title). */
#define COMBO_MM_OPTIONS_RESET_TITLE "Reset MM Randomizer"

/** The freeze's disabled reason, in SoH's reason style ("Save Not Loaded"): the
 *  words SohMenu gives its FROZEN presentation and the Cross-Game Rules rows
 *  carry, so the paired world's authoring surfaces say one thing. */
#define COMBO_MM_OPTIONS_FROZEN_REASON "Already Decided"

/** How many columns the options page declares (SoH's Randomizer > General). */
#define COMBO_MM_OPTIONS_PAGE_COLUMNS 2

/**
 * Publish MM's two descriptor tables (options and tricks) and bring up the
 * tier-4 settings model beside them. Idempotent, headless-safe, and needs no
 * Gui: the page registrar calls it before it reads the tables (the menu is
 * built during OoT's start-up, before rsbs/src/main.cpp's own call), and every
 * lock calls it for the model. It replaces the retired window's
 * Combo_MMOptionsWindow_Init, minus the window.
 */
void Combo_MMOptionsPages_Init(void);

/**
 * The column (0 or 1) an option group sits in on the options page, or -1 for a
 * group outside ComboMMOptionGroup. Groups keep MM's own order within a column:
 * column 0 holds Logic & Conditions then Shuffle Options (23 rows), column 1
 * holds Items, Starting Items and Hints then Reset (25 rows), so the two columns
 * end within a couple of rows of each other.
 */
int Combo_MMOptionsPage_GroupColumn(uint8_t group);

/** A row's presentation (the ADR 0004 section 6 states the pages use). */
typedef enum {
    /** Editable. No disabled tooltip. */
    COMBO_MM_ROW_LIVE = 0,
    /** Disabled by capability (ADR 0004 section 5): the option's behaviour is
     *  absent from this build, or a trick is reserved or not yet bound.
     *  The reason is the table's own fragment. */
    COMBO_MM_ROW_BLOCKED,
    /** Frozen at creation (section 6 state 4): read-only. The reason is
     *  COMBO_MM_OPTIONS_FROZEN_REASON, never the capability reason, because
     *  "Already Decided" and a capability reason send a player looking in
     *  different places. */
    COMBO_MM_ROW_FROZEN,
} ComboMMRowState;

/**
 * The option row's state, and its reason in @p reason (never NULL on return;
 * "" when LIVE; @p reason may be NULL). Frozen wins over a capability block:
 * once the world exists the row's value is the world's, whatever its capability.
 */
ComboMMRowState Combo_MMOptionsPage_OptionState(const ComboMMOptionDesc* desc, const char** reason);

/** The trick row's state and reason, by the same rule. A reserved or unbound
 *  trick is BLOCKED with the table's reason. */
ComboMMRowState Combo_MMOptionsPage_TrickState(const ComboMMTrickDesc* desc, const char** reason);

/**
 * The options page's gray state note, one of three sentences (never NULL):
 *  - frozen: the profile was stamped at creation; it names the escape (the
 *    title screen) and prints no fingerprint, because the one number shown for
 *    the world is Combo > Cross-Game Rules';
 *  - paired but not frozen: a legacy pre-freeze pair, frozen at first crossing;
 *  - unpaired: these are saved into the next paired world at generation.
 */
const char* Combo_MMOptionsPage_StatusNote(void);

/** True when ADR 0004 section 6's editable-but-not-active note shows: the
 *  profile is not frozen and Majora's Mask is not the running game. */
bool Combo_MMOptionsPage_Suspended(void);

/** That note's sentence. */
const char* Combo_MMOptionsPage_SuspendedNote(void);

/** The creation-lock warning (orange), or NULL once frozen (the status note
 *  says it then). */
const char* Combo_MMOptionsPage_LockWarning(void);

/** The vanilla-fallback warning (orange), shown in every state. */
const char* Combo_MMOptionsPage_FallbackWarning(void);

/** Tricks that can be turned on in this build: bound and not reserved. */
int Combo_MMOptionsPage_SettableTrickCount(void);

/**
 * The tricks page's gray note, written into @p buf (always NUL-terminated when
 * @p size > 0; returns @p buf): the freeze sentence once frozen, otherwise the
 * honest headline "N of M tricks are supported by the randomizer logic so
 * far..." (86 checkboxes of which most do nothing would be the overclaim). With
 * no trick table registered, a sentence saying so.
 */
const char* Combo_MMOptionsPage_TricksNote(char* buf, size_t size);

/**
 * Reset's action (the confirm's Reset button): clears every option CVar and
 * every trick CVar, back to "never chosen". Clears rather than writing the
 * defaults, because the paired logic default tells "the player chose this" from
 * "nobody touched it" by the key's presence. Under a freeze the writers refuse
 * on their own, so a confirm answered after a freeze landed changes nothing.
 */
void Combo_MMOptionsPage_ResetAll(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_COMBO_MM_OPTIONS_PAGE_H
