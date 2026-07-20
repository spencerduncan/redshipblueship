/**
 * @file test_save_roundtrip.c
 * @brief Headless unit tests for the unified .redsave format (Phase 2 T6, #35).
 *
 * Exercises rsbs::SaveManager end-to-end with NO ROMs and NO display: seed the
 * cross-game shadow copies + gComboCtx, Save to a temp directory, wipe live
 * state, Load, and assert a byte-exact round-trip — plus the header contents and
 * the fail-safe rejection paths (bad version, bad tier size, corrupt CRC), each
 * of which must leave live state untouched.
 *
 * Linkage note: like test_roundtrip_integrity.c / test_archive_hotswap.c, this
 * file is #included into test_runner.cpp at FILE SCOPE (compiled as C++, NOT
 * inside an extern "C" block) so it can call the C++ rsbs::SaveManager API. The
 * Test_Save* entry points keep C++ linkage; they are referenced only from this
 * TU's gTests[] table.
 */

#include "../context.h"
#include "../game.h"
#include "../save.h"
#include "../test_runner.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#define SAVE_ASSERT(cond, msg)                  \
    do {                                        \
        if (!(cond)) {                          \
            printf("[TEST] FAIL: %s\n", (msg)); \
            return TEST_FAIL;                   \
        }                                       \
    } while (0)

namespace {

const char* const kSaveTestDir = "rsbs_test_saves";

// Deterministic, position-dependent fill so a swapped/truncated byte is obvious.
uint8_t SaveTestOoTByte(size_t i) {
    return static_cast<uint8_t>(0xA1u + i * 31u);
}
uint8_t SaveTestMMByte(size_t i) {
    return static_cast<uint8_t>(0x5Cu + i * 17u);
}

// Seed gComboCtx + both shadow copies with recognizable, distinct content.
void SaveTestSeed(uint32_t tag) {
    Context_InitFrozenStates();
    ComboContext_Init();  // stamps magic "OoT+MM<3" + version, zeroes the rest
    gComboCtx.saveSlot = static_cast<int32_t>(tag);
    gComboCtx.sharedFlags[5] = tag;
    gComboCtx.sharedRandoSeed = tag ^ 0xA5A5A5A5u;
    gComboCtx.sourceGame = GAME_OOT;
    gComboCtx.sourceIsRando = true;

    std::vector<uint8_t> oot(OOT_SAVE_CONTEXT_SIZE);
    std::vector<uint8_t> mm(MM_SAVE_CONTEXT_SIZE);
    for (size_t i = 0; i < oot.size(); i++) {
        oot[i] = SaveTestOoTByte(i);
    }
    for (size_t i = 0; i < mm.size(); i++) {
        mm[i] = SaveTestMMByte(i);
    }
    Context_UpdateShadowCopy(GAME_OOT, oot.data(), oot.size());
    Context_UpdateShadowCopy(GAME_MM, mm.data(), mm.size());
}

// Overwrite a 4-byte little-endian field at the given header offset, in place,
// without disturbing the rest of the file. Used to craft fail-safe inputs.
bool SaveTestPatchU32(const std::string& path, size_t offset, uint32_t value) {
    std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!f) {
        return false;
    }
    f.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
    uint8_t bytes[4] = {static_cast<uint8_t>(value & 0xFFu), static_cast<uint8_t>((value >> 8) & 0xFFu),
                        static_cast<uint8_t>((value >> 16) & 0xFFu), static_cast<uint8_t>((value >> 24) & 0xFFu)};
    f.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    return static_cast<bool>(f);
}

// Flip one payload byte (after the 32-byte header) to break the CRC.
bool SaveTestFlipPayloadByte(const std::string& path) {
    std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!f) {
        return false;
    }
    const std::streamoff at = static_cast<std::streamoff>(sizeof(rsbs::RsbsSaveHeader)) + 4;
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

bool SaveTestOoTShadowMatchesPattern() {
    const uint8_t* p = static_cast<const uint8_t*>(Context_GetOoTSaveContext());
    if (p == nullptr) {
        return false;
    }
    for (size_t i = 0; i < OOT_SAVE_CONTEXT_SIZE; i++) {
        if (p[i] != SaveTestOoTByte(i)) {
            return false;
        }
    }
    return true;
}

bool SaveTestMMShadowMatchesPattern() {
    const uint8_t* p = static_cast<const uint8_t*>(Context_GetMMSaveContext());
    if (p == nullptr) {
        return false;
    }
    for (size_t i = 0; i < MM_SAVE_CONTEXT_SIZE; i++) {
        if (p[i] != SaveTestMMByte(i)) {
            return false;
        }
    }
    return true;
}

}  // namespace

TestResult Test_SaveRoundtripTiers(void) {
    printf("[TEST] save-roundtrip-tiers: .redsave preserves ComboContext + both SaveContexts (#35)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir);

    const uint32_t tag = 0x1234ABCDu;
    SaveTestSeed(tag);
    mgr.DeleteSave(0);

    SAVE_ASSERT(mgr.Save(0), "Save(0) failed");

    // Wipe live state so a no-op Load could not pass by accident.
    ComboContext_Init();
    Context_ClearAllFrozenStates();
    gComboCtx.saveSlot = 0x7FFFFFFF;

    SAVE_ASSERT(mgr.Load(0), "Load(0) failed");

    SAVE_ASSERT(gComboCtx.saveSlot == static_cast<int32_t>(tag), "ComboContext.saveSlot not restored");
    SAVE_ASSERT(gComboCtx.sharedFlags[5] == tag, "ComboContext.sharedFlags not restored");
    SAVE_ASSERT(gComboCtx.sharedRandoSeed == (tag ^ 0xA5A5A5A5u), "ComboContext.sharedRandoSeed not restored");
    SAVE_ASSERT(gComboCtx.sourceIsRando, "ComboContext.sourceIsRando not restored");
    SAVE_ASSERT(gComboCtx.sourceGame == GAME_OOT, "ComboContext.sourceGame not restored");
    SAVE_ASSERT(SaveTestOoTShadowMatchesPattern(), "OoT SaveContext blob not byte-identical after round-trip");
    SAVE_ASSERT(SaveTestMMShadowMatchesPattern(), "MM SaveContext blob not byte-identical after round-trip");

    mgr.DeleteSave(0);
    printf("[TEST] PASS: three-tier round-trip is byte-exact\n");
    return TEST_PASS;
}

TestResult Test_SaveHeader(void) {
    printf("[TEST] save-header: .redsave header fields + CRC are well-formed (#35)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir);
    SaveTestSeed(0x55AA1234u);
    mgr.DeleteSave(0);
    SAVE_ASSERT(mgr.Save(0), "Save(0) failed");

    std::ifstream in(mgr.SlotPath(0), std::ios::binary);
    SAVE_ASSERT(static_cast<bool>(in), "could not reopen written slot");

    rsbs::RsbsSaveHeader h;
    in.read(reinterpret_cast<char*>(&h), sizeof(h));
    SAVE_ASSERT(in.gcount() == static_cast<std::streamsize>(sizeof(h)), "short header read");

    SAVE_ASSERT(std::memcmp(h.magic, RSBS_SAVE_MAGIC, sizeof(h.magic)) == 0, "bad magic");
    SAVE_ASSERT(h.version == RSBS_SAVE_VERSION, "bad version");
    SAVE_ASSERT(h.endian == RSBS_SAVE_ENDIAN_LE, "bad endian");
    SAVE_ASSERT(h.slot == 0, "bad slot");
    SAVE_ASSERT(h.headerSize == sizeof(rsbs::RsbsSaveHeader), "bad headerSize");
    // The FIXED record size, deliberately not sizeof(ComboContext): the record
    // is padded so appending a field cannot move it.
    SAVE_ASSERT(h.comboSize == RSBS_COMBO_CONTEXT_RECORD_SIZE, "bad comboSize");
    SAVE_ASSERT(h.ootSize == OOT_SAVE_CONTEXT_SIZE, "bad ootSize");
    SAVE_ASSERT(h.mmSize == MM_SAVE_CONTEXT_SIZE, "bad mmSize");

    // Read the payload and confirm the stored CRC actually verifies.
    std::vector<uint8_t> payload(h.comboSize + h.ootSize + h.mmSize);
    in.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    SAVE_ASSERT(in.gcount() == static_cast<std::streamsize>(payload.size()), "short payload read");
    SAVE_ASSERT(h.crc32 != 0u, "CRC unexpectedly zero");
    SAVE_ASSERT(rsbs::SaveManager::Crc32(payload.data(), payload.size()) == h.crc32, "CRC does not verify");

    mgr.DeleteSave(0);
    printf("[TEST] PASS: header well-formed and CRC verifies\n");
    return TEST_PASS;
}

TestResult Test_SaveHasDelete(void) {
    printf("[TEST] save-has-delete: HasSave/DeleteSave lifecycle (#35)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir);
    SaveTestSeed(0x00C0FFEEu);

    mgr.DeleteSave(0);
    SAVE_ASSERT(!mgr.HasSave(0), "HasSave true before any Save");

    SAVE_ASSERT(mgr.Save(0), "Save(0) failed");
    SAVE_ASSERT(mgr.HasSave(0), "HasSave false after Save");
    SAVE_ASSERT(!std::filesystem::exists(mgr.SlotPath(0) + ".tmp"), "temp file left behind after Save");

    mgr.DeleteSave(0);
    SAVE_ASSERT(!mgr.HasSave(0), "HasSave true after DeleteSave");

    printf("[TEST] PASS: HasSave/DeleteSave behave correctly\n");
    return TEST_PASS;
}

TestResult Test_SaveVersionReject(void) {
    printf("[TEST] save-version-reject: Load refuses an unknown version without clobbering state (#35)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir);
    SaveTestSeed(0x11112222u);
    mgr.DeleteSave(0);
    SAVE_ASSERT(mgr.Save(0), "Save(0) failed");

    // Corrupt only the version field (CRC covers the payload, not the header,
    // so this isolates the version check).
    SAVE_ASSERT(SaveTestPatchU32(mgr.SlotPath(0), offsetof(rsbs::RsbsSaveHeader, version), 0xFFFFu),
                "could not patch version");

    // Install distinct live state and prove Load leaves it untouched.
    SaveTestSeed(0x33334444u);
    gComboCtx.saveSlot = 0x0BADF00D;

    SAVE_ASSERT(!mgr.Load(0), "Load accepted an unknown version");
    SAVE_ASSERT(gComboCtx.saveSlot == 0x0BADF00D, "rejected Load clobbered ComboContext");
    SAVE_ASSERT(SaveTestOoTShadowMatchesPattern(), "rejected Load clobbered OoT shadow");

    mgr.DeleteSave(0);
    printf("[TEST] PASS: unknown version rejected, live state intact\n");
    return TEST_PASS;
}

TestResult Test_SaveSizeMismatch(void) {
    printf("[TEST] save-size-mismatch: Load refuses an oversized tier without clobbering state (#35)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir);
    SaveTestSeed(0xAAAA5555u);
    mgr.DeleteSave(0);
    SAVE_ASSERT(mgr.Save(0), "Save(0) failed");

    // Claim an OoT tier LARGER than this build's blob capacity. (A smaller
    // stored size is legal — see Test_SaveLegacySize — but larger can never
    // fit the shadow buffers and must be refused.)
    SAVE_ASSERT(SaveTestPatchU32(mgr.SlotPath(0), offsetof(rsbs::RsbsSaveHeader, ootSize),
                                 static_cast<uint32_t>(OOT_SAVE_CONTEXT_SIZE) + 4u),
                "could not patch ootSize");

    SaveTestSeed(0x66667777u);
    gComboCtx.saveSlot = 0x0BADF00D;

    SAVE_ASSERT(!mgr.Load(0), "Load accepted a mismatched tier size");
    SAVE_ASSERT(gComboCtx.saveSlot == 0x0BADF00D, "rejected Load clobbered ComboContext");
    SAVE_ASSERT(SaveTestOoTShadowMatchesPattern(), "rejected Load clobbered OoT shadow");

    mgr.DeleteSave(0);
    printf("[TEST] PASS: tier-size mismatch rejected, live state intact\n");
    return TEST_PASS;
}

TestResult Test_SaveLegacySize(void) {
    printf("[TEST] save-legacy-size: Load accepts a shorter (pre-capacity-fix) tier and zero-extends (#35)\n");

    // Tier sizes written by builds from before the blob capacities covered the
    // ports' full runtime SaveContexts (the N64 struct sizes). The header is
    // self-describing, so such files must still load; the missing tail is the
    // ship.* state those builds truncated away and is restored as zeros.
    const uint32_t kLegacyOoTSize = 0x1428;
    const uint32_t kLegacyMMSize = 0x48C8;

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir);
    const uint32_t tag = 0x4C454741u;  // "LEGA"
    SaveTestSeed(tag);
    mgr.DeleteSave(0);

    // Hand-craft the legacy file: same v1 layout, shorter game tiers.
    std::vector<uint8_t> payload;
    payload.reserve(sizeof(ComboContext) + kLegacyOoTSize + kLegacyMMSize);
    const uint8_t* comboBytes = reinterpret_cast<const uint8_t*>(&gComboCtx);
    payload.insert(payload.end(), comboBytes, comboBytes + sizeof(ComboContext));
    for (size_t i = 0; i < kLegacyOoTSize; i++) {
        payload.push_back(SaveTestOoTByte(i));
    }
    for (size_t i = 0; i < kLegacyMMSize; i++) {
        payload.push_back(SaveTestMMByte(i));
    }

    rsbs::RsbsSaveHeader h;
    std::memset(&h, 0, sizeof(h));
    std::memcpy(h.magic, RSBS_SAVE_MAGIC, sizeof(h.magic));
    h.version = RSBS_SAVE_VERSION;
    h.endian = RSBS_SAVE_ENDIAN_LE;
    h.slot = 0;
    h.headerSize = sizeof(rsbs::RsbsSaveHeader);
    h.comboSize = static_cast<uint32_t>(sizeof(ComboContext));
    h.ootSize = kLegacyOoTSize;
    h.mmSize = kLegacyMMSize;
    h.crc32 = rsbs::SaveManager::Crc32(payload.data(), payload.size());

    std::filesystem::create_directories(kSaveTestDir);
    {
        std::ofstream out(mgr.SlotPath(0), std::ios::binary | std::ios::trunc);
        SAVE_ASSERT(static_cast<bool>(out), "could not write legacy slot file");
        out.write(reinterpret_cast<const char*>(&h), sizeof(h));
        out.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
        SAVE_ASSERT(static_cast<bool>(out), "short write of legacy slot file");
    }

    // Wipe live state with a recognizable non-zero fill so both "restored" and
    // "zero-extended" outcomes are distinguishable from leftovers.
    ComboContext_Init();
    gComboCtx.saveSlot = 0x7FFFFFFF;
    std::vector<uint8_t> junkOoT(OOT_SAVE_CONTEXT_SIZE, 0x5A);
    std::vector<uint8_t> junkMM(MM_SAVE_CONTEXT_SIZE, 0x5A);
    Context_UpdateShadowCopy(GAME_OOT, junkOoT.data(), junkOoT.size());
    Context_UpdateShadowCopy(GAME_MM, junkMM.data(), junkMM.size());

    SAVE_ASSERT(mgr.Load(0), "Load refused a valid legacy-size file");
    SAVE_ASSERT(gComboCtx.saveSlot == static_cast<int32_t>(tag), "ComboContext not restored from legacy file");

    const uint8_t* oot = static_cast<const uint8_t*>(Context_GetOoTSaveContext());
    SAVE_ASSERT(oot != nullptr, "OoT shadow missing after legacy load");
    for (size_t i = 0; i < kLegacyOoTSize; i++) {
        SAVE_ASSERT(oot[i] == SaveTestOoTByte(i), "legacy OoT bytes not restored");
    }
    for (size_t i = kLegacyOoTSize; i < OOT_SAVE_CONTEXT_SIZE; i++) {
        SAVE_ASSERT(oot[i] == 0, "OoT tail beyond legacy size not zero-extended");
    }

    const uint8_t* mm = static_cast<const uint8_t*>(Context_GetMMSaveContext());
    SAVE_ASSERT(mm != nullptr, "MM shadow missing after legacy load");
    for (size_t i = 0; i < kLegacyMMSize; i++) {
        SAVE_ASSERT(mm[i] == SaveTestMMByte(i), "legacy MM bytes not restored");
    }
    for (size_t i = kLegacyMMSize; i < MM_SAVE_CONTEXT_SIZE; i++) {
        SAVE_ASSERT(mm[i] == 0, "MM tail beyond legacy size not zero-extended");
    }

    mgr.DeleteSave(0);
    printf("[TEST] PASS: legacy-size tiers load and zero-extend to the current capacities\n");
    return TEST_PASS;
}

TestResult Test_SaveCrcCorrupt(void) {
    printf("[TEST] save-crc-corrupt: Load refuses a corrupt payload without clobbering state (#35)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir);
    SaveTestSeed(0x0F0F0F0Fu);
    mgr.DeleteSave(0);
    SAVE_ASSERT(mgr.Save(0), "Save(0) failed");

    SAVE_ASSERT(SaveTestFlipPayloadByte(mgr.SlotPath(0)), "could not flip payload byte");

    SaveTestSeed(0xF0F0F0F0u);
    gComboCtx.saveSlot = 0x0BADF00D;

    SAVE_ASSERT(!mgr.Load(0), "Load accepted a corrupt payload");
    SAVE_ASSERT(gComboCtx.saveSlot == 0x0BADF00D, "rejected Load clobbered ComboContext");
    SAVE_ASSERT(SaveTestOoTShadowMatchesPattern(), "rejected Load clobbered OoT shadow");

    mgr.DeleteSave(0);
    printf("[TEST] PASS: corrupt payload rejected, live state intact\n");
    return TEST_PASS;
}

// ============================================================================
// Tier-1 (ComboContext) format-headroom locks — Phase 3 Wave 1.
//
// Lane A widens gComboCtx.sharedItems to carry an origin-game tag, which
// changes sizeof(ComboContext). Before this lane, DeserializeHeader demanded
// `h.comboSize == sizeof(ComboContext)` exactly, so that change would have made
// every already-written .redsave stop loading — silently, because Load's only
// failure signal was a `false` nobody surfaced. These three tests are the
// migration path: without them the headroom is an untested claim.
// ============================================================================

namespace {

// Writes a hand-crafted slot file with an arbitrary version and Tier-1 record
// length, taking the Tier-1 bytes from the front of the CURRENT gComboCtx.
// Truncating gComboCtx at `comboSize` is byte-for-byte what an older build
// wrote, as long as fields are only ever appended — which is the growth
// contract documented in context.h.
bool SaveTestWriteCraftedSlot(const std::string& path, uint32_t version, uint32_t comboSize) {
    std::vector<uint8_t> payload;
    const uint8_t* comboBytes = reinterpret_cast<const uint8_t*>(&gComboCtx);
    payload.insert(payload.end(), comboBytes, comboBytes + comboSize);
    for (size_t i = 0; i < OOT_SAVE_CONTEXT_SIZE; i++) {
        payload.push_back(SaveTestOoTByte(i));
    }
    for (size_t i = 0; i < MM_SAVE_CONTEXT_SIZE; i++) {
        payload.push_back(SaveTestMMByte(i));
    }

    rsbs::RsbsSaveHeader h;
    std::memset(&h, 0, sizeof(h));
    std::memcpy(h.magic, RSBS_SAVE_MAGIC, sizeof(h.magic));
    h.version = version;
    h.endian = RSBS_SAVE_ENDIAN_LE;
    h.slot = 0;
    h.headerSize = sizeof(rsbs::RsbsSaveHeader);
    h.comboSize = comboSize;
    h.ootSize = static_cast<uint32_t>(OOT_SAVE_CONTEXT_SIZE);
    h.mmSize = static_cast<uint32_t>(MM_SAVE_CONTEXT_SIZE);
    h.crc32 = rsbs::SaveManager::Crc32(payload.data(), payload.size());

    std::filesystem::create_directories(kSaveTestDir);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(&h), sizeof(h));
    out.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    return static_cast<bool>(out);
}

}  // namespace

TestResult Test_SaveComboLegacyRecord(void) {
    printf("[TEST] save-combo-legacy-record: a pre-headroom .redsave still loads and zero-extends\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir);

    const uint32_t tag = 0x0DDBA11u;
    SaveTestSeed(tag);
    for (size_t i = 0; i < 32; i++) {
        gComboCtx.sharedItems[i] = static_cast<uint16_t>(0x2000u + i);
    }
    mgr.DeleteSave(0);

    // The legacy record length is the PINNED pre-carve prefix, not
    // offsetof(ComboContext, reserved): the ADR 0002 carve moved that offsetof
    // forward past sharedItemsTagged, so deriving "legacy" from it would
    // silently include the carved fields and stop exercising the true shipped
    // pre-carve prefix. context.h static_asserts tie the constant to
    // offsetof(ComboContext, sharedItemsTagged), so it cannot drift.
    const uint32_t kLegacyComboSize = RSBS_COMBO_CONTEXT_PRECARVE_SIZE;
    SAVE_ASSERT(kLegacyComboSize < RSBS_COMBO_CONTEXT_RECORD_SIZE,
                "legacy Tier-1 must be shorter than the record budget");
    SAVE_ASSERT(SaveTestWriteCraftedSlot(mgr.SlotPath(0), RSBS_SAVE_VERSION_MIN, kLegacyComboSize),
                "could not write legacy-record slot file");

    // Scribble everything past the legacy prefix — the carved tagged-item
    // array AND the remaining reserved tail — so "zero-extended" is provably
    // the loader's doing and not a leftover zero.
    ComboContext_Init();
    gComboCtx.saveSlot = 0x7FFFFFFF;
    std::memset(gComboCtx.sharedItemsTagged, 0x5A, sizeof(gComboCtx.sharedItemsTagged));
    std::memset(gComboCtx.reserved, 0x5A, sizeof(gComboCtx.reserved));

    SAVE_ASSERT(mgr.Load(0), "Load refused a pre-headroom .redsave — the migration path is broken");

    SAVE_ASSERT(gComboCtx.saveSlot == static_cast<int32_t>(tag), "legacy saveSlot not restored");
    SAVE_ASSERT(gComboCtx.sharedFlags[5] == tag, "legacy sharedFlags not restored");
    SAVE_ASSERT(gComboCtx.sharedRandoSeed == (tag ^ 0xA5A5A5A5u), "legacy sharedRandoSeed not restored");
    SAVE_ASSERT(gComboCtx.sourceIsRando, "legacy sourceIsRando not restored");
    SAVE_ASSERT(gComboCtx.sourceGame == GAME_OOT, "legacy sourceGame not restored");
    for (size_t i = 0; i < 32; i++) {
        SAVE_ASSERT(gComboCtx.sharedItems[i] == static_cast<uint16_t>(0x2000u + i),
                    "legacy sharedItems not restored");
    }
    // The fields carved from the pre-carve headroom must read as UNSET, which
    // for SharedItem means all-zero members — the growth contract's "zero
    // means unset" made concrete.
    for (size_t i = 0; i < RSBS_SHARED_ITEM_CAP; i++) {
        SAVE_ASSERT(gComboCtx.sharedItemsTagged[i].originGame == GAME_NONE &&
                        gComboCtx.sharedItemsTagged[i].flags == 0 && gComboCtx.sharedItemsTagged[i].id == 0,
                    "tagged items from a pre-carve record must load as unset slots");
    }
    for (size_t i = 0; i < sizeof(gComboCtx.reserved); i++) {
        SAVE_ASSERT(gComboCtx.reserved[i] == 0, "Tier-1 tail beyond the legacy record not zero-extended");
    }
    SAVE_ASSERT(SaveTestOoTShadowMatchesPattern(), "OoT blob not restored from legacy-record file");
    SAVE_ASSERT(SaveTestMMShadowMatchesPattern(), "MM blob not restored from legacy-record file");

    mgr.DeleteSave(0);
    printf("[TEST] PASS: pre-headroom Tier-1 loads, added fields read as zero\n");
    return TEST_PASS;
}

TestResult Test_SaveComboRecordFixed(void) {
    printf("[TEST] save-combo-record-fixed: Tier-1 is written at a fixed padded size and carries the tail\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir);
    SaveTestSeed(0xFEEDFACEu);

    // Stand in for a field Lane A will carve out of `reserved`: if these bytes
    // survive a Save/Load, so will the real field.
    for (size_t i = 0; i < sizeof(gComboCtx.reserved); i++) {
        gComboCtx.reserved[i] = static_cast<uint8_t>(0x30u + i * 7u);
    }

    mgr.DeleteSave(0);
    SAVE_ASSERT(mgr.Save(0), "Save(0) failed");

    // The written Tier-1 length must be the budget, NOT sizeof(ComboContext) —
    // that decoupling is what keeps the on-disk size stable when the struct
    // grows. Total file length proves the padding is actually on disk.
    const uintmax_t expected = sizeof(rsbs::RsbsSaveHeader) + RSBS_COMBO_CONTEXT_RECORD_SIZE +
                               OOT_SAVE_CONTEXT_SIZE + MM_SAVE_CONTEXT_SIZE;
    SAVE_ASSERT(std::filesystem::file_size(mgr.SlotPath(0)) == expected,
                "slot file length does not match a fixed-size padded Tier-1");
    SAVE_ASSERT(sizeof(ComboContext) <= RSBS_COMBO_CONTEXT_RECORD_SIZE,
                "ComboContext no longer fits its record budget");

    ComboContext_Init();
    gComboCtx.saveSlot = 0x7FFFFFFF;

    SAVE_ASSERT(mgr.Load(0), "Load(0) failed");
    for (size_t i = 0; i < sizeof(gComboCtx.reserved); i++) {
        SAVE_ASSERT(gComboCtx.reserved[i] == static_cast<uint8_t>(0x30u + i * 7u),
                    "reserved headroom not round-tripped — a future field would be lost");
    }

    mgr.DeleteSave(0);
    printf("[TEST] PASS: Tier-1 record size is fixed and its headroom round-trips\n");
    return TEST_PASS;
}

TestResult Test_SaveComboOversize(void) {
    printf("[TEST] save-combo-oversize: Load refuses a Tier-1 larger than this build can hold\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir);
    SaveTestSeed(0x13571357u);
    mgr.DeleteSave(0);
    SAVE_ASSERT(mgr.Save(0), "Save(0) failed");

    // A Tier-1 record from a future, wider format. Accepting it would mean
    // silently truncating cross-game state; the only safe answer is to refuse
    // (loudly — Load logs the reason).
    SAVE_ASSERT(SaveTestPatchU32(mgr.SlotPath(0), offsetof(rsbs::RsbsSaveHeader, comboSize),
                                 RSBS_COMBO_CONTEXT_RECORD_SIZE + 4u),
                "could not patch comboSize");

    SaveTestSeed(0x2468ACE0u);
    gComboCtx.saveSlot = 0x0BADF00D;

    SAVE_ASSERT(!mgr.Load(0), "Load accepted an oversized Tier-1 record");
    SAVE_ASSERT(gComboCtx.saveSlot == 0x0BADF00D, "rejected Load clobbered ComboContext");
    SAVE_ASSERT(SaveTestOoTShadowMatchesPattern(), "rejected Load clobbered OoT shadow");

    mgr.DeleteSave(0);
    printf("[TEST] PASS: oversized Tier-1 rejected, live state intact\n");
    return TEST_PASS;
}

TestResult Test_SaveTaggedItems(void) {
    printf("[TEST] save-tagged-items: origin-tagged shared items round-trip byte-exact (ADR 0002)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir);

    SaveTestSeed(0x7A66EDu);

    // A representative spread: first slot, adjacent slot, and the LAST slot,
    // covering both origin games and a set flags bit. Values are arbitrary
    // in-range ids — the test asserts transport, not item semantics.
    gComboCtx.sharedItemsTagged[0].originGame = GAME_OOT;
    gComboCtx.sharedItemsTagged[0].flags = 0;
    gComboCtx.sharedItemsTagged[0].id = 0x00A7;
    gComboCtx.sharedItemsTagged[1].originGame = GAME_MM;
    gComboCtx.sharedItemsTagged[1].flags = RSBS_SHARED_ITEM_REDEEMED;
    gComboCtx.sharedItemsTagged[1].id = 0x0042;
    gComboCtx.sharedItemsTagged[RSBS_SHARED_ITEM_CAP - 1].originGame = GAME_OOT;
    gComboCtx.sharedItemsTagged[RSBS_SHARED_ITEM_CAP - 1].flags = 0;
    gComboCtx.sharedItemsTagged[RSBS_SHARED_ITEM_CAP - 1].id = 0x0123;

    SharedItem expected[RSBS_SHARED_ITEM_CAP];
    std::memcpy(expected, gComboCtx.sharedItemsTagged, sizeof(expected));

    mgr.DeleteSave(0);
    SAVE_ASSERT(mgr.Save(0), "Save(0) failed");

    // Wipe live state, then scribble the tagged array so a restored zero is
    // provably the loader's doing.
    ComboContext_Init();
    std::memset(gComboCtx.sharedItemsTagged, 0x5A, sizeof(gComboCtx.sharedItemsTagged));

    SAVE_ASSERT(mgr.Load(0), "Load(0) failed");

    SAVE_ASSERT(std::memcmp(expected, gComboCtx.sharedItemsTagged, sizeof(expected)) == 0,
                "tagged shared items not byte-identical after a .redsave round-trip");
    // Spot-check through the TYPED view too, so a memcmp-passing-but-misread
    // layout (e.g. an endianness or padding surprise) still fails loudly.
    SAVE_ASSERT(gComboCtx.sharedItemsTagged[0].originGame == GAME_OOT, "slot 0 origin tag lost");
    SAVE_ASSERT(gComboCtx.sharedItemsTagged[0].id == 0x00A7, "slot 0 id lost");
    SAVE_ASSERT(gComboCtx.sharedItemsTagged[1].originGame == GAME_MM, "slot 1 origin tag lost");
    SAVE_ASSERT(gComboCtx.sharedItemsTagged[1].flags == RSBS_SHARED_ITEM_REDEEMED, "slot 1 redeemed bit lost");
    SAVE_ASSERT(gComboCtx.sharedItemsTagged[RSBS_SHARED_ITEM_CAP - 1].id == 0x0123, "last slot id lost");
    // Untouched slots must come back as unset, not as scribble.
    SAVE_ASSERT(gComboCtx.sharedItemsTagged[2].originGame == GAME_NONE && gComboCtx.sharedItemsTagged[2].flags == 0 &&
                    gComboCtx.sharedItemsTagged[2].id == 0,
                "an unpopulated slot must round-trip as unset");

    mgr.DeleteSave(0);
    printf("[TEST] PASS: tagged shared items survive Save/Load; empty slots stay unset\n");
    return TEST_PASS;
}
