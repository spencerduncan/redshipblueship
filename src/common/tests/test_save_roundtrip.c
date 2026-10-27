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

#include "../combo_tracker_view.h" // #1129: the departure captures Tier-1 carries
#include "../context.h"
#include "../crossing_store.h" // Combo_Crossings_SerializedSize: the v3 Tier-4 length
#include "../game.h"
#include "../save.h"
#include "../shared_items.h" // Combo_SharedItemSlot: the logical outbox across both segments
#include "../test_runner.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
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

inline const char* kSaveTestDir() {
    return rsbs::test::ScratchDirCStr("rsbs_test_saves");
}

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
    ComboContext_Init(); // stamps magic "OoT+MM<3" + version, zeroes the rest
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
    uint8_t bytes[4] = { static_cast<uint8_t>(value & 0xFFu), static_cast<uint8_t>((value >> 8) & 0xFFu),
                         static_cast<uint8_t>((value >> 16) & 0xFFu), static_cast<uint8_t>((value >> 24) & 0xFFu) };
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

} // namespace

TestResult Test_SaveRoundtripTiers(void) {
    printf("[TEST] save-roundtrip-tiers: .redsave preserves ComboContext + both SaveContexts (#35)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir());

    const uint32_t tag = 0x1234ABCDu;
    SaveTestSeed(tag);
    // #1129: both games' departure captures ride Tier-1. Patterned bits over
    // counts that end mid-byte, read back through the view.
    std::vector<uint8_t> ootBits(2000 / 8 + 1, 0);
    std::vector<uint8_t> mmBits(1500 / 8 + 1, 0);
    for (uint32_t i = 0; i < 2000; i++) {
        ootBits[i >> 3] |= (uint8_t)((i % 3 == 0 ? 1u : 0u) << (i & 7));
    }
    for (uint32_t i = 0; i < 1500; i++) {
        mmBits[i >> 3] |= (uint8_t)((i % 5 == 1 ? 1u : 0u) << (i & 7));
    }
    Combo_TrackerFreezeAvailability((uint8_t)GAME_OOT, ootBits.data(), 2000);
    Combo_TrackerFreezeAvailability((uint8_t)GAME_MM, mmBits.data(), 1500);
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
    int ootWrong = 0;
    int mmWrong = 0;
    for (uint32_t i = 0; i < 2000; i++) {
        ootWrong += Combo_TrackerFrozenAvailability((uint8_t)GAME_OOT, (uint16_t)i) != (i % 3 == 0 ? 1 : 0) ? 1 : 0;
    }
    for (uint32_t i = 0; i < 1500; i++) {
        mmWrong += Combo_TrackerFrozenAvailability((uint8_t)GAME_MM, (uint16_t)i) != (i % 5 == 1 ? 1 : 0) ? 1 : 0;
    }
    printf("[TEST] save-roundtrip-tiers #1129: departure captures after the round trip: OoT %d of 2000 ids wrong, MM "
           "%d of 1500 wrong\n",
           ootWrong, mmWrong);
    SAVE_ASSERT(ootWrong == 0 && mmWrong == 0, "the tracker availability captures did not survive the round-trip");
    SAVE_ASSERT(Combo_TrackerFrozenAvailability((uint8_t)GAME_OOT, 2000) == -1 &&
                    Combo_TrackerFrozenAvailability((uint8_t)GAME_MM, 1500) == -1,
                "an id past a capture's count reads as captured after the round-trip");

    mgr.DeleteSave(0);
    printf("[TEST] PASS: three-tier round-trip is byte-exact\n");
    return TEST_PASS;
}

TestResult Test_SaveHeader(void) {
    printf("[TEST] save-header: .redsave header fields + CRC are well-formed (#35)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir());
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

    // Read the payload and confirm the stored CRC actually verifies. From
    // format v3 the payload is Tiers 1-3 plus the self-sized Tier-4 crossing
    // block (ADR 0010 O7), so the CRC covers everything after the header.
    std::vector<uint8_t> payload(h.comboSize + h.ootSize + h.mmSize + Combo_Crossings_SerializedSize());
    in.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    SAVE_ASSERT(in.gcount() == static_cast<std::streamsize>(payload.size()), "short payload read");
    SAVE_ASSERT(in.peek() == std::char_traits<char>::eof(), "bytes after the Tier-4 crossing block");
    SAVE_ASSERT(h.crc32 != 0u, "CRC unexpectedly zero");
    SAVE_ASSERT(rsbs::SaveManager::Crc32(payload.data(), payload.size()) == h.crc32, "CRC does not verify");

    mgr.DeleteSave(0);
    printf("[TEST] PASS: header well-formed and CRC verifies\n");
    return TEST_PASS;
}

TestResult Test_SaveHasDelete(void) {
    printf("[TEST] save-has-delete: HasSave/DeleteSave lifecycle (#35)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir());
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
    mgr.SetSaveDirectory(kSaveTestDir());
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
    mgr.SetSaveDirectory(kSaveTestDir());
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
    mgr.SetSaveDirectory(kSaveTestDir());
    const uint32_t tag = 0x4C454741u; // "LEGA"
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
    // Hand-assembled from Tiers 1-3, so it claims the last version WITHOUT the
    // Tier-4 crossing block (ADR 0010 O7) rather than RSBS_SAVE_VERSION.
    h.version = RSBS_SAVE_VERSION_CROSSINGS - 1u;
    h.endian = RSBS_SAVE_ENDIAN_LE;
    h.slot = 0;
    h.headerSize = sizeof(rsbs::RsbsSaveHeader);
    h.comboSize = static_cast<uint32_t>(sizeof(ComboContext));
    h.ootSize = kLegacyOoTSize;
    h.mmSize = kLegacyMMSize;
    h.crc32 = rsbs::SaveManager::Crc32(payload.data(), payload.size());

    std::filesystem::create_directories(kSaveTestDir());
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
    mgr.SetSaveDirectory(kSaveTestDir());
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

    std::filesystem::create_directories(kSaveTestDir());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(&h), sizeof(h));
    out.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    return static_cast<bool>(out);
}

} // namespace

TestResult Test_SaveComboLegacyRecord(void) {
    printf("[TEST] save-combo-legacy-record: a pre-headroom .redsave still loads and zero-extends\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir());

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
    // #1129: a capture in the live context must not survive a load of a record
    // written before the captures existed.
    const uint8_t liveCapture[1] = { 0xFF };
    Combo_TrackerFreezeAvailability((uint8_t)GAME_OOT, liveCapture, 8);
    Combo_TrackerFreezeAvailability((uint8_t)GAME_MM, liveCapture, 8);

    SAVE_ASSERT(mgr.Load(0), "Load refused a pre-headroom .redsave — the migration path is broken");
    SAVE_ASSERT(Combo_TrackerFrozenAvailability((uint8_t)GAME_OOT, 1) == -1 &&
                    Combo_TrackerFrozenAvailability((uint8_t)GAME_MM, 1) == -1,
                "a pre-#1129 record must read as no tracker availability capture");

    SAVE_ASSERT(gComboCtx.saveSlot == static_cast<int32_t>(tag), "legacy saveSlot not restored");
    SAVE_ASSERT(gComboCtx.sharedFlags[5] == tag, "legacy sharedFlags not restored");
    SAVE_ASSERT(gComboCtx.sharedRandoSeed == (tag ^ 0xA5A5A5A5u), "legacy sharedRandoSeed not restored");
    SAVE_ASSERT(gComboCtx.sourceIsRando, "legacy sourceIsRando not restored");
    SAVE_ASSERT(gComboCtx.sourceGame == GAME_OOT, "legacy sourceGame not restored");
    for (size_t i = 0; i < 32; i++) {
        SAVE_ASSERT(gComboCtx.sharedItems[i] == static_cast<uint16_t>(0x2000u + i), "legacy sharedItems not restored");
    }
    // The fields carved from the pre-carve headroom must read as UNSET, which
    // for SharedItem means all-zero members — the growth contract's "zero
    // means unset" made concrete.
    for (size_t i = 0; i < RSBS_SHARED_ITEM_CAP; i++) {
        SAVE_ASSERT(gComboCtx.sharedItemsTagged[i].originGame == GAME_NONE &&
                        gComboCtx.sharedItemsTagged[i].flags == 0 && gComboCtx.sharedItemsTagged[i].id == 0,
                    "tagged items from a pre-carve record must load as unset slots");
    }
    // The ADR 0011 carve reads as ABSENT from a pre-carve record, which is the
    // whole compatibility claim: formatVersion is the occupancy tag, so an
    // all-zero block is "no combo settings were ever frozen" — exempt from
    // comparison and repaired at the first crossing (accepted answer O5) —
    // rather than a formatted record whose direction byte happens to be 0.
    SAVE_ASSERT(gComboCtx.comboSettings.formatVersion == 0,
                "the combo settings record from a pre-carve save must read as ABSENT");
    SAVE_ASSERT(gComboCtx.comboSettingsHash == 0,
                "the combo settings fingerprint from a pre-carve save must read as unset");
    for (size_t i = 0; i < sizeof(gComboCtx.reserved); i++) {
        SAVE_ASSERT(gComboCtx.reserved[i] == 0, "Tier-1 tail beyond the legacy record not zero-extended");
    }
    SAVE_ASSERT(SaveTestOoTShadowMatchesPattern(), "OoT blob not restored from legacy-record file");
    SAVE_ASSERT(SaveTestMMShadowMatchesPattern(), "MM blob not restored from legacy-record file");

    // ------------------------------------------------------------------
    // v2 case: FIXED BYTE OFFSETS, not offsetof-against-offsetof (#490)
    // ------------------------------------------------------------------
    // The prefix case above proves appended fields zero-extend. It says
    // nothing about the fields already shipped BEHIND foreignPlacements, and
    // that is where the live hazard is: RSBS_FOREIGN_PLACEMENT_CAP is carved
    // ahead of grantCursors and sharedItemOverflowCount, so widening the array
    // in place slides both off the offsets every shipped .redsave stored them
    // at. Nothing at runtime notices — the inner COMBO_CONTEXT_MAGIC sits at
    // offset 0 and survives any tail shift, and the CRC passes because the
    // bytes are unchanged; only their interpretation moved.
    //
    // So: place the bytes by LITERAL INTEGER offset into the record image,
    // load it through the real SaveManager::Load, and read the result out of
    // the named struct members. The expected offsets are integers matching
    // bytes physically placed in a crafted file — never offsetof compared
    // against offsetof, which would move in lockstep with the mistake.
    //
    // RED against any in-place cap bump (8 -> 12 is enough): the crafted
    // cursor is then read out of the middle of the widened placement array,
    // this test fails, and the build plus every other test stays green. That
    // is the exact failure mode nothing else catches.
    static const size_t kGrantCursorsOffset = 672u;        // grantCursors[0].sourceKey
    static const size_t kGrantCursorsLastSeqOffset = 676u; // grantCursors[0].lastSeq
    static const size_t kOverflowCountOffset = 736u;       // sharedItemOverflowCount
    static const uint32_t kCraftedSourceKey = 0xC0FFEE01u;
    static const uint32_t kCraftedLastSeq = 0x00000205u;
    static const uint32_t kCraftedOverflow = 0x00BADBEDu;
    // The SECOND shared-resource block (#525 optional tier). It is the newest
    // carve and therefore the one most exposed to the hazard this case exists
    // for: it sits directly behind sharedResources[8], so an in-place widen of
    // THAT array — the one thing RSBS_SHARED_RESOURCE_CAP's comment forbids —
    // slides this block off 824 while every other test stays green. Crafted as
    // a whole 4-byte slot so the kind/flags/value member offsets are pinned too.
    static const size_t kSharedResourcesExtOffset = 824u; // sharedResourcesExt[0]
    static const uint8_t kCraftedExtKind = RSBS_SHARED_RES_QUIVER_TIER;
    static const uint8_t kCraftedExtFlags = RSBS_SHARED_RES_F_MONOTONIC;
    static const uint16_t kCraftedExtValue = 0x0003u;
    // The ADR 0011 carve: the whole-pair fingerprint at 880 (ADR 0009 claim 2,
    // spent exactly as reserved) and the 12-byte frozen rule record at 884 (ADR
    // 0011 claim 10). Crafted BYTE BY BYTE at the literal offsets, so the record
    // pins every MEMBER offset too — those same twelve bytes are what the
    // canonical digest encoder walks in declaration order, so a member reorder
    // here would silently re-identify every already-created world rather than
    // merely moving a field.
    //
    // formatVersion is crafted as 0 ON PURPOSE, and the other eleven bytes are
    // distinctive: the tag says ABSENT, so the load's combo identity check
    // (ADR 0011 decision 4) exempts the record and the file loads — while the
    // loader still copies all twelve bytes, which is what lets every member be
    // read back and its offset asserted. A formatted record would be COMPARED,
    // and an arbitrary crafted one would (correctly) be refused before anything
    // could be read out of it. The identity refusal has its own lock; this one
    // is about where the bytes land.
    static const size_t kComboSettingsHashOffset = 880u;
    static const size_t kComboSettingsOffset = 884u;
    static const uint32_t kCraftedComboHash = 0xC0FFEE11u;
    static const uint8_t kCraftedComboRecord[12] = {
        0x00,       // formatVersion — ABSENT tag (see above)
        0x03,       // direction
        0x06,       // poolSizeOoT
        0x07,       // poolSizeMM
        0x21, 0x43, // itemClassOoT LE == 0x4321
        0x65, 0x87, // itemClassMM  LE == 0x8765
        0x02,       // goal
        0x09,       // logicRung
        0x11,       // comboFlags (byte 10, formerly spare0; #668)
        0x22,       // spare1
    };

    SaveTestSeed(tag); // re-inits gComboCtx and restamps the magic/version
    {
        // Write through a byte pointer at the literal offsets. Deliberately
        // NOT gComboCtx.grantCursors[0].sourceKey = ...: assigning through the
        // member would place the bytes wherever the current layout happens to
        // put them, which is precisely the thing under test.
        uint8_t* image = reinterpret_cast<uint8_t*>(&gComboCtx);
        std::memcpy(image + kGrantCursorsOffset, &kCraftedSourceKey, sizeof(kCraftedSourceKey));
        std::memcpy(image + kGrantCursorsLastSeqOffset, &kCraftedLastSeq, sizeof(kCraftedLastSeq));
        std::memcpy(image + kOverflowCountOffset, &kCraftedOverflow, sizeof(kCraftedOverflow));
        std::memcpy(image + kSharedResourcesExtOffset + 0, &kCraftedExtKind, sizeof(kCraftedExtKind));
        std::memcpy(image + kSharedResourcesExtOffset + 1, &kCraftedExtFlags, sizeof(kCraftedExtFlags));
        std::memcpy(image + kSharedResourcesExtOffset + 2, &kCraftedExtValue, sizeof(kCraftedExtValue));
        std::memcpy(image + kComboSettingsHashOffset, &kCraftedComboHash, sizeof(kCraftedComboHash));
        std::memcpy(image + kComboSettingsOffset, kCraftedComboRecord, sizeof(kCraftedComboRecord));
    }

    mgr.DeleteSave(0);
    // v2: the crafted file carries Tiers 1-3 only (no v3 Tier-4 crossing block).
    SAVE_ASSERT(
        SaveTestWriteCraftedSlot(mgr.SlotPath(0), RSBS_SAVE_VERSION_CROSSINGS - 1u, (uint32_t)sizeof(ComboContext)),
        "could not write v2 fixed-offset slot file");

    // Scribble the whole carve region so a pass cannot come from leftovers.
    ComboContext_Init();
    std::memset(gComboCtx.grantCursors, 0x5A, sizeof(gComboCtx.grantCursors));
    gComboCtx.sharedItemOverflowCount = 0x5A5A5A5Au;
    std::memset(gComboCtx.foreignPlacements, 0x5A, sizeof(gComboCtx.foreignPlacements));
    std::memset(gComboCtx.sharedResources, 0x5A, sizeof(gComboCtx.sharedResources));
    std::memset(gComboCtx.sharedResourcesExt, 0x5A, sizeof(gComboCtx.sharedResourcesExt));
    std::memset(&gComboCtx.comboSettings, 0x5A, sizeof(gComboCtx.comboSettings));
    gComboCtx.comboSettingsHash = 0x5A5A5A5Au;

    SAVE_ASSERT(mgr.Load(0), "Load refused a full-length v2 Tier-1 record");

    SAVE_ASSERT(gComboCtx.grantCursors[0].sourceKey == kCraftedSourceKey,
                "byte 672 did not land in grantCursors[0].sourceKey — a field ahead of the ADR 0005 "
                "carve grew in place and orphaned every shipped save's cursors");
    SAVE_ASSERT(gComboCtx.grantCursors[0].lastSeq == kCraftedLastSeq,
                "byte 676 did not land in grantCursors[0].lastSeq — see above");
    SAVE_ASSERT(gComboCtx.sharedItemOverflowCount == kCraftedOverflow,
                "byte 736 did not land in sharedItemOverflowCount — a field ahead of it grew in place");

    // Companion runtime assertions on the layout itself. These are the cheap
    // half; the crafted-bytes assertions above are the half that cannot be
    // satisfied by a self-consistent wrong layout.
    SAVE_ASSERT(offsetof(ComboContext, grantCursors) == kGrantCursorsOffset,
                "grantCursors moved off .redsave byte offset 672");
    SAVE_ASSERT(offsetof(ComboContext, sharedItemOverflowCount) == kOverflowCountOffset,
                "sharedItemOverflowCount moved off .redsave byte offset 736");

    SAVE_ASSERT(gComboCtx.sharedResourcesExt[0].kind == kCraftedExtKind &&
                    gComboCtx.sharedResourcesExt[0].flags == kCraftedExtFlags &&
                    gComboCtx.sharedResourcesExt[0].value == kCraftedExtValue,
                "byte 824 did not land in sharedResourcesExt[0] — the first shared-resource block grew "
                "in place (which RSBS_SHARED_RESOURCE_CAP forbids) and orphaned every shipped save's "
                "shared ammo");
    SAVE_ASSERT(offsetof(ComboContext, sharedResourcesExt) == kSharedResourcesExtOffset,
                "sharedResourcesExt moved off .redsave byte offset 824");

    SAVE_ASSERT(gComboCtx.comboSettingsHash == kCraftedComboHash,
                "byte 880 did not land in comboSettingsHash — a field before it grew in place and orphaned every "
                "shipped save's combo identity (ADR 0009 claim 2)");
    SAVE_ASSERT(gComboCtx.comboSettings.formatVersion == kCraftedComboRecord[0] &&
                    gComboCtx.comboSettings.direction == kCraftedComboRecord[1] &&
                    gComboCtx.comboSettings.poolSizeOoT == kCraftedComboRecord[2] &&
                    gComboCtx.comboSettings.poolSizeMM == kCraftedComboRecord[3] &&
                    gComboCtx.comboSettings.itemClassOoT == 0x4321u && gComboCtx.comboSettings.itemClassMM == 0x8765u &&
                    gComboCtx.comboSettings.goal == kCraftedComboRecord[8] &&
                    gComboCtx.comboSettings.logicRung == kCraftedComboRecord[9] &&
                    gComboCtx.comboSettings.comboFlags == kCraftedComboRecord[10] &&
                    gComboCtx.comboSettings.spare1 == kCraftedComboRecord[11],
                "bytes 884..895 did not land member-for-member in comboSettings — the combo rule record's layout "
                "IS .redsave format, and it is the same byte order the canonical digest encoder walks");
    SAVE_ASSERT(offsetof(ComboContext, comboSettingsHash) == kComboSettingsHashOffset,
                "comboSettingsHash moved off .redsave byte offset 880");
    SAVE_ASSERT(offsetof(ComboContext, comboSettings) == kComboSettingsOffset,
                "comboSettings moved off .redsave byte offset 884");

    mgr.DeleteSave(0);
    printf("[TEST] PASS: pre-headroom Tier-1 loads, added fields read as zero, "
           "and bytes 672/676/736/824/880/884 still land in their carves\n");
    return TEST_PASS;
}

TestResult Test_SaveComboRecordFixed(void) {
    printf("[TEST] save-combo-record-fixed: Tier-1 is written at a fixed padded size and carries the tail\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir());
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
    // Plus the v3 Tier-4 crossing block (ADR 0010 O7), which is variable
    // length: exactly what the resident store serializes to.
    const uintmax_t expected = sizeof(rsbs::RsbsSaveHeader) + RSBS_COMBO_CONTEXT_RECORD_SIZE + OOT_SAVE_CONTEXT_SIZE +
                               MM_SAVE_CONTEXT_SIZE + Combo_Crossings_SerializedSize();
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
    mgr.SetSaveDirectory(kSaveTestDir());
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
    mgr.SetSaveDirectory(kSaveTestDir());

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

// ============================================================================
// THE SHARED-ITEM OVERFLOW (operator ruling 2026-10-27, "do the variable length
// block"; Refs #1222). The cross-game outbox is ONE logical array of
// RSBS_SHARED_ITEM_LOGICAL_CAP slots: Tier-1's fixed sharedItemsTagged[64]
// first, then an overflow segment the .redsave carries as the Tier-5 block
// (format version 5). Before it, the 65th un-redeemed crossing was refused and
// the fill bounded each side at 64 crossings to keep that refusal unreachable.
// ============================================================================

namespace {

uint8_t SaveTestOverflowOrigin(int i) {
    return static_cast<uint8_t>((i % 3 == 0) ? GAME_MM : GAME_OOT);
}
uint16_t SaveTestOverflowId(int i) {
    return static_cast<uint16_t>(0x0200u + static_cast<unsigned>(i) * 7u);
}

bool SaveTestSlotIs(int i, uint8_t origin, uint8_t flags, uint16_t id) {
    const SharedItem* s = Combo_SharedItemSlot(i);
    return s != nullptr && s->originGame == origin && s->flags == flags && s->id == id;
}

bool SaveTestSlotEmpty(int i) {
    const SharedItem* s = Combo_SharedItemSlot(i);
    return s != nullptr && s->originGame == GAME_NONE && s->flags == 0 && s->id == 0;
}

// Record `n` un-redeemed crossings with the pattern above; returns how many the
// outbox accepted.
int SaveTestRecordCrossings(int n) {
    int recorded = 0;
    for (int i = 0; i < n; i++) {
        if (Combo_RecordSharedItemCrossing(static_cast<GameId>(SaveTestOverflowOrigin(i)), SaveTestOverflowId(i)) >=
            0) {
            recorded++;
        }
    }
    return recorded;
}

// The first `n` logical slots hold exactly the pattern, in order.
int SaveTestPatternMismatches(int n) {
    int wrong = 0;
    for (int i = 0; i < n; i++) {
        wrong += SaveTestSlotIs(i, SaveTestOverflowOrigin(i), RSBS_SHARED_ITEM_CROSSING, SaveTestOverflowId(i)) ? 0 : 1;
    }
    return wrong;
}

std::vector<uint8_t> SaveTestReadAll(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool SaveTestWriteAll(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

// Re-stamp the header CRC over everything after the header (every writer's payload).
void SaveTestRestampCrc(std::vector<uint8_t>& file) {
    rsbs::RsbsSaveHeader h;
    std::memcpy(&h, file.data(), sizeof(h));
    h.crc32 = rsbs::SaveManager::Crc32(file.data() + sizeof(h), file.size() - sizeof(h));
    std::memcpy(file.data(), &h, sizeof(h));
}

// Where Tier-4 starts in a file this build wrote.
size_t SaveTestTier4Offset() {
    return sizeof(rsbs::RsbsSaveHeader) + RSBS_COMBO_CONTEXT_RECORD_SIZE + OOT_SAVE_CONTEXT_SIZE + MM_SAVE_CONTEXT_SIZE;
}

uint32_t SaveTestGetU32(const std::vector<uint8_t>& b, size_t at) {
    return static_cast<uint32_t>(b[at]) | (static_cast<uint32_t>(b[at + 1]) << 8) |
           (static_cast<uint32_t>(b[at + 2]) << 16) | (static_cast<uint32_t>(b[at + 3]) << 24);
}

// A v4 file as a v4 build wrote it: the fixed Tier-1 record (the live gComboCtx,
// zero padded), both game tiers, and the Tier-4 crossing block, and nothing after.
bool SaveTestWriteV4Slot(const std::string& path) {
    std::vector<uint8_t> payload;
    const uint8_t* comboBytes = reinterpret_cast<const uint8_t*>(&gComboCtx);
    payload.insert(payload.end(), comboBytes, comboBytes + sizeof(ComboContext));
    payload.insert(payload.end(), RSBS_COMBO_CONTEXT_RECORD_SIZE - sizeof(ComboContext), uint8_t{ 0 });
    for (size_t i = 0; i < OOT_SAVE_CONTEXT_SIZE; i++) {
        payload.push_back(SaveTestOoTByte(i));
    }
    for (size_t i = 0; i < MM_SAVE_CONTEXT_SIZE; i++) {
        payload.push_back(SaveTestMMByte(i));
    }
    std::vector<uint8_t> crossings(Combo_Crossings_SerializedSize());
    if (Combo_Crossings_Serialize(crossings.data(), crossings.size()) != crossings.size()) {
        return false;
    }
    payload.insert(payload.end(), crossings.begin(), crossings.end());

    rsbs::RsbsSaveHeader h;
    std::memset(&h, 0, sizeof(h));
    std::memcpy(h.magic, RSBS_SAVE_MAGIC, sizeof(h.magic));
    h.version = RSBS_SAVE_VERSION_SHARED_OVERFLOW - 1u; // 4: the last version without Tier-5
    h.endian = RSBS_SAVE_ENDIAN_LE;
    h.slot = 0;
    h.headerSize = sizeof(rsbs::RsbsSaveHeader);
    h.comboSize = RSBS_COMBO_CONTEXT_RECORD_SIZE;
    h.ootSize = static_cast<uint32_t>(OOT_SAVE_CONTEXT_SIZE);
    h.mmSize = static_cast<uint32_t>(MM_SAVE_CONTEXT_SIZE);
    h.crc32 = rsbs::SaveManager::Crc32(payload.data(), payload.size());

    std::filesystem::create_directories(kSaveTestDir());
    std::vector<uint8_t> file(sizeof(h));
    std::memcpy(file.data(), &h, sizeof(h));
    file.insert(file.end(), payload.begin(), payload.end());
    return SaveTestWriteAll(path, file);
}

struct SaveTestAwardLog {
    std::vector<SharedItem> items;
};

void SaveTestAward(const SharedItem* item, void* ctx) {
    static_cast<SaveTestAwardLog*>(ctx)->items.push_back(*item);
}

} // namespace

TestResult Test_SaveSharedItemOverflowRoundtrip(void) {
    printf("[TEST] save-shared-item-overflow-roundtrip: un-redeemed crossings past the fixed 64 slots record, save "
           "and load in order (Tier-5)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir());
    SaveTestSeed(0x0F10F10Fu);
    Combo_ClearSharedItemOutbox();
    mgr.DeleteSave(0);

    // ---- 65: one past the fixed segment ------------------------------------
    const int n = static_cast<int>(RSBS_SHARED_ITEM_CAP) + 1;
    const int recorded = SaveTestRecordCrossings(n);
    printf("[TEST]   %d of %d un-redeemed crossings recorded, refusal count %u\n", recorded, n,
           (unsigned)Combo_GetSharedItemOverflowCount());
    SAVE_ASSERT(recorded == n && Combo_GetSharedItemOverflowCount() == 0,
                "the 65th un-redeemed crossing was refused: the outbox stops at the fixed 64 slots");
    SAVE_ASSERT(mgr.Save(0), "Save(0) failed");

    ComboContext_Init(); // clears the fixed slots and the overflow alike
    SAVE_ASSERT(SaveTestSlotEmpty(0) && SaveTestSlotEmpty(n - 1), "ComboContext_Init left a shared-item slot set");
    SAVE_ASSERT(mgr.Load(0), "Load(0) failed");
    const int wrong = SaveTestPatternMismatches(n);
    printf("[TEST]   after the round trip: %d of %d slots wrong; slot %d empty=%d; refusal count %u\n", wrong, n, n,
           SaveTestSlotEmpty(n) ? 1 : 0, (unsigned)Combo_GetSharedItemOverflowCount());
    SAVE_ASSERT(wrong == 0, "the 65 crossings did not come back in pickup order");
    SAVE_ASSERT(SaveTestSlotEmpty(n), "a slot past the last crossing came back set");
    SAVE_ASSERT(Combo_GetSharedItemOverflowCount() == 0, "the refusal count moved");
    SAVE_ASSERT(Combo_CountSharedItems(GAME_OOT, false) + Combo_CountSharedItems(GAME_MM, false) == n,
                "the counts do not span both segments");

    // ---- the whole logical array: the largest Tier-5 block ------------------
    SaveTestSeed(0x0F10F110u);
    const int all = static_cast<int>(RSBS_SHARED_ITEM_LOGICAL_CAP);
    SAVE_ASSERT(SaveTestRecordCrossings(all) == all, "the logical array did not take its full capacity");
    SAVE_ASSERT(!Combo_SharedItemCrossingHasRoom(), "a full logical array still reports room");
    mgr.DeleteSave(0);
    SAVE_ASSERT(mgr.Save(0), "Save(0) of a full outbox failed");
    ComboContext_Init();
    SAVE_ASSERT(mgr.Load(0), "Load(0) of a full outbox failed");
    const int wrongAll = SaveTestPatternMismatches(all);
    printf("[TEST]   full outbox (%d slots) after the round trip: %d wrong\n", all, wrongAll);
    SAVE_ASSERT(wrongAll == 0, "a full outbox did not round-trip in order");
    SAVE_ASSERT(Combo_SharedItemSlot(all) == nullptr, "a slot past the logical capacity exists");

    mgr.DeleteSave(0);
    ComboContext_Init();
    printf("[TEST] PASS: crossings past the fixed segment record and round-trip in pickup order\n");
    return TEST_PASS;
}

TestResult Test_SaveSharedItemOverflowLegacy(void) {
    printf("[TEST] save-shared-item-overflow-legacy: a v4 .redsave loads its 64 slots with an empty overflow and "
           "re-saves as v5; a malformed Tier-5 is refused\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir());
    SaveTestSeed(0x0F10F111u);
    Combo_ClearSharedItemOutbox();
    for (int i = 0; i < static_cast<int>(RSBS_SHARED_ITEM_CAP); i++) {
        gComboCtx.sharedItemsTagged[i].originGame = SaveTestOverflowOrigin(i);
        gComboCtx.sharedItemsTagged[i].flags = RSBS_SHARED_ITEM_CROSSING;
        gComboCtx.sharedItemsTagged[i].id = SaveTestOverflowId(i);
    }
    Combo_Crossings_Clear();
    mgr.DeleteSave(0);
    SAVE_ASSERT(SaveTestWriteV4Slot(mgr.SlotPath(0)), "could not write the v4 slot file");

    // Live state that a load must replace: an outbox past the fixed segment.
    ComboContext_Init();
    for (int i = 0; i < static_cast<int>(RSBS_SHARED_ITEM_CAP) + 6; i++) {
        Combo_RecordSharedItemCrossing(GAME_OOT, 0x7777);
    }

    SAVE_ASSERT(mgr.LoadSlot(0) == RSBS_LOAD_OK, "a v4 .redsave was refused");
    const int wrong = SaveTestPatternMismatches(static_cast<int>(RSBS_SHARED_ITEM_CAP));
    const bool overflowEmpty = SaveTestSlotEmpty(static_cast<int>(RSBS_SHARED_ITEM_CAP));
    printf("[TEST]   v4 load: %d of 64 fixed slots wrong; first overflow slot empty=%d\n", wrong,
           overflowEmpty ? 1 : 0);
    SAVE_ASSERT(wrong == 0, "a v4 file's 64 fixed slots did not load intact");
    SAVE_ASSERT(overflowEmpty, "a v4 file must load with an EMPTY overflow, not the live session's");
    SAVE_ASSERT(Combo_CountSharedItems(GAME_OOT, true) + Combo_CountSharedItems(GAME_MM, true) ==
                    static_cast<int>(RSBS_SHARED_ITEM_CAP),
                "a v4 load holds other than its 64 entries");

    // It re-saves as v5, with an empty Tier-5 block after Tier-4.
    SAVE_ASSERT(mgr.Save(0), "re-save of the loaded v4 file failed");
    std::vector<uint8_t> file = SaveTestReadAll(mgr.SlotPath(0));
    rsbs::RsbsSaveHeader h;
    SAVE_ASSERT(file.size() >= sizeof(h), "short re-saved file");
    std::memcpy(&h, file.data(), sizeof(h));
    const size_t t5 = SaveTestTier4Offset() + Combo_Crossings_SerializedSize();
    printf("[TEST]   re-saved: version %u, %zu bytes (Tier-5 at %zu)\n", (unsigned)h.version, file.size(), t5);
    SAVE_ASSERT(h.version == RSBS_SAVE_VERSION_SHARED_OVERFLOW, "the re-save is not format version 5");
    SAVE_ASSERT(file.size() == t5 + RSBS_SHARED_OVERFLOW_BLOCK_HEADER_SIZE,
                "the re-save does not end in an empty Tier-5 block");
    SAVE_ASSERT(std::memcmp(file.data() + t5, RSBS_SHARED_OVERFLOW_BLOCK_MAGIC, 4) == 0 &&
                    SaveTestGetU32(file, t5 + 8) == 0u,
                "Tier-5 is not an empty RSSO block");

    // ---- malformed v5 files are refused, never truncated -------------------
    struct Bad {
        const char* what;
        RsbsRefuseReason reason;
    };
    const Bad cases[4] = {
        { "a v5 file without its Tier-5 block", RSBS_REFUSE_TRUNCATED },
        { "a Tier-5 count past the overflow capacity", RSBS_REFUSE_TIER_SIZE },
        { "a Tier-5 block with a bad magic", RSBS_REFUSE_TIER_SIZE },
        { "a Tier-5 count with no rows behind it", RSBS_REFUSE_TRUNCATED },
    };
    for (int c = 0; c < 4; c++) {
        std::vector<uint8_t> bad = file;
        if (c == 0) {
            bad.resize(t5);
        } else if (c == 1) {
            const uint32_t over = RSBS_SHARED_ITEM_OVERFLOW_CAP + 1u;
            std::memcpy(bad.data() + t5 + 8, &over, sizeof(over));
        } else if (c == 2) {
            bad[t5] = 'X';
        } else {
            const uint32_t one = 1u;
            std::memcpy(bad.data() + t5 + 8, &one, sizeof(one));
        }
        SaveTestRestampCrc(bad);
        mgr.DeleteSave(0);
        SAVE_ASSERT(SaveTestWriteAll(mgr.SlotPath(0), bad), "could not write a malformed slot file");
        const RsbsLoadOutcome outcome = mgr.LoadSlot(0);
        const RsbsRefuseReason reason = mgr.GetSlotRefuseReason(0);
        printf("[TEST]   %s: outcome %d, reason %s\n", cases[c].what, (int)outcome,
               rsbs::SaveManager::RefuseReasonLabel(reason));
        SAVE_ASSERT(outcome == RSBS_LOAD_REFUSED && reason == cases[c].reason, cases[c].what);
    }

    mgr.DeleteSave(0);
    Combo_Crossings_Clear();
    ComboContext_Init();
    printf("[TEST] PASS: v4 loads with an empty overflow and re-saves as v5; a malformed Tier-5 is refused\n");
    return TEST_PASS;
}

TestResult Test_SharedItemOverflowOrder(void) {
    printf("[TEST] shared-item-overflow-order: one logical array across both segments: pickup order is redemption "
           "order, and compaction pulls the overflow head into the fixed segment\n");

    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    const int fixed = static_cast<int>(RSBS_SHARED_ITEM_CAP);
    int recorded = 0;
    for (int i = 0; i < fixed; i++) {
        recorded += Combo_RecordSharedItemCrossing(GAME_OOT, static_cast<uint16_t>(100 + i)) >= 0 ? 1 : 0;
    }
    for (int j = 0; j < 6; j++) {
        recorded += Combo_RecordSharedItemCrossing(GAME_MM, static_cast<uint16_t>(500 + j)) >= 0 ? 1 : 0;
    }
    printf("[TEST]   %d of %d crossings recorded\n", recorded, fixed + 6);
    SAVE_ASSERT(recorded == fixed + 6, "crossings past the fixed 64 slots were refused");
    for (int j = 0; j < 6; j++) {
        SAVE_ASSERT(SaveTestSlotIs(fixed + j, GAME_MM, RSBS_SHARED_ITEM_CROSSING, static_cast<uint16_t>(500 + j)),
                    "the overflow segment does not continue the fixed one in pickup order");
    }

    // The in-process producer's content de-dup spans the overflow too.
    const int first = Combo_RecordSharedItem(GAME_MM, 777);
    const int again = Combo_RecordSharedItem(GAME_MM, 777);
    SAVE_ASSERT(first == fixed + 6 && again == first, "the in-process de-dup did not find its own overflow entry");
    Combo_SharedItemSlot(first)->flags |= RSBS_SHARED_ITEM_REDEEMED; // retired: the walks below see crossings only

    // Redeem the fixed segment's 64 OoT crossings, in order.
    SaveTestAwardLog oot;
    SAVE_ASSERT(Combo_RedeemSharedItemsForGame(GAME_OOT, SaveTestAward, &oot) == fixed, "OoT redemption count");
    for (int i = 0; i < fixed; i++) {
        SAVE_ASSERT(oot.items[i].id == static_cast<uint16_t>(100 + i), "OoT redemption out of pickup order");
    }

    // One more crossing reclaims the OLDEST redeemed entry (slot 0). Everything
    // after it moves down one, so the overflow head (MM 500) becomes the fixed
    // segment's last slot and the fixed segment stays the dense prefix.
    SAVE_ASSERT(Combo_RecordSharedItemCrossing(GAME_MM, 506) >= 0, "the reclaiming record was refused");
    const uint8_t redeemedCrossing = RSBS_SHARED_ITEM_CROSSING | RSBS_SHARED_ITEM_REDEEMED;
    printf("[TEST]   after the reclaim: slot 0 id %u, slot 63 origin %u id %u, slot 70 id %u\n",
           Combo_SharedItemSlot(0)->id, Combo_SharedItemSlot(fixed - 1)->originGame,
           Combo_SharedItemSlot(fixed - 1)->id, Combo_SharedItemSlot(fixed + 6)->id);
    SAVE_ASSERT(SaveTestSlotIs(0, GAME_OOT, redeemedCrossing, 101) &&
                    SaveTestSlotIs(fixed - 2, GAME_OOT, redeemedCrossing, static_cast<uint16_t>(100 + fixed - 1)),
                "the redeemed OoT entries did not shift down one");
    SAVE_ASSERT(SaveTestSlotIs(fixed - 1, GAME_MM, RSBS_SHARED_ITEM_CROSSING, 500),
                "the overflow head did not move into the fixed segment's freed tail");
    SAVE_ASSERT(SaveTestSlotIs(fixed + 4, GAME_MM, RSBS_SHARED_ITEM_CROSSING, 505) &&
                    SaveTestSlotIs(fixed + 5, GAME_MM, RSBS_SHARED_ITEM_REDEEMED, 777) &&
                    SaveTestSlotIs(fixed + 6, GAME_MM, RSBS_SHARED_ITEM_CROSSING, 506) && SaveTestSlotEmpty(fixed + 7),
                "the overflow did not compact by one with the new crossing last");

    SaveTestAwardLog mm;
    SAVE_ASSERT(Combo_RedeemSharedItemsForGame(GAME_MM, SaveTestAward, &mm) == 7, "MM redemption count");
    for (int j = 0; j < 7; j++) {
        SAVE_ASSERT(mm.items[j].id == static_cast<uint16_t>(500 + j), "MM redemption out of pickup order");
    }
    SAVE_ASSERT(Combo_GetSharedItemOverflowCount() == 0, "the refusal count moved");

    ComboContext_Init();
    printf("[TEST] PASS: both segments are one array in pickup order; compaction crosses the boundary\n");
    return TEST_PASS;
}

TestResult Test_SharedItemOverflowHasRoom(void) {
    printf("[TEST] shared-item-overflow-has-room: a give has room at 64 un-redeemed crossings and loses it only at "
           "the logical capacity\n");

    ComboContext_Init();
    Combo_ClearSharedItemOutbox();
    const int fixed = static_cast<int>(RSBS_SHARED_ITEM_CAP);
    const int all = static_cast<int>(RSBS_SHARED_ITEM_LOGICAL_CAP);
    int recorded = 0;
    for (int i = 0; i < fixed; i++) {
        recorded += Combo_RecordSharedItemCrossing(GAME_OOT, static_cast<uint16_t>(100 + i)) >= 0 ? 1 : 0;
    }
    const bool roomAt64 = Combo_SharedItemCrossingHasRoom();
    printf("[TEST]   %d un-redeemed crossings: room=%d\n", recorded, roomAt64 ? 1 : 0);
    SAVE_ASSERT(recorded == fixed && roomAt64, "the outbox reports no room at 64 un-redeemed crossings");

    for (int i = fixed; i < all; i++) {
        recorded += Combo_RecordSharedItemCrossing(GAME_OOT, static_cast<uint16_t>(100 + i)) >= 0 ? 1 : 0;
    }
    const bool roomAtCap = Combo_SharedItemCrossingHasRoom();
    printf("[TEST]   %d un-redeemed crossings (capacity %d): room=%d\n", recorded, all, roomAtCap ? 1 : 0);
    SAVE_ASSERT(recorded == all, "the outbox refused before its logical capacity");
    SAVE_ASSERT(!roomAtCap, "a full logical array still reports room");
    SAVE_ASSERT(Combo_GetSharedItemOverflowCount() == 0, "a record below the capacity was counted as refused");
    SAVE_ASSERT(Combo_RecordSharedItemCrossing(GAME_OOT, 9) == -1 && Combo_GetSharedItemOverflowCount() == 1,
                "the record past the capacity was not refused and counted once");

    // An entry redeemed in the overflow segment is reclaimable.
    Combo_SharedItemSlot(100)->flags |= RSBS_SHARED_ITEM_REDEEMED;
    SAVE_ASSERT(Combo_SharedItemCrossingHasRoom(), "a redeemed overflow entry does not give room");
    SAVE_ASSERT(Combo_RecordSharedItemCrossing(GAME_MM, 9) >= 0, "the record into the reclaimed slot was refused");
    SAVE_ASSERT(SaveTestSlotIs(100, GAME_OOT, RSBS_SHARED_ITEM_CROSSING, 201) &&
                    SaveTestSlotIs(all - 1, GAME_MM, RSBS_SHARED_ITEM_CROSSING, 9),
                "reclaiming an overflow entry did not compact the tail and append the new crossing last");
    SAVE_ASSERT(!Combo_SharedItemCrossingHasRoom() && Combo_GetSharedItemOverflowCount() == 1,
                "after the reclaim the array is full again with no further refusal");

    ComboContext_Init();
    printf("[TEST] PASS: room until the logical capacity, refusal and reclaim at it\n");
    return TEST_PASS;
}
