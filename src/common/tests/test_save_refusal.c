/**
 * @file test_save_refusal.c
 * @brief REFUSED-state locks for the unified .redsave (#533).
 *
 * The bug class: a .redsave that failed validation (CRC, truncation, wrong
 * slot, future version) left the session byte-for-byte identical to "this slot
 * is empty", and the next OoT autosave rename-overwrote the mostly-intact
 * original — including the ONLY copy of the MM half — with a blank Tier-1 and
 * an all-zero Tier-3. These tests lock the three #533 mechanisms:
 *
 *   1. QUARANTINE: every refusal renames the failing file aside with a reason
 *      suffix, byte-identical, never overwritten in place (dedupe on repeat).
 *   2. LATCH: the refusing session takes a per-slot write latch; Save() into
 *      an unarmed slot refuses. The counterfactual is authored directly: a
 *      Save() aimed at an un-established slot holding a corrupt fixture must
 *      leave the fixture untouched — revert the latch and that exact call
 *      rename-overwrites it.
 *   3. SURFACE: ReadMeta/GetSlotState report REFUSED as a first-class state,
 *      distinct from ABSENT, both while the failing file is in place and
 *      after it was quarantined away.
 *
 * Linkage note: like test_save_roundtrip.c, this file is #included into
 * test_runner.cpp at FILE SCOPE (compiled as C++, NOT inside extern "C") so it
 * can drive the C++ rsbs::SaveManager API.
 */

#include "../context.h"
#include "../game.h"
#include "../save.h"
#include "../test_runner.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#define REFUSE_ASSERT(cond, msg)                \
    do {                                        \
        if (!(cond)) {                          \
            printf("[TEST] FAIL: %s\n", (msg)); \
            return TEST_FAIL;                   \
        }                                       \
    } while (0)

namespace {

const char* const kRefusalTestDir = "rsbs_test_refusal_saves";

uint8_t RefusalOoTByte(size_t i) {
    return static_cast<uint8_t>(0x9Du + i * 13u);
}
uint8_t RefusalMMByte(size_t i) {
    return static_cast<uint8_t>(0x31u + i * 29u);
}

// Seed gComboCtx + both shadows so Save() has real content to serialize and a
// refused Load has recognizable live state to (not) clobber.
void RefusalSeed(uint32_t tag) {
    Context_InitFrozenStates();
    ComboContext_Init();
    gComboCtx.saveSlot = static_cast<int32_t>(tag);
    gComboCtx.sharedRandoSeed = tag ^ 0x5A5A5A5Au;
    gComboCtx.sourceGame = GAME_OOT;

    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE);
    std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE);
    for (size_t i = 0; i < oot.size(); i++) {
        oot[i] = RefusalOoTByte(i);
    }
    for (size_t i = 0; i < mm.size(); i++) {
        mm[i] = RefusalMMByte(i);
    }
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
}

bool RefusalReadFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool RefusalPatchU32(const std::string& path, size_t offset, uint32_t value) {
    std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!f) {
        return false;
    }
    f.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
    uint8_t bytes[4] = { static_cast<uint8_t>(value & 0xFFu), static_cast<uint8_t>((value >> 8) & 0xFFu),
                         static_cast<uint8_t>((value >> 16) & 0xFFu), static_cast<uint8_t>((value >> 24) & 0xFFu) };
    f.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    return static_cast<bool>(f);
}

bool RefusalFlipPayloadByte(const std::string& path) {
    std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!f) {
        return false;
    }
    const std::streamoff at = static_cast<std::streamoff>(sizeof(rsbs::RsbsSaveHeader)) + 8;
    f.seekg(at, std::ios::beg);
    char b = 0;
    f.read(&b, 1);
    if (!f) {
        return false;
    }
    b = static_cast<char>(b ^ 0xFF);
    f.seekp(at, std::ios::beg);
    f.write(&b, 1);
    return static_cast<bool>(f);
}

// All quarantine files for a slot: "<slotfile>.<...>.bak", sorted for
// deterministic assertions.
std::vector<std::string> RefusalQuarantines(int slot) {
    std::vector<std::string> found;
    const std::filesystem::path slotPath = rsbs::SaveManager::Instance().SlotPath(slot);
    const std::string prefix = slotPath.filename().string();
    std::error_code ec;
    std::filesystem::directory_iterator it(slotPath.parent_path(), ec);
    if (ec) {
        return found;
    }
    for (const auto& entry : it) {
        const std::string name = entry.path().filename().string();
        if (name.size() > prefix.size() && name.compare(0, prefix.size(), prefix) == 0 && name.size() >= 4 &&
            name.compare(name.size() - 4, 4, ".bak") == 0) {
            found.push_back(entry.path().string());
        }
    }
    std::sort(found.begin(), found.end());
    return found;
}

// Wipe the test directory and the per-slot session latches: the fixture setup
// for "a fresh process meets this file".
void RefusalFreshDir() {
    std::error_code ec;
    std::filesystem::remove_all(kRefusalTestDir, ec);
    std::filesystem::create_directories(kRefusalTestDir, ec);
    rsbs::SaveManager::Instance().ResetSlotSessionState();
}

// Author a VALID slot-0 .redsave, then reset the latches so the file looks
// like it was written by a previous session.
bool RefusalAuthorValidSlot(int slot, uint32_t tag) {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    RefusalSeed(tag);
    mgr.ArmSlotOnCreate(slot);
    if (!mgr.Save(slot)) {
        return false;
    }
    mgr.ResetSlotSessionState();
    return true;
}

// The shared per-fixture body: given a corrupt file already at `slot`'s path,
// assert the full refusal contract — refused outcome, live state untouched,
// evidence quarantined byte-identical, latch set, follow-up Save refused and
// destroying nothing.
TestResult RefusalExpectRefusal(int slot, RsbsRefuseReason expectedReason, const char* what) {
    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    const std::string slotPath = mgr.SlotPath(slot);

    std::vector<uint8_t> fixture;
    REFUSE_ASSERT(RefusalReadFile(slotPath, fixture), "could not snapshot the corrupt fixture");

    // Distinct live state, so a refused load that commits anyway is caught.
    RefusalSeed(0x0BADF00Du);
    gComboCtx.saveSlot = 0x0BADF00D;

    printf("[TEST]   fixture: %s\n", what);
    REFUSE_ASSERT(mgr.LoadSlot(slot) == RSBS_LOAD_REFUSED, "corrupt fixture was not REFUSED");
    REFUSE_ASSERT(gComboCtx.saveSlot == 0x0BADF00D, "refused load clobbered live ComboContext");

    // QUARANTINED, not deleted, not left at the slot path.
    REFUSE_ASSERT(!std::filesystem::exists(slotPath), "refused file was left at the slot path");
    std::vector<std::string> baks = RefusalQuarantines(slot);
    REFUSE_ASSERT(baks.size() == 1, "expected exactly one quarantine file");
    REFUSE_ASSERT(baks[0].find(std::string("refused-")) != std::string::npos,
                  "quarantine filename must carry a reason suffix");
    std::vector<uint8_t> quarantined;
    REFUSE_ASSERT(RefusalReadFile(baks[0], quarantined), "could not read the quarantine file");
    REFUSE_ASSERT(quarantined == fixture, "quarantined evidence is not byte-identical to the refused file");

    // REFUSED is a first-class state, distinct from ABSENT, with the reason.
    REFUSE_ASSERT(mgr.GetSlotState(slot) == RSBS_SLOT_REFUSED, "slot must read REFUSED after a refusal");
    REFUSE_ASSERT(mgr.GetSlotRefuseReason(slot) == expectedReason, "refusal reason mismatch");
    REFUSE_ASSERT(!mgr.IsSlotWritable(slot), "refused slot must not be writable");

    // The LATCH: the very next save (the autosave that used to destroy the
    // evidence) must refuse and mint nothing.
    REFUSE_ASSERT(!mgr.Save(slot), "Save into a REFUSED slot must be latched");
    REFUSE_ASSERT(!std::filesystem::exists(slotPath), "a latched Save still wrote the slot path");
    std::vector<uint8_t> after;
    REFUSE_ASSERT(RefusalReadFile(baks[0], after), "quarantine file vanished after the latched Save");
    REFUSE_ASSERT(after == fixture, "the latched Save altered the quarantined evidence");

    return TEST_PASS;
}

}  // namespace

TestResult Test_SaveRefusedQuarantine(void) {
    printf("[TEST] save-refused-quarantine: every refusal quarantines the evidence and latches the slot (#533)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kRefusalTestDir);

    // ---- CRC-corrupt (single flipped payload byte) -----------------------
    RefusalFreshDir();
    REFUSE_ASSERT(RefusalAuthorValidSlot(0, 0xC1C1C1C1u), "could not author the valid base save");
    REFUSE_ASSERT(RefusalFlipPayloadByte(mgr.SlotPath(0)), "could not flip a payload byte");
    if (RefusalExpectRefusal(0, RSBS_REFUSE_CRC, "flipped payload byte (CRC)") != TEST_PASS) {
        return TEST_FAIL;
    }

    // ---- Truncated (short Tier-3, e.g. torn cloud sync) ------------------
    RefusalFreshDir();
    REFUSE_ASSERT(RefusalAuthorValidSlot(0, 0x72C472C4u), "could not author the valid base save");
    {
        std::error_code ec;
        const uintmax_t full = std::filesystem::file_size(mgr.SlotPath(0), ec);
        REFUSE_ASSERT(!ec && full > 4096, "could not stat the authored save");
        std::filesystem::resize_file(mgr.SlotPath(0), full - 2048, ec);
        REFUSE_ASSERT(!ec, "could not truncate the fixture");
    }
    if (RefusalExpectRefusal(0, RSBS_REFUSE_TRUNCATED, "truncated Tier-3") != TEST_PASS) {
        return TEST_FAIL;
    }

    // ---- Wrong slot (slot 2's file copied over slot 1's path) ------------
    // The V13 case: header.slot is stamped but was never compared, so a
    // cloud-sync/manual copy silently attached one pair's identity + MM world
    // to a different OoT file.
    RefusalFreshDir();
    REFUSE_ASSERT(RefusalAuthorValidSlot(2, 0x51075107u), "could not author the valid slot-2 save");
    {
        std::error_code ec;
        std::filesystem::copy_file(mgr.SlotPath(2), mgr.SlotPath(1), ec);
        REFUSE_ASSERT(!ec, "could not copy slot 2's file to slot 1's path");
    }
    if (RefusalExpectRefusal(1, RSBS_REFUSE_WRONG_SLOT, "slot 2's file at slot 1's path") != TEST_PASS) {
        return TEST_FAIL;
    }
    // The donor file is untouched and still valid at its own slot.
    REFUSE_ASSERT(mgr.GetSlotState(2) == RSBS_SLOT_VALID, "the donor slot must remain VALID");
    REFUSE_ASSERT(mgr.HasSave(2), "the donor slot must still read as having a save");

    // ---- Future version (newer build's file opened by this one) ----------
    RefusalFreshDir();
    REFUSE_ASSERT(RefusalAuthorValidSlot(0, 0xF07F07F0u), "could not author the valid base save");
    REFUSE_ASSERT(RefusalPatchU32(mgr.SlotPath(0), offsetof(rsbs::RsbsSaveHeader, version),
                                  RSBS_SAVE_VERSION + 1u),
                  "could not patch the version field");
    if (RefusalExpectRefusal(0, RSBS_REFUSE_VERSION, "future format version") != TEST_PASS) {
        return TEST_FAIL;
    }

    std::error_code ec;
    std::filesystem::remove_all(kRefusalTestDir, ec);
    rsbs::SaveManager::Instance().ResetSlotSessionState();
    printf("[TEST] PASS: CRC/truncation/wrong-slot/future-version each quarantine + latch, evidence intact\n");
    return TEST_PASS;
}

TestResult Test_SaveWriteLatch(void) {
    printf("[TEST] save-write-latch: Save refuses any slot this session did not load, create, or erase (#533)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kRefusalTestDir);

    // ---- The counterfactual, authored directly ---------------------------
    // A corrupt fixture sits at slot 0; the session never opened the slot;
    // an autosave-shaped Save(0) arrives. WITH the latch it refuses and the
    // fixture survives byte-identical IN PLACE. Revert the latch and this
    // exact call rename-overwrites the fixture with a blank Tier-1 +
    // all-zero Tier-3 — the #533 data loss.
    RefusalFreshDir();
    REFUSE_ASSERT(RefusalAuthorValidSlot(0, 0x1A7C41A7u), "could not author the valid base save");
    REFUSE_ASSERT(RefusalFlipPayloadByte(mgr.SlotPath(0)), "could not corrupt the fixture");
    std::vector<uint8_t> fixture;
    REFUSE_ASSERT(RefusalReadFile(mgr.SlotPath(0), fixture), "could not snapshot the fixture");

    RefusalSeed(0xA07A57A0u);  // live state to (not) serialize
    REFUSE_ASSERT(!mgr.IsSlotWritable(0), "an un-established slot must not be writable");
    REFUSE_ASSERT(!mgr.Save(0), "Save into an un-established slot must refuse");

    std::vector<uint8_t> after;
    REFUSE_ASSERT(RefusalReadFile(mgr.SlotPath(0), after), "the fixture vanished — the latched Save destroyed it");
    REFUSE_ASSERT(after == fixture, "the latched Save altered the fixture in place");
    REFUSE_ASSERT(RefusalQuarantines(0).empty(), "a refused Save must not quarantine by itself");

    // ---- Absent slot: still unwritable until established -----------------
    REFUSE_ASSERT(!mgr.Save(2), "Save into a never-opened empty slot must refuse");
    REFUSE_ASSERT(!std::filesystem::exists(mgr.SlotPath(2)), "the refused Save minted a file anyway");

    // ---- The three legitimate arming events ------------------------------
    // (a) Opening an empty slot (the load path's ABSENT outcome).
    REFUSE_ASSERT(mgr.LoadSlot(2) == RSBS_LOAD_ABSENT, "an empty slot must open as ABSENT");
    REFUSE_ASSERT(mgr.IsSlotWritable(2), "opening an empty slot must arm it");
    REFUSE_ASSERT(mgr.Save(2), "Save after establishing the empty slot must succeed");
    REFUSE_ASSERT(mgr.GetSlotState(2) == RSBS_SLOT_VALID, "the established slot's save must be valid");

    // (b) A successful load.
    mgr.ResetSlotSessionState();
    REFUSE_ASSERT(!mgr.IsSlotWritable(2), "a fresh session must start with the slot latched");
    REFUSE_ASSERT(mgr.LoadSlot(2) == RSBS_LOAD_OK, "the valid save must load");
    REFUSE_ASSERT(mgr.IsSlotWritable(2), "a successful load must arm the slot");

    // (c) An explicit erase.
    mgr.ResetSlotSessionState();
    mgr.DeleteSave(2);
    REFUSE_ASSERT(mgr.IsSlotWritable(2), "an explicit erase must arm the slot");
    REFUSE_ASSERT(!std::filesystem::exists(mgr.SlotPath(2)), "erase must remove the slot file");

    // ---- #498/#564: the arrival IDENTITY refusal — latch, no quarantine ----
    // The fourth refusal producer. Unlike every load refusal above, the slot
    // FILE is healthy — the running session's resolved profile diverged from
    // the pair's creation identity — so RefuseSlotIdentity must latch and
    // surface REFUSED while leaving the file byte-identical IN PLACE and
    // minting no quarantine evidence: the healthy .redsave is exactly what
    // the latch is protecting from the divergent session's captures.
    RefusalFreshDir();
    REFUSE_ASSERT(RefusalAuthorValidSlot(0, 0x1DE47177u), "could not author the healthy pair fixture");
    std::vector<uint8_t> healthy;
    REFUSE_ASSERT(RefusalReadFile(mgr.SlotPath(0), healthy), "could not snapshot the healthy fixture");
    REFUSE_ASSERT(mgr.LoadSlot(0) == RSBS_LOAD_OK, "the healthy fixture must load");
    REFUSE_ASSERT(mgr.IsSlotWritable(0), "the loaded slot must be writable before the refusal");

    mgr.RefuseSlotIdentity(0);
    REFUSE_ASSERT(!mgr.IsSlotWritable(0), "identity refusal must latch the slot");
    REFUSE_ASSERT(mgr.GetSlotState(0) == RSBS_SLOT_REFUSED, "identity refusal must surface REFUSED");
    REFUSE_ASSERT(mgr.GetSlotRefuseReason(0) == RSBS_REFUSE_IDENTITY, "the reason must name the identity mismatch");
    REFUSE_ASSERT(!mgr.Save(0), "Save must refuse after an identity refusal");
    {
        std::vector<uint8_t> afterIdentity;
        REFUSE_ASSERT(RefusalReadFile(mgr.SlotPath(0), afterIdentity) && afterIdentity == healthy,
                      "the identity refusal (or its latched Save) altered the HEALTHY slot file");
    }
    REFUSE_ASSERT(RefusalQuarantines(0).empty(), "an identity refusal must not quarantine the healthy file");
    // -1 is the legitimate "no active slot" value the arrival can hand over;
    // it must be a no-op, not an out-of-bounds write.
    mgr.RefuseSlotIdentity(-1);
    // The explicit erase releases it, like every other refusal.
    mgr.DeleteSave(0);
    REFUSE_ASSERT(mgr.IsSlotWritable(0), "erase must release the identity latch");
    REFUSE_ASSERT(mgr.GetSlotRefuseReason(0) == RSBS_REFUSE_NONE, "erase must clear the identity refusal record");

    std::error_code ec;
    std::filesystem::remove_all(kRefusalTestDir, ec);
    mgr.ResetSlotSessionState();
    printf("[TEST] PASS: the latch protects un-established slots; load/create/erase are the only keys; the "
           "identity refusal latches without touching the healthy file\n");
    return TEST_PASS;
}

TestResult Test_SaveArmOnCreate(void) {
    printf("[TEST] save-arm-on-create: file-create quarantines a failing .redsave before its first write (#533)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kRefusalTestDir);

    // ---- Create over a corrupt .redsave preserves the evidence -----------
    // The z_sram.c seam: a new OoT file is created in a slot whose stale
    // .redsave is corrupt and was never load-attempted. The first
    // Save_SaveFile must not rename-overwrite it: ArmSlotOnCreate quarantines
    // first, then arms.
    RefusalFreshDir();
    REFUSE_ASSERT(RefusalAuthorValidSlot(0, 0xCEA7E000u), "could not author the valid base save");
    REFUSE_ASSERT(RefusalFlipPayloadByte(mgr.SlotPath(0)), "could not corrupt the fixture");
    std::vector<uint8_t> fixture;
    REFUSE_ASSERT(RefusalReadFile(mgr.SlotPath(0), fixture), "could not snapshot the fixture");

    RefusalSeed(0x0EA7E001u);
    mgr.ArmSlotOnCreate(0);
    REFUSE_ASSERT(!std::filesystem::exists(mgr.SlotPath(0)), "create must have moved the corrupt file aside");
    {
        std::vector<std::string> baks = RefusalQuarantines(0);
        REFUSE_ASSERT(baks.size() == 1, "create over a corrupt file must quarantine it");
        std::vector<uint8_t> quarantined;
        REFUSE_ASSERT(RefusalReadFile(baks[0], quarantined), "could not read the quarantine file");
        REFUSE_ASSERT(quarantined == fixture, "create's quarantine is not byte-identical to the evidence");
    }
    REFUSE_ASSERT(mgr.IsSlotWritable(0), "create must arm the slot");
    REFUSE_ASSERT(mgr.Save(0), "the new file's first save must commit");
    REFUSE_ASSERT(mgr.GetSlotState(0) == RSBS_SLOT_VALID, "the new save must be valid");
    {
        // The new save did not disturb the evidence.
        std::vector<std::string> baks = RefusalQuarantines(0);
        REFUSE_ASSERT(baks.size() == 1, "the first save must not touch the quarantine");
        std::vector<uint8_t> quarantined;
        REFUSE_ASSERT(RefusalReadFile(baks[0], quarantined) && quarantined == fixture,
                      "the first save altered the quarantined evidence");
    }

    // ---- Sticky refusal + dedupe: evidence is NEVER overwritten ----------
    RefusalFreshDir();
    REFUSE_ASSERT(RefusalAuthorValidSlot(0, 0x57ECC000u), "could not author the valid base save");
    REFUSE_ASSERT(RefusalFlipPayloadByte(mgr.SlotPath(0)), "could not corrupt the fixture");
    REFUSE_ASSERT(mgr.LoadSlot(0) == RSBS_LOAD_REFUSED, "first refusal");
    // The slot path is now empty, but re-opening it must NOT flip to
    // ABSENT-and-armed: the refusal is sticky for the session.
    REFUSE_ASSERT(mgr.LoadSlot(0) == RSBS_LOAD_REFUSED, "a refused slot must stay REFUSED on re-open");
    REFUSE_ASSERT(!mgr.IsSlotWritable(0), "a refused slot must stay latched on re-open");
    // Cloud sync re-materializes another corrupt file: refusing it must
    // quarantine to a SECOND name, not overwrite the first evidence.
    REFUSE_ASSERT(RefusalAuthorValidSlot(0, 0x57ECC001u), "could not re-author the slot file");
    // (Re-authoring reset the latches; corrupt it and refuse again.)
    REFUSE_ASSERT(RefusalFlipPayloadByte(mgr.SlotPath(0)), "could not corrupt the re-authored file");
    REFUSE_ASSERT(mgr.LoadSlot(0) == RSBS_LOAD_REFUSED, "second refusal");
    REFUSE_ASSERT(RefusalQuarantines(0).size() == 2, "the second refusal must dedupe, never overwrite evidence");

    // ---- The explicit erase releases everything --------------------------
    mgr.DeleteSave(0);
    REFUSE_ASSERT(mgr.GetSlotState(0) == RSBS_SLOT_ABSENT, "erase must clear the REFUSED state");
    REFUSE_ASSERT(RefusalQuarantines(0).empty(), "erase is the sanctioned disposal of quarantined evidence");
    REFUSE_ASSERT(mgr.IsSlotWritable(0), "erase must arm the slot");

    std::error_code ec;
    std::filesystem::remove_all(kRefusalTestDir, ec);
    mgr.ResetSlotSessionState();
    printf("[TEST] PASS: create quarantines before first write; refusals are sticky; evidence dedupes\n");
    return TEST_PASS;
}

TestResult Test_SaveRefusedMeta(void) {
    printf("[TEST] save-refused-meta: the slot surface reports REFUSED distinctly from empty (#533)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kRefusalTestDir);

    // ---- Empty slot: ABSENT, no quarantine -------------------------------
    RefusalFreshDir();
    {
        rsbs::SlotMeta meta = mgr.ReadMeta(0);
        REFUSE_ASSERT(meta.state == RSBS_SLOT_ABSENT && !meta.hasQuarantine && !meta.exists,
                      "an empty slot must read ABSENT with no quarantine");
    }

    // ---- In-place header-refused file (never load-attempted) -------------
    {
        std::ofstream out(mgr.SlotPath(0), std::ios::binary | std::ios::trunc);
        const char junk[64] = "NOTASAVE-this is not a redsave header at all............";
        out.write(junk, sizeof(junk));
    }
    {
        rsbs::SlotMeta meta = mgr.ReadMeta(0);
        REFUSE_ASSERT(meta.state == RSBS_SLOT_REFUSED, "a failing file in place must read REFUSED, not empty");
        REFUSE_ASSERT(meta.refuseReason == RSBS_REFUSE_HEADER, "the header reason must surface");
        REFUSE_ASSERT(meta.exists, "the failing file does exist");
        REFUSE_ASSERT(mgr.GetSlotState(0) == RSBS_SLOT_REFUSED, "GetSlotState must agree");
    }

    // ---- After the load attempt: quarantined, still REFUSED --------------
    REFUSE_ASSERT(mgr.LoadSlot(0) == RSBS_LOAD_REFUSED, "the junk file must refuse");
    {
        rsbs::SlotMeta meta = mgr.ReadMeta(0);
        REFUSE_ASSERT(meta.state == RSBS_SLOT_REFUSED, "a quarantined slot must STILL read REFUSED");
        REFUSE_ASSERT(!meta.exists, "the slot path is empty after quarantine");
        REFUSE_ASSERT(meta.hasQuarantine, "the quarantine evidence must be reported");
    }

    // ---- A fresh process: evidence outlives the session record -----------
    mgr.ResetSlotSessionState();
    {
        rsbs::SlotMeta meta = mgr.ReadMeta(0);
        REFUSE_ASSERT(meta.state == RSBS_SLOT_ABSENT, "a fresh session sees the empty slot as ABSENT");
        REFUSE_ASSERT(meta.hasQuarantine, "…but the on-disk quarantine evidence still surfaces");
    }

    // ---- Wrong-slot file in place surfaces before any load --------------
    RefusalFreshDir();
    REFUSE_ASSERT(RefusalAuthorValidSlot(2, 0x33E7A000u), "could not author the valid slot-2 save");
    {
        std::error_code ec;
        std::filesystem::copy_file(mgr.SlotPath(2), mgr.SlotPath(1), ec);
        REFUSE_ASSERT(!ec, "could not copy slot 2's file to slot 1's path");
    }
    {
        rsbs::SlotMeta meta = mgr.ReadMeta(1);
        REFUSE_ASSERT(meta.state == RSBS_SLOT_REFUSED, "a wrong-slot file must read REFUSED in place");
        REFUSE_ASSERT(meta.refuseReason == RSBS_REFUSE_WRONG_SLOT, "the wrong-slot reason must surface");
        rsbs::SlotMeta donor = mgr.ReadMeta(2);
        REFUSE_ASSERT(donor.state == RSBS_SLOT_VALID, "the donor slot must stay VALID");
    }

    std::error_code ec;
    std::filesystem::remove_all(kRefusalTestDir, ec);
    mgr.ResetSlotSessionState();
    printf("[TEST] PASS: ABSENT / VALID / REFUSED are three different surfaced facts\n");
    return TEST_PASS;
}
