#pragma once

/**
 * @file combo_save_files_view.h
 * @brief The Combo > Save Files page's model: one row per save file, built from
 *        the `.redsave` slot metadata (rsbs::SlotMeta), in the player's words.
 *
 * WHY A PAGE. The only surface that ever drew the `.redsave` slot state was
 * ComboMenuBar's file-select panel, and ComboMenuBar was compiled but never
 * constructed, so nobody could see it. It was deleted on 2026-09-28. What it
 * showed that nothing else shows is the state of a file BEFORE it is loaded: a
 * load that refuses for the cross-game rules or a damaged cross-game record
 * posts a "Not paired:" toast (RsbsSave_EmitLoadToast), but a refusal for the
 * file itself (header, version, size, slot, truncation, CRC) or for a missing
 * commit (commit skew) posts nothing a player sees, and ReadMeta reports a
 * structurally refused file before any load is attempted. The page is
 * read-only: loading, creating and erasing a file stay in Ocarina of Time's
 * file select, whose erase already releases a refused slot (OnDeleteFile ->
 * RsbsSave_DeleteSave), so the panel's Load / Delete / Save-to-slot buttons have
 * no reader left.
 *
 * PAGE OR WINDOW. The state is read between sessions (which file to load, why
 * one is refused), never during play, so it is a menu page under Combo, not a
 * window (docs/ui-style-guide.md section 10; ADR 0004's 2026-09-28 paragraph).
 *
 * ADR 0008 rule 1: the model is src/common's; the drawing is
 * games/oot/soh/SohGui/SohMenuComboSaveFiles.cpp. Game-header-free (ADR 0002).
 * Locked by ComboSaveFilesView (this model, ROM-free) and drawn by UiSnapshot
 * (Combo > Save Files, whose states are authored through the test seam below).
 */

#include "save.h"

#include <string>

/** What a row says about its file, which the page's note also reads. */
enum class ComboSaveFileKind {
    Empty,   // no file, nothing refused
    Ready,   // a file this build loads
    Refused, // a file this build (or this session) will not load
};

struct ComboSaveFileRow {
    ComboSaveFileKind kind = ComboSaveFileKind::Empty;
    std::string file;       // "File 1"
    std::string name;       // rsbs::SlotNameLine for a file whose header passes, else empty
    std::string started;    // "OoT, MM" / "OoT" / "MM" / "None" for such a file, else empty
    std::string lastPlayed; // "OoT" / "MM" for such a file, else empty
    std::string status;     // "Ready", "Empty", "Not loaded: <reason>", ...
    std::string tooltip;    // one or two sentences for the status cell, empty for none
};

/** The player's words for a refusal reason: a short fragment that follows "Not
 *  loaded: ". Never empty and never an issue number; RSBS_REFUSE_NONE and an
 *  unknown value read "File could not be checked". */
const char* Combo_SaveFiles_RefuseText(RsbsRefuseReason reason);

/** The row for one slot's metadata. Pure: the lock drives it directly. */
ComboSaveFileRow Combo_SaveFiles_RowFor(int slot, const rsbs::SlotMeta& meta);

/** Every slot's row, in slot order (at most RSBS_SAVE_MAX_SLOTS), read from
 *  SaveManager::ReadMeta, or from the test seam's table while one is set.
 *  Returns how many were written. */
int Combo_SaveFiles_Rows(ComboSaveFileRow* out, int max);

/** The page's gray note for a set of rows: one or two sentences. */
std::string Combo_SaveFiles_Note(const ComboSaveFileRow* rows, int count);

/** TEST SEAM (UiSnapshot's states, the ComboSaveFilesView row): while set, the
 *  rows come from `metas` instead of the disk. `metas == nullptr` clears it. The
 *  table is copied; at most RSBS_SAVE_MAX_SLOTS entries are used. */
void Combo_SaveFiles_SetMetaForTest(const rsbs::SlotMeta* metas, int count);
