/**
 * @file combo_save_files_view.cpp
 * @brief The Combo > Save Files page's model. See combo_save_files_view.h.
 */

#include "combo_save_files_view.h"

#include <cctype>
#include <cstdio>

namespace {

bool gUseTestMeta = false;
rsbs::SlotMeta gTestMeta[RSBS_SAVE_MAX_SLOTS] = {};
int gTestMetaCount = 0;
// Moves on every seam call, so a changed table is re-read.
unsigned gTestMetaSerial = 0;

// The cached view and what it was read against.
ComboSaveFilesView gView;
bool gHaveView = false;
bool gReadDeferred = false;
uint32_t gViewEpoch = 0;
unsigned gViewSerial = 0;
int gReadCount = 0;

/** One read of every slot's metadata into rows: from the seam, or from the disk
 *  under the writer's lock. False (rows untouched) while a write is in flight. */
bool ReadRows(ComboSaveFileRow* out) {
    rsbs::SlotMeta metas[RSBS_SAVE_MAX_SLOTS] = {};
    if (gUseTestMeta) {
        for (int slot = 0; slot < gTestMetaCount; slot++) {
            metas[slot] = gTestMeta[slot];
        }
    } else if (!rsbs::SaveManager::Instance().TryReadMetaAll(metas, RSBS_SAVE_MAX_SLOTS)) {
        return false;
    }
    for (int slot = 0; slot < RSBS_SAVE_MAX_SLOTS; slot++) {
        out[slot] = Combo_SaveFiles_RowFor(slot, metas[slot]);
    }
    return true;
}

} // namespace

const char* Combo_SaveFiles_RefuseText(RsbsRefuseReason reason) {
    // The page's own fragments after "Not paired: ". A refusal that posted a toast
    // shows the toast's words instead (SlotMeta.refuseWords); these cover the
    // refusals that post none, and any whose words were not recorded. The
    // developer label (SaveManager::RefuseReasonLabel) stays on stderr.
    switch (reason) {
        case RSBS_REFUSE_UNREADABLE:
            return "File could not be read";
        case RSBS_REFUSE_HEADER:
            return "Not a save file";
        case RSBS_REFUSE_VERSION:
        case RSBS_REFUSE_TIER_SIZE:
            return "File made by another build";
        case RSBS_REFUSE_WRONG_SLOT:
            return "File belongs to another slot";
        case RSBS_REFUSE_TRUNCATED:
            return "File is incomplete";
        case RSBS_REFUSE_CRC:
            return "File is damaged";
        case RSBS_REFUSE_COMBO_MAGIC:
            return "Cross-game record is damaged";
        case RSBS_REFUSE_COMMIT_SKEW:
            return "Ocarina of Time save is newer";
        case RSBS_REFUSE_IDENTITY:
            return "Settings differ from its creation";
        case RSBS_REFUSE_GENERATION:
            return "Termina could not be generated";
        case RSBS_REFUSE_CROSSINGS:
            return "Cross-game items are damaged";
        case RSBS_REFUSE_MISSING:
            return "Cross-game record is missing";
        case RSBS_REFUSE_NONE:
        default:
            return "File could not be checked";
    }
}

std::string Combo_SaveFiles_ToastWords(const char* words) {
    std::string out = words != nullptr ? words : "";
    while (!out.empty() && (out.back() == '.' || out.back() == ' ')) {
        out.pop_back();
    }
    if (!out.empty()) {
        out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    }
    return out;
}

ComboSaveFileRow Combo_SaveFiles_RowFor(int slot, const rsbs::SlotMeta& meta) {
    ComboSaveFileRow row;
    char file[16];
    std::snprintf(file, sizeof(file), "File %d", slot + 1);
    row.file = file;
    row.backupKept = meta.hasQuarantine;

    // A name only from a record whose header this build accepts: ReadMeta stops
    // before the names otherwise, and a refused file's bytes are not trusted.
    if (meta.exists && meta.valid) {
        row.name = rsbs::SlotNameLine(meta);
        if (meta.lastGame == GAME_OOT) {
            row.lastPlayed = "OoT";
        } else if (meta.lastGame == GAME_MM) {
            row.lastPlayed = "MM";
        }
    }

    if (meta.state == RSBS_SLOT_REFUSED) {
        row.kind = ComboSaveFileKind::Refused;
        // The toast's words where the refusal posted one, so the page and the
        // toast say the same thing; the page's own words otherwise.
        const std::string toastWords = Combo_SaveFiles_ToastWords(meta.refuseWords);
        row.status = std::string("Not paired: ") +
                     (toastWords.empty() ? std::string(Combo_SaveFiles_RefuseText(meta.refuseReason)) : toastWords);
        row.tooltip = meta.hasQuarantine ? "The original file is kept beside the save as a backup. Erasing this "
                                           "file in the file select frees the slot and discards the backup."
                                         : "The file is left in place. Erasing this file in the file select frees "
                                           "the slot.";
    } else if (meta.state == RSBS_SLOT_VALID) {
        row.kind = ComboSaveFileKind::Ready;
        row.status = "Ready";
        if (meta.commitSkew > 0) {
            row.tooltip = "Loaded from its newest save, which was newer than the Ocarina of Time save. Both games "
                          "resumed from the same moment.";
        }
    } else {
        // No .redsave at the slot. That is not "no file": Ocarina of Time's own
        // file can still be there (a record set aside by a refusal in an earlier
        // session, or one that was never written), so the words name the record.
        row.kind = ComboSaveFileKind::Empty;
        row.status = meta.hasQuarantine ? "No cross-game record (backup kept)" : "No cross-game record";
        if (meta.hasQuarantine) {
            if (meta.quarantineReason != RSBS_REFUSE_NONE) {
                row.tooltip = std::string("A cross-game record refused as \"") +
                              Combo_SaveFiles_RefuseText(meta.quarantineReason) +
                              "\" was set aside beside the save as a backup. Erasing this file in the file select "
                              "discards it.";
            } else {
                row.tooltip = "A cross-game record that could not be loaded was set aside beside the save as a "
                              "backup. Erasing this file in the file select discards it.";
            }
        }
    }
    return row;
}

std::string Combo_SaveFiles_Note(const ComboSaveFileRow* rows, int count) {
    bool anyReady = false;
    bool anyRefused = false;
    bool anyBackup = false;
    for (int i = 0; rows != nullptr && i < count; i++) {
        anyReady = anyReady || rows[i].kind == ComboSaveFileKind::Ready;
        anyRefused = anyRefused || rows[i].kind == ComboSaveFileKind::Refused;
        anyBackup = anyBackup || rows[i].backupKept;
    }
    // Each case opens with different words, so a player can say which one they
    // see. None claims there are no save files: a slot with no cross-game record
    // can still hold an Ocarina of Time file.
    if (anyRefused) {
        // A refused file is never opened (#836, operator ruling 2026-10-01): the
        // file select keeps the player on the file list. The two ways out are a
        // good copy of the record put back, or the player's own erase.
        return "A refused file does not open. Put a good copy of its cross-game record back, or erase it in the "
               "file select to free the slot.";
    }
    if (anyReady) {
        return "Each file holds both games. Load, create and erase files from the Ocarina of Time file select.";
    }
    if (anyBackup) {
        return "A cross-game record that could not be loaded was kept as a backup. Erasing its file in the "
               "Ocarina of Time file select discards it.";
    }
    return "No cross-game record yet. One is written when a file is created in the Ocarina of Time file select.";
}

const ComboSaveFilesView& Combo_SaveFiles_View(bool pageOpened) {
    // The epoch is taken BEFORE the read: a write that lands during or after it
    // moves the epoch past this value, so the next call reads again.
    const uint32_t epoch = rsbs::SaveManager::Instance().SlotStateEpoch();
    const bool stale =
        !gHaveView || pageOpened || gReadDeferred || epoch != gViewEpoch || gTestMetaSerial != gViewSerial;
    if (!stale) {
        return gView;
    }
    ComboSaveFileRow rows[RSBS_SAVE_MAX_SLOTS];
    if (!ReadRows(rows)) {
        // A .redsave write is in flight: keep what the page showed, try again
        // next frame. A first read that defers shows every slot without a record.
        gReadDeferred = true;
        if (!gHaveView) {
            const rsbs::SlotMeta none{};
            for (int slot = 0; slot < RSBS_SAVE_MAX_SLOTS; slot++) {
                gView.rows[slot] = Combo_SaveFiles_RowFor(slot, none);
            }
            gView.count = RSBS_SAVE_MAX_SLOTS;
            gView.note = Combo_SaveFiles_Note(gView.rows, gView.count);
        }
        return gView;
    }
    for (int slot = 0; slot < RSBS_SAVE_MAX_SLOTS; slot++) {
        gView.rows[slot] = rows[slot];
    }
    gView.count = RSBS_SAVE_MAX_SLOTS;
    gView.note = Combo_SaveFiles_Note(gView.rows, gView.count);
    gHaveView = true;
    gReadDeferred = false;
    gViewEpoch = epoch;
    gViewSerial = gTestMetaSerial;
    gReadCount++;
    return gView;
}

int Combo_SaveFiles_ReadCountForTest() {
    return gReadCount;
}

void Combo_SaveFiles_SetMetaForTest(const rsbs::SlotMeta* metas, int count) {
    gUseTestMeta = metas != nullptr;
    gTestMetaSerial++;
    gTestMetaCount = 0;
    for (int i = 0; i < RSBS_SAVE_MAX_SLOTS; i++) {
        gTestMeta[i] = rsbs::SlotMeta{};
    }
    if (metas == nullptr) {
        return;
    }
    gTestMetaCount = count < RSBS_SAVE_MAX_SLOTS ? (count < 0 ? 0 : count) : RSBS_SAVE_MAX_SLOTS;
    for (int i = 0; i < gTestMetaCount; i++) {
        gTestMeta[i] = metas[i];
    }
}
