/**
 * @file test_combo_save_files_view.c
 * @brief ROM-free lock for the Combo > Save Files page's model
 *        (src/common/combo_save_files_view.cpp).
 *
 * CTest row ComboSaveFilesView ("combo-save-files-view", label "redship").
 * Display-free and disk-free: every row is built from a synthetic
 * rsbs::SlotMeta, directly or through the page's test seam, and the
 * SaveManager calls below (the refusal record, its words, the epoch) never
 * touch a file, so the player's own Save folder is never read.
 *
 * WHAT IT ASSERTS:
 *   A. Every refusal reason has the page's own words: non-empty, no issue
 *      number, no trailing period, distinct from the developer label on stderr,
 *      and short enough that "Refused: <reason>" stays one table cell's line
 *      or two (at most 45 characters). NONE and an out-of-range value read the
 *      fallback.
 *   A2. A refusal that posted a toast shows the TOAST's words, keyed to what
 *      the toast said, not to the reason code: each of the load's three
 *      IDENTITY toasts (rules differ, another build, damaged record) gives a
 *      status equal to the toast line itself (RSBS_REFUSAL_TOAST_PREFIX + its
 *      message), so three toasts on one reason code give three statuses; each of
 *      the arrival's and the MM spoiler's toasts gives its own toast line too:
 *      one prefix, and every message capitalized without a period (#836). (paired-load-restore legs
 *      3 and 4 lock the recording itself on a real load and a real arrival.)
 *   A3. The SaveManager's record of those words: kept only while the slot is
 *      refused, cleared by every change of the refusal record, and each change
 *      moves SlotStateEpoch.
 *   B. One row per state: a slot with no record (never "Empty": an OoT file may
 *      be there); the same with a backup kept, whose tooltip names the reason
 *      the backup's file name carries (the restart case) or a generic one; a
 *      ready file (its name is rsbs::SlotNameLine's, its last game, no
 *      tooltip); a ready file whose last load resumed a newer commit (a
 *      tooltip); a file refused for its header (no name: its bytes are not
 *      read); a healthy file this session refused (its name still shows) with
 *      and without a backup (the tooltip says which).
 *   C. The cached view: one read serves any number of calls; it re-reads when
 *      the page is opened, when the seam changes and when SlotStateEpoch
 *      moves, and at no other call; the rows come in slot order with "File N"
 *      labels, a short table reads as slots with no record, and clearing the
 *      seam drops the table.
 *   D. The note: one case per situation (no record anywhere, every file pairs,
 *      one does not, a backup kept), each opening with different words, each
 *      within the note budget (R-X1: two sentences, 200 characters), none
 *      claiming there are no save files, and the refused note saying what a
 *      refused file does (it does not open, #836) and the two ways out (a good
 *      copy of its record put back, or the erase).
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++).
 */

#include <cstdio>
#include <cstring>
#include <string>

#include "../combo_save_files_view.h"
#include "../pairing_refusal_toast.h"
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

/** A healthy file this session refused as IDENTITY, with `words` recorded. */
rsbs::SlotMeta CsfRefusedWith(const char* words) {
    rsbs::SlotMeta m = CsfReady("Link", "Link", GAME_OOT);
    m.state = RSBS_SLOT_REFUSED;
    m.refuseReason = RSBS_REFUSE_IDENTITY;
    std::snprintf(m.refuseWords, sizeof(m.refuseWords), "%s", words != nullptr ? words : "");
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

    // ---- A. Every refusal reason has the page's own words ----------------------
    for (int r = RSBS_REFUSE_UNREADABLE; r <= RSBS_REFUSE_MISSING; r++) {
        const RsbsRefuseReason reason = static_cast<RsbsRefuseReason>(r);
        const char* text = Combo_SaveFiles_RefuseText(reason);
        CSF_ASSERT(text != nullptr && text[0] != '\0', "refusal reason %d has no player words", r);
        if (text == nullptr || text[0] == '\0') {
            continue;
        }
        const std::string cell = std::string(RSBS_REFUSAL_TOAST_PREFIX) + " " + text;
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

    // ---- A2. A refusal that posted a toast shows the toast's words -------------
    {
        // The load's three IDENTITY toasts: one reason code, three toasts, and
        // the page must say whichever one the player saw.
        const int loadKinds[3] = { RSBS_LOAD_TOAST_REFUSED_RULES, RSBS_LOAD_TOAST_REFUSED_OTHER_BUILD,
                                   RSBS_LOAD_TOAST_REFUSED_DAMAGED };
        std::string statuses[3];
        for (int i = 0; i < 3; i++) {
            const char* message = RsbsSave_LoadToastRefusalMessage(loadKinds[i]);
            CSF_ASSERT(message != nullptr && message[0] != '\0', "load toast kind %d has no message", loadKinds[i]);
            if (message == nullptr) {
                continue;
            }
            // The toast line as the overlay draws it: the prefix + " " + message
            // (RsbsSave_EmitLoadToast's prefix for every refusal kind).
            const std::string toastLine = std::string(RSBS_REFUSAL_TOAST_PREFIX) + " " + message;
            const ComboSaveFileRow row = Combo_SaveFiles_RowFor(1, CsfRefusedWith(message));
            statuses[i] = row.status;
            CSF_ASSERT(row.kind == ComboSaveFileKind::Refused && row.status == toastLine,
                       "the load toast \"%s\" gives the page status \"%s\"", toastLine.c_str(), row.status.c_str());
        }
        CSF_ASSERT(statuses[0] != statuses[1] && statuses[1] != statuses[2] && statuses[0] != statuses[2],
                   "three load toasts on one reason code give the same page status");
        CSF_ASSERT(RsbsSave_LoadToastRefusalMessage(RSBS_LOAD_TOAST_RULES_RESTORED) == nullptr,
                   "a restore toast has refusal words");

        // The arrival's and the MM spoiler's toasts, through the production copy
        // (Combo_PairingRefusalToastMessage): the page repeats the toast's own
        // line, prefix and message, with nothing trimmed (#836).
        const int pairingKinds[4] = { RSBS_PAIRING_REFUSAL_MM_OPTIONS, RSBS_PAIRING_REFUSAL_RULES,
                                      RSBS_PAIRING_REFUSAL_MISSING_HALF, RSBS_PAIRING_REFUSAL_SPOILER };
        const char* details[4] = { nullptr, "Goal", nullptr, RSBS_SPOILER_REFUSAL_OTHER_SEED };
        for (int i = 0; i < 4; i++) {
            char message[256];
            Combo_PairingRefusalToastMessage(pairingKinds[i], details[i], message, sizeof(message));
            CSF_ASSERT(message[0] != '\0', "pairing toast kind %d has no message", pairingKinds[i]);
            const ComboSaveFileRow row = Combo_SaveFiles_RowFor(1, CsfRefusedWith(message));
            const std::string want = std::string(Combo_PairingRefusalToastPrefix(pairingKinds[i])) + " " + message;
            CSF_ASSERT(row.status == want, "the pairing toast \"%s\" gives the page status \"%s\" (want \"%s\")",
                       message, row.status.c_str(), want.c_str());
            CSF_ASSERT(message[std::strlen(message) - 1] != '.' && !(message[0] >= 'a' && message[0] <= 'z'),
                       "the pairing toast \"%s\" is not capitalized without a trailing period", message);
        }

        // No words recorded (a refusal with no toast): the page's own words.
        const ComboSaveFileRow bare = Combo_SaveFiles_RowFor(1, CsfRefusedWith(""));
        CSF_ASSERT(bare.status ==
                       std::string(RSBS_REFUSAL_TOAST_PREFIX) + " " + Combo_SaveFiles_RefuseText(RSBS_REFUSE_IDENTITY),
                   "a refusal with no recorded words reads \"%s\"", bare.status.c_str());
    }

    // ---- A3. The SaveManager's record of the words -----------------------------
    {
        rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
        mgr.ResetSlotSessionState();
        const int slot = 2;
        uint32_t epoch = mgr.SlotStateEpoch();
        mgr.NoteSlotRefusalWords(slot, "ignored");
        CSF_ASSERT(std::strcmp(mgr.GetSlotRefusalWords(slot), "") == 0,
                   "words were kept for a slot that is not refused: \"%s\"", mgr.GetSlotRefusalWords(slot));
        CSF_ASSERT(mgr.SlotStateEpoch() == epoch, "a no-op note moved the epoch");

        mgr.RefuseSlotIdentity(slot);
        CSF_ASSERT(mgr.SlotStateEpoch() != epoch, "a refusal did not move the epoch");
        epoch = mgr.SlotStateEpoch();
        mgr.NoteSlotRefusalWords(slot, RsbsSave_LoadToastRefusalMessage(RSBS_LOAD_TOAST_REFUSED_DAMAGED));
        CSF_ASSERT(std::strcmp(mgr.GetSlotRefusalWords(slot), "Cross-game record is damaged") == 0,
                   "the refused slot's words are \"%s\"", mgr.GetSlotRefusalWords(slot));
        CSF_ASSERT(mgr.SlotStateEpoch() != epoch, "noting words did not move the epoch");

        // A later refusal replaces the record, and its words with it.
        mgr.RefuseSlotGeneration(slot);
        CSF_ASSERT(std::strcmp(mgr.GetSlotRefusalWords(slot), "") == 0,
                   "a new refusal kept the previous refusal's words \"%s\"", mgr.GetSlotRefusalWords(slot));
        mgr.NoteSlotRefusalWords(slot, Combo_RefusalWords(RSBS_REFUSAL_WORDS_NO_MM_WORLD));
        epoch = mgr.SlotStateEpoch();
        mgr.ResetSlotSessionState();
        CSF_ASSERT(std::strcmp(mgr.GetSlotRefusalWords(slot), "") == 0,
                   "releasing the refusal kept its words \"%s\"", mgr.GetSlotRefusalWords(slot));
        CSF_ASSERT(mgr.SlotStateEpoch() != epoch, "releasing the refusal did not move the epoch");
        mgr.NoteSlotRefusalWords(-1, "out of range");
        mgr.NoteSlotRefusalWords(RSBS_SAVE_MAX_SLOTS, "out of range");
    }

    // ---- B. One row per state -----------------------------------------------
    {
        const ComboSaveFileRow row = Combo_SaveFiles_RowFor(0, CsfEmpty());
        CSF_ASSERT(row.kind == ComboSaveFileKind::Empty, "an absent slot is not Empty");
        CSF_ASSERT(row.file == "File 1", "slot 0 is labelled \"%s\", not \"File 1\"", row.file.c_str());
        CSF_ASSERT(row.status == "No cross-game record", "an absent slot's status is \"%s\"", row.status.c_str());
        CSF_ASSERT(row.status.find("Empty") == std::string::npos, "an absent slot claims the slot is empty");
        CSF_ASSERT(row.name.empty() && row.lastPlayed.empty() && row.tooltip.empty() && !row.backupKept,
                   "an absent slot shows a name, a last game, a tooltip or a backup");
    }
    {
        // After a restart: no record, no session refusal, only the evidence.
        rsbs::SlotMeta m = CsfEmpty();
        m.hasQuarantine = true;
        m.quarantineReason = RSBS_REFUSE_CRC;
        const ComboSaveFileRow row = Combo_SaveFiles_RowFor(1, m);
        CSF_ASSERT(row.kind == ComboSaveFileKind::Empty && row.backupKept,
                   "an absent slot with a backup is not a record-less row with a backup");
        CSF_ASSERT(row.status == "No cross-game record (backup kept)", "an absent slot with a backup reads \"%s\"",
                   row.status.c_str());
        CSF_ASSERT(row.tooltip.find(Combo_SaveFiles_RefuseText(RSBS_REFUSE_CRC)) != std::string::npos,
                   "the backup's tooltip does not name the reason its file name carries: \"%s\"", row.tooltip.c_str());
        m.quarantineReason = RSBS_REFUSE_NONE;
        const ComboSaveFileRow unnamed = Combo_SaveFiles_RowFor(1, m);
        CSF_ASSERT(unnamed.tooltip.find("backup") != std::string::npos &&
                       unnamed.tooltip.find(Combo_SaveFiles_RefuseText(RSBS_REFUSE_CRC)) == std::string::npos,
                   "a backup with no readable reason has the tooltip \"%s\"", unnamed.tooltip.c_str());
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
        CSF_ASSERT(row.status ==
                       std::string(RSBS_REFUSAL_TOAST_PREFIX) + " " + Combo_SaveFiles_RefuseText(RSBS_REFUSE_HEADER),
                   "a header refusal reads \"%s\"", row.status.c_str());
        CSF_ASSERT(row.name.empty() && row.lastPlayed.empty(),
                   "a file refused for its header shows the name \"%s\" or last game \"%s\"", row.name.c_str(),
                   row.lastPlayed.c_str());
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
        CSF_ASSERT(row.status.rfind(std::string(RSBS_REFUSAL_TOAST_PREFIX) + " ", 0) == 0,
                   "a refusal's status \"%s\" does not open \"%s \"", row.status.c_str(), RSBS_REFUSAL_TOAST_PREFIX);
        CSF_ASSERT(row.tooltip.find("backup") != std::string::npos,
                   "a refusal with a backup does not name the backup: \"%s\"", row.tooltip.c_str());
    }

    // ---- C. The cached view ------------------------------------------------------
    {
        rsbs::SlotMeta refused = CsfEmpty();
        refused.exists = true;
        refused.state = RSBS_SLOT_REFUSED;
        refused.refuseReason = RSBS_REFUSE_CRC;
        const rsbs::SlotMeta table[2] = { CsfReady("Link", "Link", GAME_OOT), refused };
        Combo_SaveFiles_SetMetaForTest(table, 2);

        const int before = Combo_SaveFiles_ReadCountForTest();
        const ComboSaveFilesView& first = Combo_SaveFiles_View(true);
        CSF_ASSERT(Combo_SaveFiles_ReadCountForTest() == before + 1, "opening the page did not read (%d -> %d)",
                   before, Combo_SaveFiles_ReadCountForTest());
        CSF_ASSERT(first.count == RSBS_SAVE_MAX_SLOTS, "the view has %d rows, not %d", first.count,
                   RSBS_SAVE_MAX_SLOTS);
        if (first.count == RSBS_SAVE_MAX_SLOTS) {
            CSF_ASSERT(first.rows[0].kind == ComboSaveFileKind::Ready && first.rows[0].file == "File 1",
                       "row 1 is not the ready File 1");
            CSF_ASSERT(first.rows[1].kind == ComboSaveFileKind::Refused && first.rows[1].file == "File 2",
                       "row 2 is not the refused File 2");
            CSF_ASSERT(first.rows[2].kind == ComboSaveFileKind::Empty && first.rows[2].file == "File 3",
                       "a slot past the seam's table is not a record-less File 3");
        }
        CSF_ASSERT(first.note == Combo_SaveFiles_Note(first.rows, first.count),
                   "the view's note is not the note of its own rows");

        // Many frames of the page drawing its note and its table: no read.
        for (int frame = 0; frame < 120; frame++) {
            (void)Combo_SaveFiles_View(false); // the note's PreFunc
            (void)Combo_SaveFiles_View(false); // the table
        }
        CSF_ASSERT(Combo_SaveFiles_ReadCountForTest() == before + 1,
                   "240 draws of an unchanged page read the source %d times, not once",
                   Combo_SaveFiles_ReadCountForTest() - before);

        // The save state moves (a refusal recorded): one read, then quiet again.
        rsbs::SaveManager::Instance().RefuseSlotIdentity(0);
        (void)Combo_SaveFiles_View(false);
        (void)Combo_SaveFiles_View(false);
        CSF_ASSERT(Combo_SaveFiles_ReadCountForTest() == before + 2,
                   "a moved SlotStateEpoch gave %d reads, not one", Combo_SaveFiles_ReadCountForTest() - before - 1);
        rsbs::SaveManager::Instance().ResetSlotSessionState();

        // Opening the page again reads again.
        (void)Combo_SaveFiles_View(false); // the reset above moved the epoch
        const int settled = Combo_SaveFiles_ReadCountForTest();
        (void)Combo_SaveFiles_View(true);
        CSF_ASSERT(Combo_SaveFiles_ReadCountForTest() == settled + 1, "reopening the page did not read");

        // Clearing drops the table: a later seam with no rows reads all
        // record-less, not the previous table's leftovers (the seam change
        // alone makes the view read).
        Combo_SaveFiles_SetMetaForTest(nullptr, 0);
        Combo_SaveFiles_SetMetaForTest(table, 0);
        const ComboSaveFilesView& cleared = Combo_SaveFiles_View(false);
        CSF_ASSERT(Combo_SaveFiles_ReadCountForTest() == settled + 2, "a changed seam did not read");
        for (int i = 0; i < cleared.count; i++) {
            CSF_ASSERT(cleared.rows[i].kind == ComboSaveFileKind::Empty, "row %d kept a cleared table's state", i + 1);
        }
        Combo_SaveFiles_SetMetaForTest(nullptr, 0);
    }

    // ---- D. The note ---------------------------------------------------------
    {
        ComboSaveFileRow none[RSBS_SAVE_MAX_SLOTS];
        ComboSaveFileRow ready[RSBS_SAVE_MAX_SLOTS];
        ComboSaveFileRow refused[RSBS_SAVE_MAX_SLOTS];
        ComboSaveFileRow backup[RSBS_SAVE_MAX_SLOTS];
        rsbs::SlotMeta aside = CsfEmpty();
        aside.hasQuarantine = true;
        aside.quarantineReason = RSBS_REFUSE_COMMIT_SKEW;
        for (int i = 0; i < RSBS_SAVE_MAX_SLOTS; i++) {
            none[i] = Combo_SaveFiles_RowFor(i, CsfEmpty());
            ready[i] = Combo_SaveFiles_RowFor(i, CsfEmpty());
            refused[i] = Combo_SaveFiles_RowFor(i, CsfEmpty());
            backup[i] = Combo_SaveFiles_RowFor(i, i == 1 ? aside : CsfEmpty());
        }
        ready[1] = Combo_SaveFiles_RowFor(1, CsfReady("Link", "Link", GAME_OOT));
        rsbs::SlotMeta r = CsfEmpty();
        r.exists = true;
        r.state = RSBS_SLOT_REFUSED;
        r.refuseReason = RSBS_REFUSE_COMMIT_SKEW;
        refused[0] = ready[1];
        refused[2] = Combo_SaveFiles_RowFor(2, r);

        const std::string notes[4] = {
            Combo_SaveFiles_Note(none, RSBS_SAVE_MAX_SLOTS),
            Combo_SaveFiles_Note(ready, RSBS_SAVE_MAX_SLOTS),
            Combo_SaveFiles_Note(refused, RSBS_SAVE_MAX_SLOTS),
            Combo_SaveFiles_Note(backup, RSBS_SAVE_MAX_SLOTS),
        };
        CSF_ASSERT(notes[0].rfind("No cross-game record yet", 0) == 0, "the no-record note is \"%s\"",
                   notes[0].c_str());
        CSF_ASSERT(notes[1].rfind("Each file holds both games", 0) == 0, "the all-ready note is \"%s\"",
                   notes[1].c_str());
        // What a refusal does (#836, operator ruling 2026-10-01): the file does
        // not open. Never the retired unpaired-play words.
        CSF_ASSERT(notes[2].rfind("A refused file does not open", 0) == 0, "the refused note is \"%s\"",
                   notes[2].c_str());
        CSF_ASSERT(notes[2].find("good copy") != std::string::npos && notes[2].find("erase") != std::string::npos &&
                       notes[2].find("un-randomized") == std::string::npos &&
                       notes[2].find("saves nothing") == std::string::npos,
                   "the refused note does not name the two ways out of a refused file: \"%s\"", notes[2].c_str());
        CSF_ASSERT(notes[3].find("backup") != std::string::npos, "the backup note is \"%s\"", notes[3].c_str());
        for (int i = 0; i < 4; i++) {
            CSF_ASSERT(!notes[i].empty() && notes[i].size() <= 200, "note %d is %zu characters", i, notes[i].size());
            const int sentences = CsfSentences(notes[i]);
            CSF_ASSERT(sentences >= 1 && sentences <= 2, "note %d has %d sentences: \"%s\"", i, sentences,
                       notes[i].c_str());
            CSF_ASSERT(notes[i].find("No save files") == std::string::npos,
                       "note %d claims there are no save files: \"%s\"", i, notes[i].c_str());
            for (int j = 0; j < i; j++) {
                CSF_ASSERT(notes[i].substr(0, 12) != notes[j].substr(0, 12),
                           "notes %d and %d open with the same words", j, i);
            }
        }
        CSF_ASSERT(Combo_SaveFiles_Note(nullptr, 0) == notes[0], "no rows at all does not read the no-record note");
    }

    if (failures != 0) {
        printf("[TEST] combo-save-files-view: %d failure(s)\n", failures);
        return TEST_FAIL;
    }
    printf("[TEST] combo-save-files-view: PASS\n");
    return TEST_PASS;
}
