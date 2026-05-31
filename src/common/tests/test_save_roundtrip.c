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
    SAVE_ASSERT(h.comboSize == sizeof(ComboContext), "bad comboSize");
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
    printf("[TEST] save-size-mismatch: Load refuses a mismatched tier size without clobbering state (#35)\n");

    rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    mgr.SetSaveDirectory(kSaveTestDir);
    SaveTestSeed(0xAAAA5555u);
    mgr.DeleteSave(0);
    SAVE_ASSERT(mgr.Save(0), "Save(0) failed");

    // Claim a different OoT tier size than this build expects.
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
