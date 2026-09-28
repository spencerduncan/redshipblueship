#pragma once

/**
 * @file combo_save_files_view.h
 * @brief The Combo > Save Files page's model: one row per save file, built from
 *        the `.redsave` slot metadata (rsbs::SlotMeta), in the player's words.
 *
 * WHY A PAGE. The only surface that ever drew the `.redsave` slot state was
 * ComboMenuBar's file-select panel, and ComboMenuBar was compiled but never
 * constructed, so nobody could see it. It was deleted on 2026-09-28. What the
 * page adds is each file's cross-game state between loads, in one place:
 *   - BEFORE any load, ReadMeta reports the refusals it can see without one:
 *     the header, version, tier size, slot and truncation checks. It does NOT
 *     run the CRC, the Tier-4 crossing block or the commit-generation
 *     comparison, so a file that fails one of those reads "Ready" until a load
 *     refuses it.
 *   - AFTER a refusal, the session's record names it: the load's structural,
 *     CRC, crossing-block and commit-skew refusals (which post no toast), and
 *     the refusals that do post one. For those the row repeats the toast's own
 *     reason words (SlotMeta.refuseWords, recorded where the toast is posted:
 *     the load's "Not paired:" toasts in save.cpp, the arrival's and the MM
 *     spoiler's "Not saved:" toasts in MM_Rando_EmitPairingRefusalToast).
 *   - AFTER A RESTART, a refusal that quarantined its file is gone from the
 *     session record but its evidence is not: the row says a backup was kept
 *     and names the reason its file name carries (SlotMeta.quarantineReason).
 * The page is read-only: loading, creating and erasing a file stay in Ocarina of
 * Time's file select, whose erase already releases a refused slot (OnDeleteFile
 * -> RsbsSave_DeleteSave), so the panel's Load / Delete / Save-to-slot buttons
 * have no reader left.
 *
 * NOT PER FRAME. ReadMeta reads each slot file whole (about 200 KB) and scans the
 * Save directory. Combo_SaveFiles_View caches one read for the note and the
 * table, re-reads only when the page is opened or the SaveManager's
 * SlotStateEpoch moves, and reads under the writer's lock (TryReadMetaAll), so
 * the page never holds a slot file open across a save's rename.
 *
 * PAGE OR WINDOW. The state is read between sessions (which file to load, why
 * one is refused), never during play, so it is a menu page under Combo, not a
 * window (docs/ui-style-guide.md section 10; ADR 0004's 2026-09-28 paragraph).
 *
 * ADR 0008 rule 1: the model is src/common's; the drawing is
 * games/oot/soh/SohGui/SohMenuComboSaveFiles.cpp. Game-header-free (ADR 0002).
 * Locked by ComboSaveFilesView (this model, ROM-free), paired-load-restore legs
 * 3 and 4 (a real refused load's and arrival's row repeats its toast) and drawn
 * by UiSnapshot (Combo > Save Files, whose states are authored through the test
 * seam below).
 */

#include "save.h"

#include <string>

/** What a row says about its slot, which the page's note also reads. */
enum class ComboSaveFileKind {
    Empty,   // no cross-game record at the slot (an Ocarina of Time file may still be there)
    Ready,   // a cross-game record this build lists as loadable
    Refused, // a record this build (or this session) will not load
};

struct ComboSaveFileRow {
    ComboSaveFileKind kind = ComboSaveFileKind::Empty;
    bool backupKept = false; // a refused record was set aside beside the save (*.bak)
    std::string file;        // "File 1"
    std::string name;        // rsbs::SlotNameLine for a record whose header passes, else empty
    std::string lastPlayed;  // "OoT" / "MM" for such a record, else empty
    std::string status;      // "Ready", "No cross-game record", "Not paired: <reason>", ...
    std::string tooltip;     // one or two sentences for the status cell, empty for none
};

/** The page's own words for a refusal reason: a short fragment that follows
 *  "Not paired: ". Used when the refusal posted no toast (the structural, CRC,
 *  crossing-block and commit-skew refusals) or its words were not recorded.
 *  Never empty and never an issue number; RSBS_REFUSE_NONE and an unknown value
 *  read "File could not be checked". */
const char* Combo_SaveFiles_RefuseText(RsbsRefuseReason reason);

/** A toast's reason words as a status fragment: the first letter capitalized
 *  and a trailing period dropped ("rules changed (Goal)." -> "Rules changed
 *  (Goal)"). */
std::string Combo_SaveFiles_ToastWords(const char* words);

/** The row for one slot's metadata. Pure: the lock drives it directly. */
ComboSaveFileRow Combo_SaveFiles_RowFor(int slot, const rsbs::SlotMeta& meta);

/** The page's gray note for a set of rows: one or two sentences. */
std::string Combo_SaveFiles_Note(const ComboSaveFileRow* rows, int count);

/** What the page draws: every slot's row (RSBS_SAVE_MAX_SLOTS) and the note. */
struct ComboSaveFilesView {
    ComboSaveFileRow rows[RSBS_SAVE_MAX_SLOTS];
    int count = 0;
    std::string note;
};

/**
 * The page's rows and note, from ONE read that the note and the table share.
 * Re-reads its source (SaveManager::TryReadMetaAll, or the test seam's table)
 * only when `pageOpened`, when the SaveManager's SlotStateEpoch has moved since
 * the last read, when the test seam changed, or when the last attempt found a
 * write in flight; otherwise it returns the cached view untouched.
 */
const ComboSaveFilesView& Combo_SaveFiles_View(bool pageOpened);

/** How many times Combo_SaveFiles_View has read its source: the lock's
 *  observable for "not every frame". */
int Combo_SaveFiles_ReadCountForTest();

/** TEST SEAM (UiSnapshot's states, the ComboSaveFilesView row): while set, the
 *  rows come from `metas` instead of the disk. `metas == nullptr` clears it. The
 *  table is copied; at most RSBS_SAVE_MAX_SLOTS entries are used; a slot past
 *  the table reads as a slot with no record. Every call makes the next
 *  Combo_SaveFiles_View re-read. */
void Combo_SaveFiles_SetMetaForTest(const rsbs::SlotMeta* metas, int count);
