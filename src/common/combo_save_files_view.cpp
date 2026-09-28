/**
 * @file combo_save_files_view.cpp
 * @brief The Combo > Save Files page's model. See combo_save_files_view.h.
 */

#include "combo_save_files_view.h"

#include <cstdio>

namespace {

bool gUseTestMeta = false;
rsbs::SlotMeta gTestMeta[RSBS_SAVE_MAX_SLOTS] = {};
int gTestMetaCount = 0;

} // namespace

const char* Combo_SaveFiles_RefuseText(RsbsRefuseReason reason) {
    // Short fragments after "Not paired: ", in the copy of the load toasts
    // (RsbsSave_EmitLoadToast) where a toast exists for the same reason. The
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
            return "Older than the OoT save";
        case RSBS_REFUSE_IDENTITY:
            return "Settings differ from its creation";
        case RSBS_REFUSE_GENERATION:
            return "Termina could not be generated";
        case RSBS_REFUSE_CROSSINGS:
            return "Cross-game items are damaged";
        case RSBS_REFUSE_NONE:
        default:
            return "File could not be checked";
    }
}

ComboSaveFileRow Combo_SaveFiles_RowFor(int slot, const rsbs::SlotMeta& meta) {
    ComboSaveFileRow row;
    char file[16];
    std::snprintf(file, sizeof(file), "File %d", slot + 1);
    row.file = file;

    // A name only from a file whose header this build accepts: ReadMeta stops
    // before the names otherwise, and a refused file's bytes are not trusted.
    if (meta.exists && meta.valid) {
        row.name = rsbs::SlotNameLine(meta);
        // Which halves have begun: the old panel's "[OoT v] [MM v]" markers, in
        // words. The name line alone hides a started MM half whose name matches.
        if (meta.ootStarted && meta.mmStarted) {
            row.started = "OoT, MM";
        } else if (meta.ootStarted) {
            row.started = "OoT";
        } else if (meta.mmStarted) {
            row.started = "MM";
        } else {
            row.started = "None";
        }
        if (meta.lastGame == GAME_OOT) {
            row.lastPlayed = "OoT";
        } else if (meta.lastGame == GAME_MM) {
            row.lastPlayed = "MM";
        }
    }

    if (meta.state == RSBS_SLOT_REFUSED) {
        row.kind = ComboSaveFileKind::Refused;
        row.status = std::string("Not paired: ") + Combo_SaveFiles_RefuseText(meta.refuseReason);
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
        row.kind = ComboSaveFileKind::Empty;
        row.status = meta.hasQuarantine ? "Empty (backup kept)" : "Empty";
        if (meta.hasQuarantine) {
            row.tooltip = "A file that could not be loaded was set aside beside the save as a backup.";
        }
    }
    return row;
}

int Combo_SaveFiles_Rows(ComboSaveFileRow* out, int max) {
    if (out == nullptr || max <= 0) {
        return 0;
    }
    const int count = max < RSBS_SAVE_MAX_SLOTS ? max : RSBS_SAVE_MAX_SLOTS;
    for (int slot = 0; slot < count; slot++) {
        if (gUseTestMeta) {
            // A slot past the table reads as an empty one.
            const rsbs::SlotMeta empty{};
            out[slot] = Combo_SaveFiles_RowFor(slot, slot < gTestMetaCount ? gTestMeta[slot] : empty);
        } else {
            // ReadMeta reads a header and a few bytes, no CRC: cheap enough for a
            // menu frame (the old panel called it per frame for the same reason).
            out[slot] = Combo_SaveFiles_RowFor(slot, rsbs::SaveManager::Instance().ReadMeta(slot));
        }
    }
    return count;
}

std::string Combo_SaveFiles_Note(const ComboSaveFileRow* rows, int count) {
    bool anyFile = false;
    bool anyRefused = false;
    for (int i = 0; rows != nullptr && i < count; i++) {
        anyFile = anyFile || rows[i].kind != ComboSaveFileKind::Empty;
        anyRefused = anyRefused || rows[i].kind == ComboSaveFileKind::Refused;
    }
    // Each case opens with different words, so a player can say which one they see.
    if (anyRefused) {
        return "A file that is not paired plays Ocarina of Time without its Majora's Mask half, and nothing is saved "
               "to the pair.";
    }
    if (!anyFile) {
        return "No save files yet. Create one from the Ocarina of Time file select.";
    }
    return "Each file holds both games. Load, create and erase files from the Ocarina of Time file select.";
}

void Combo_SaveFiles_SetMetaForTest(const rsbs::SlotMeta* metas, int count) {
    gUseTestMeta = metas != nullptr;
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
