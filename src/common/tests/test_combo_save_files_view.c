/**
 * @file test_combo_save_files_view.c
 * @brief ROM-free lock for the Combo > Save Files page's model
 *        (src/common/combo_save_files_view.cpp).
 *
 * CTest row ComboSaveFilesView ("combo-save-files-view", label "redship").
 * Display-free and disk-free: every row is built from a synthetic
 * rsbs::SlotMeta, directly or through the page's test seam, so the player's own
 * Save folder is never read.
 *
 * WHAT IT ASSERTS:
 *   A. Every refusal reason has player words: non-empty, no issue number, no
 *      trailing period, distinct from the developer label on stderr, and short
 *      enough that "Not paired: <reason>" stays one table cell's line or two
 *      (at most 45 characters). NONE and an out-of-range value read the
 *      fallback. The three reasons a load toast also names use the toast's own
 *      words (RsbsSave_EmitLoadToast), so the page and the toast agree.
 *   B. One row per state: an empty slot; an empty slot with a set-aside backup;
 *      a ready file (its name is rsbs::SlotNameLine's, its last game spelled
 *      out, no tooltip); a ready file whose last load resumed a newer commit
 *      (a tooltip); a file refused for its header (no name: its bytes are not
 *      read); a healthy file this session refused (its name still shows) with
 *      and without a backup (the tooltip says which).
 *   C. The rows through the seam: slot order, "File N" labels, a short table
 *      reads as empty slots, `max` is honoured, and clearing the seam drops the
 *      table.
 *   D. The note: one case per situation (no file, every file pairs, one does
 *      not), each opening with different words, each within the note budget
 *      (R-X1: two sentences, 200 characters).
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++).
 */

#include <cstdio>
#include <cstring>
#include <string>

#include "../combo_save_files_view.h"
#include "../save.h"

namespace {

#define CSF_ASSERT(cond, ...)                         \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            failures++;                               \
        }                                             \
    } while (0)

rsbs::SlotMeta CsfEmpty() {
    rsbs::SlotMeta m{};
    m.state = RSBS_SLOT_ABSENT;
    m.lastGame = GAME_NONE;
    return m;
}

rsbs::SlotMeta CsfReady(const char* oot, const char* mm, GameId last) {
    rsbs::SlotMeta m = CsfEmpty();
    m.exists = true;
    m.valid = true;
    m.state = RSBS_SLOT_VALID;
    m.lastGame = last;
    std::snprintf(m.ootName, sizeof(m.ootName), "%s", oot);
    std::snprintf(m.mmName, sizeof(m.mmName), "%s", mm);
    m.ootStarted = oot[0] != '\0';
    m.mmStarted = mm[0] != '\0';
    return m;
}

int CsfSentences(const std::string& s) {
    int n = 0;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '.' && (i + 1 == s.size() || s[i + 1] == ' ')) {
            n++;
        }
    }
    return n;
}

bool CsfHasDigitOrHash(const char* s) {
    for (; *s != '\0'; s++) {
        if ((*s >= '0' && *s <= '9') || *s == '#') {
            return true;
        }
    }
    return false;
}

} // namespace

TestResult Test_ComboSaveFilesView(void) {
    printf("[TEST] combo-save-files-view: the Combo > Save Files page's rows and note, from synthetic slot metadata\n");
    int failures = 0;

    // ---- A. Every refusal reason has player words ----------------------------
    for (int r = RSBS_REFUSE_UNREADABLE; r <= RSBS_REFUSE_CROSSINGS; r++) {
        const RsbsRefuseReason reason = static_cast<RsbsRefuseReason>(r);
        const char* text = Combo_SaveFiles_RefuseText(reason);
        CSF_ASSERT(text != nullptr && text[0] != '\0', "refusal reason %d has no player words", r);
        if (text == nullptr || text[0] == '\0') {
            continue;
        }
        const std::string cell = std::string("Not paired: ") + text;
        CSF_ASSERT(!CsfHasDigitOrHash(text), "reason %d's words carry a number or '#': \"%s\"", r, text);
        CSF_ASSERT(text[std::strlen(text) - 1] != '.', "reason %d's words end in a period: \"%s\"", r, text);
        CSF_ASSERT(cell.size() <= 45, "reason %d's cell is %zu characters (at most 45): \"%s\"", r, cell.size(),
                   cell.c_str());
        CSF_ASSERT(std::strcmp(text, rsbs::SaveManager::RefuseReasonLabel(reason)) != 0,
                   "reason %d shows the developer label \"%s\" instead of player words", r, text);
        CSF_ASSERT(std::strcmp(text, Combo_SaveFiles_RefuseText(RSBS_REFUSE_NONE)) != 0,
                   "reason %d falls through to the fallback \"%s\"", r, text);
    }
    CSF_ASSERT(std::strcmp(Combo_SaveFiles_RefuseText(RSBS_REFUSE_NONE), "File could not be checked") == 0,
               "RSBS_REFUSE_NONE reads \"%s\", not the fallback", Combo_SaveFiles_RefuseText(RSBS_REFUSE_NONE));
    CSF_ASSERT(std::strcmp(Combo_SaveFiles_RefuseText(static_cast<RsbsRefuseReason>(250)),
                           "File could not be checked") == 0,
               "an out-of-range reason does not read the fallback");
    // The toast's own words where a toast names the same refusal.
    CSF_ASSERT(std::strcmp(Combo_SaveFiles_RefuseText(RSBS_REFUSE_COMBO_MAGIC), "Cross-game record is damaged") == 0,
               "COMBO_MAGIC does not use the damaged-record toast's words");
    CSF_ASSERT(std::strcmp(Combo_SaveFiles_RefuseText(RSBS_REFUSE_VERSION), "File made by another build") == 0,
               "VERSION does not use the other-build toast's words");

    // ---- B. One row per state -----------------------------------------------
    {
        const ComboSaveFileRow row = Combo_SaveFiles_RowFor(0, CsfEmpty());
        CSF_ASSERT(row.kind == ComboSaveFileKind::Empty, "an absent slot is not Empty");
        CSF_ASSERT(row.file == "File 1", "slot 0 is labelled \"%s\", not \"File 1\"", row.file.c_str());
        CSF_ASSERT(row.status == "Empty", "an absent slot's status is \"%s\"", row.status.c_str());
        CSF_ASSERT(row.name.empty() && row.started.empty() && row.lastPlayed.empty() && row.tooltip.empty(),
                   "an absent slot shows a name, started halves, a last game or a tooltip");
    }
    {
        rsbs::SlotMeta m = CsfEmpty();
        m.hasQuarantine = true;
        const ComboSaveFileRow row = Combo_SaveFiles_RowFor(1, m);
        CSF_ASSERT(row.kind == ComboSaveFileKind::Empty, "an absent slot with a backup is not Empty");
        CSF_ASSERT(row.status == "Empty (backup kept)", "an absent slot with a backup reads \"%s\"",
                   row.status.c_str());
        CSF_ASSERT(row.tooltip.find("backup") != std::string::npos, "the backup has no tooltip: \"%s\"",
                   row.tooltip.c_str());
    }
    {
        const rsbs::SlotMeta m = CsfReady("Link", "Link", GAME_MM);
        const ComboSaveFileRow row = Combo_SaveFiles_RowFor(2, m);
        CSF_ASSERT(row.kind == ComboSaveFileKind::Ready, "a valid file is not Ready");
        CSF_ASSERT(row.file == "File 3", "slot 2 is labelled \"%s\"", row.file.c_str());
        CSF_ASSERT(row.status == "Ready", "a valid file's status is \"%s\"", row.status.c_str());
        CSF_ASSERT(row.name == rsbs::SlotNameLine(m), "the name \"%s\" is not SlotNameLine's \"%s\"",
                   row.name.c_str(), rsbs::SlotNameLine(m).c_str());
        CSF_ASSERT(row.lastPlayed == "MM", "a file last played in Majora's Mask reads \"%s\"",
                   row.lastPlayed.c_str());
        // Both halves started under one name: the name line says it once, so
        // only the Started cell shows the MM half exists (the old [MM v]).
        CSF_ASSERT(row.started == "OoT, MM", "a file with both halves started reads \"%s\"", row.started.c_str());
        CSF_ASSERT(row.tooltip.empty(), "a plain ready file has a tooltip: \"%s\"", row.tooltip.c_str());
    }
    {
        rsbs::SlotMeta m = CsfReady("Zelda", "", GAME_OOT);
        m.commitSkew = 1;
        const ComboSaveFileRow row = Combo_SaveFiles_RowFor(0, m);
        CSF_ASSERT(row.kind == ComboSaveFileKind::Ready && row.lastPlayed == "OoT",
                   "an OoT-last ready file reads kind %d, last \"%s\"", static_cast<int>(row.kind),
                   row.lastPlayed.c_str());
        CSF_ASSERT(row.name == "OoT: Zelda", "an OoT-only file's name is \"%s\"", row.name.c_str());
        CSF_ASSERT(row.started == "OoT", "an OoT-only file's started cell is \"%s\"", row.started.c_str());
        CSF_ASSERT(!row.tooltip.empty(), "a ready file that resumed a newer commit has no tooltip");
    }
    {
        // Refused for its header: exists, not valid; ReadMeta stops before the names.
        rsbs::SlotMeta m = CsfEmpty();
        m.exists = true;
        m.valid = false;
        m.state = RSBS_SLOT_REFUSED;
        m.refuseReason = RSBS_REFUSE_HEADER;
        std::snprintf(m.ootName, sizeof(m.ootName), "%s", "Junk");
        m.ootStarted = true;
        const ComboSaveFileRow row = Combo_SaveFiles_RowFor(0, m);
        CSF_ASSERT(row.kind == ComboSaveFileKind::Refused, "a file refused for its header is not Refused");
        CSF_ASSERT(row.status == std::string("Not paired: ") + Combo_SaveFiles_RefuseText(RSBS_REFUSE_HEADER),
                   "a header refusal reads \"%s\"", row.status.c_str());
        CSF_ASSERT(row.name.empty() && row.started.empty(),
                   "a file refused for its header shows the name \"%s\" or started \"%s\"", row.name.c_str(),
                   row.started.c_str());
        CSF_ASSERT(row.tooltip.find("left in place") != std::string::npos,
                   "a refusal without a backup does not say the file is left in place: \"%s\"",
                   row.tooltip.c_str());
    }
    {
        // A healthy file this session refused (identity), with its evidence aside.
        rsbs::SlotMeta m = CsfReady("Link", "Link", GAME_OOT);
        m.state = RSBS_SLOT_REFUSED;
        m.refuseReason = RSBS_REFUSE_IDENTITY;
        m.hasQuarantine = true;
        const ComboSaveFileRow row = Combo_SaveFiles_RowFor(1, m);
        CSF_ASSERT(row.kind == ComboSaveFileKind::Refused, "a session-refused valid file is not Refused");
        CSF_ASSERT(row.name == rsbs::SlotNameLine(m), "a session-refused valid file lost its name: \"%s\"",
                   row.name.c_str());
        CSF_ASSERT(row.status.rfind("Not paired: ", 0) == 0, "a refusal's status \"%s\" does not open \"Not paired: \"",
                   row.status.c_str());
        CSF_ASSERT(row.tooltip.find("backup") != std::string::npos,
                   "a refusal with a backup does not name the backup: \"%s\"", row.tooltip.c_str());
    }

    // ---- C. The rows through the seam ----------------------------------------
    {
        rsbs::SlotMeta refused = CsfEmpty();
        refused.exists = true;
        refused.state = RSBS_SLOT_REFUSED;
        refused.refuseReason = RSBS_REFUSE_CRC;
        const rsbs::SlotMeta table[2] = { CsfReady("Link", "Link", GAME_OOT), refused };
        Combo_SaveFiles_SetMetaForTest(table, 2);
        ComboSaveFileRow rows[RSBS_SAVE_MAX_SLOTS + 2];
        const int count = Combo_SaveFiles_Rows(rows, RSBS_SAVE_MAX_SLOTS + 2);
        CSF_ASSERT(count == RSBS_SAVE_MAX_SLOTS, "the seam yields %d rows, not %d", count, RSBS_SAVE_MAX_SLOTS);
        if (count == RSBS_SAVE_MAX_SLOTS) {
            CSF_ASSERT(rows[0].kind == ComboSaveFileKind::Ready && rows[0].file == "File 1",
                       "row 1 is not the ready File 1");
            CSF_ASSERT(rows[1].kind == ComboSaveFileKind::Refused && rows[1].file == "File 2",
                       "row 2 is not the refused File 2");
            CSF_ASSERT(rows[2].kind == ComboSaveFileKind::Empty && rows[2].file == "File 3",
                       "a slot past the seam's table is not an empty File 3");
        }
        CSF_ASSERT(Combo_SaveFiles_Rows(rows, 2) == 2, "max 2 does not yield 2 rows");
        CSF_ASSERT(Combo_SaveFiles_Rows(rows, 0) == 0, "max 0 yields rows");
        CSF_ASSERT(Combo_SaveFiles_Rows(nullptr, 3) == 0, "a null output yields rows");

        // Clearing drops the table: a later seam with no rows reads all empty,
        // not the previous table's leftovers.
        Combo_SaveFiles_SetMetaForTest(nullptr, 0);
        Combo_SaveFiles_SetMetaForTest(table, 0);
        const int cleared = Combo_SaveFiles_Rows(rows, RSBS_SAVE_MAX_SLOTS);
        for (int i = 0; i < cleared; i++) {
            CSF_ASSERT(rows[i].kind == ComboSaveFileKind::Empty, "row %d kept a cleared table's state", i + 1);
        }
        Combo_SaveFiles_SetMetaForTest(nullptr, 0);
    }

    // ---- D. The note ---------------------------------------------------------
    {
        ComboSaveFileRow none[RSBS_SAVE_MAX_SLOTS];
        ComboSaveFileRow ready[RSBS_SAVE_MAX_SLOTS];
        ComboSaveFileRow refused[RSBS_SAVE_MAX_SLOTS];
        for (int i = 0; i < RSBS_SAVE_MAX_SLOTS; i++) {
            none[i] = Combo_SaveFiles_RowFor(i, CsfEmpty());
            ready[i] = Combo_SaveFiles_RowFor(i, CsfEmpty());
            refused[i] = Combo_SaveFiles_RowFor(i, CsfEmpty());
        }
        ready[1] = Combo_SaveFiles_RowFor(1, CsfReady("Link", "Link", GAME_OOT));
        rsbs::SlotMeta r = CsfEmpty();
        r.exists = true;
        r.state = RSBS_SLOT_REFUSED;
        r.refuseReason = RSBS_REFUSE_COMMIT_SKEW;
        refused[0] = ready[1];
        refused[2] = Combo_SaveFiles_RowFor(2, r);

        const std::string notes[3] = {
            Combo_SaveFiles_Note(none, RSBS_SAVE_MAX_SLOTS),
            Combo_SaveFiles_Note(ready, RSBS_SAVE_MAX_SLOTS),
            Combo_SaveFiles_Note(refused, RSBS_SAVE_MAX_SLOTS),
        };
        CSF_ASSERT(notes[0].rfind("No save files yet", 0) == 0, "the no-file note is \"%s\"", notes[0].c_str());
        CSF_ASSERT(notes[1].rfind("Each file holds both games", 0) == 0, "the all-ready note is \"%s\"",
                   notes[1].c_str());
        CSF_ASSERT(notes[2].find("not paired") != std::string::npos, "the refused note is \"%s\"", notes[2].c_str());
        for (int i = 0; i < 3; i++) {
            CSF_ASSERT(!notes[i].empty() && notes[i].size() <= 200, "note %d is %zu characters", i, notes[i].size());
            const int sentences = CsfSentences(notes[i]);
            CSF_ASSERT(sentences >= 1 && sentences <= 2, "note %d has %d sentences: \"%s\"", i, sentences,
                       notes[i].c_str());
            for (int j = 0; j < i; j++) {
                CSF_ASSERT(notes[i].substr(0, 12) != notes[j].substr(0, 12),
                           "notes %d and %d open with the same words", j, i);
            }
        }
        CSF_ASSERT(Combo_SaveFiles_Note(nullptr, 0) == notes[0], "no rows at all does not read the no-file note");
    }

    if (failures != 0) {
        printf("[TEST] combo-save-files-view: %d failure(s)\n", failures);
        return TEST_FAIL;
    }
    printf("[TEST] combo-save-files-view: PASS\n");
    return TEST_PASS;
}
