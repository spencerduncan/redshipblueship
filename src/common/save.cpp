/**
 * @file save.cpp
 * @brief Unified cross-game save file (.redsave) implementation (Phase 2 T6, #35).
 *
 * See save.h for the format. This file depends only on src/common (context +
 * game headers) and the standard library — never on either game's z64save.h.
 */

#include "save.h"

#include "context.h"
#include "game.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace rsbs {

namespace {

// Tier sizes as this build writes them. All three are CAPACITIES, not exact
// struct sizes: the game-tier constants come from game.h (the same values the
// context-layer shadow buffers are allocated at, so reading the shadows here is
// always in-bounds), and the Tier-1 constant is the fixed record size from
// context.h, which is >= sizeof(ComboContext) by static_assert. On Load the
// header's stored sizes drive the reads instead: older files with shorter tiers
// are accepted and zero-extended (see DeserializeHeader).
constexpr uint32_t kComboSize = RSBS_COMBO_CONTEXT_RECORD_SIZE;
constexpr uint32_t kOoTSize = static_cast<uint32_t>(OOT_SAVE_CONTEXT_SIZE);
constexpr uint32_t kMMSize = static_cast<uint32_t>(MM_SAVE_CONTEXT_SIZE);

static_assert(sizeof(ComboContext) <= kComboSize,
              "ComboContext must fit the fixed Tier-1 record");

bool SlotInRange(int slot) {
    return slot >= 0 && slot < RSBS_SAVE_MAX_SLOTS;
}

// A rejected Load used to return false and tell the user NOTHING — the save
// simply did not come back. Every refusal now names itself on stderr so a
// format break is diagnosable from a log instead of a bug report that reads
// "my save vanished".
void SaveLogReject(bool verbose, const char* reason, unsigned long long got,
                   unsigned long long expected) {
    if (!verbose) {
        return;
    }
    std::fprintf(stderr, "[RsbsSave] refusing slot file: %s (got %llu, expected %llu)\n",
                 reason, got, expected);
}

// Save() used to fail SILENTLY at five separate points, and every production
// caller discards the bool it returns (games/oot/soh/SaveManager.cpp's OnSaveFile
// and OnExitGame hooks both ignore it). A save that does not happen is therefore
// indistinguishable from one that does, right up until the player reloads and
// finds a stale or empty slot — which is exactly the shape of the "saving is a
// little broken" report this exists to make diagnosable. Load already names every
// refusal; the write path now does too.
bool SaveLogFail(int slot, const char* reason) {
    std::fprintf(stderr, "[RsbsSave] slot %d NOT saved: %s\n", slot, reason);
    return false;
}

}  // namespace

SaveManager& SaveManager::Instance() {
    static SaveManager sInstance;
    return sInstance;
}

void SaveManager::SetSaveDirectory(const std::string& dir) {
    mSaveDir = dir.empty() ? std::string(".") : dir;
}

void SaveManager::SetActiveSlot(int slot) {
    // Normalize anything out of range to "none". The value most likely to
    // arrive here by mistake is MM's 0xFF fileNum sentinel, and silently
    // clamping that to a real slot would write one game's session over an
    // unrelated slot; -1 makes every consumer's "do not write" branch fire.
    mActiveSlot = SlotInRange(slot) ? slot : -1;
}

int SaveManager::GetActiveSlot() const {
    return mActiveSlot;
}

std::string SaveManager::SlotPath(int slot) const {
    return (std::filesystem::path(mSaveDir) /
            ("redship_slot" + std::to_string(slot) + ".redsave"))
        .string();
}

// CRC32 (reflected, polynomial 0xEDB88320 — the zlib/PNG variant). Table-free
// so there is no static-init ordering concern; the payload is only ~24KB.
uint32_t SaveManager::Crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

bool SaveManager::Save(int slot) {
    if (!SlotInRange(slot)) {
        return SaveLogFail(slot, "slot index out of range");
    }

    // Serialization source = the context-layer shadow copies. Both must be
    // present (Context_InitFrozenStates run); otherwise there is nothing to
    // capture and we refuse rather than write a half-empty file.
    const void* ootShadow = Context_GetOoTSaveContext();
    const void* mmShadow = Context_GetMMSaveContext();
    if (ootShadow == nullptr || mmShadow == nullptr) {
        return SaveLogFail(slot, "context shadows are absent (Context_InitFrozenStates has not run)");
    }

    // Assemble Tiers 1..3 contiguously so the CRC covers exactly the bytes we
    // write, in write order: ComboContext, OoT blob, MM blob.
    std::vector<uint8_t> payload;
    payload.reserve(kComboSize + kOoTSize + kMMSize);
    // Tier-1 goes out at the FIXED record size: the live struct, then zeros to
    // the budget. The padding is what gives ComboContext room to grow later
    // without the serialized size — and therefore every existing save file's
    // validity — moving.
    const uint8_t* comboBytes = reinterpret_cast<const uint8_t*>(&gComboCtx);
    payload.insert(payload.end(), comboBytes, comboBytes + sizeof(ComboContext));
    payload.insert(payload.end(), kComboSize - sizeof(ComboContext), uint8_t{0});
    payload.insert(payload.end(), static_cast<const uint8_t*>(ootShadow),
                   static_cast<const uint8_t*>(ootShadow) + kOoTSize);
    payload.insert(payload.end(), static_cast<const uint8_t*>(mmShadow),
                   static_cast<const uint8_t*>(mmShadow) + kMMSize);

    RsbsSaveHeader header;
    std::memset(&header, 0, sizeof(header));
    std::memcpy(header.magic, RSBS_SAVE_MAGIC, sizeof(header.magic));
    header.version = RSBS_SAVE_VERSION;
    header.endian = RSBS_SAVE_ENDIAN_LE;
    header.slot = static_cast<uint8_t>(slot);
    header.headerSize = static_cast<uint16_t>(sizeof(RsbsSaveHeader));
    header.comboSize = kComboSize;
    header.ootSize = kOoTSize;
    header.mmSize = kMMSize;
    header.crc32 = Crc32(payload.data(), payload.size());

    std::error_code ec;
    std::filesystem::create_directories(mSaveDir, ec);

    // Atomic write: fill a temp file, then rename over the real slot so a
    // crash mid-write never leaves a half-written slot.
    const std::string finalPath = SlotPath(slot);
    const std::string tmpPath = finalPath + ".tmp";
    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::fprintf(stderr, "[RsbsSave] slot %d NOT saved: cannot open '%s' for writing\n",
                         slot, tmpPath.c_str());
            return false;
        }
        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
        out.write(reinterpret_cast<const char*>(payload.data()),
                  static_cast<std::streamsize>(payload.size()));
        out.flush();
        if (!out) {
            out.close();
            std::filesystem::remove(tmpPath, ec);
            return SaveLogFail(slot, "write failed (disk full or permissions?); temp file discarded");
        }
    }

    std::filesystem::rename(tmpPath, finalPath, ec);
    if (ec) {
        std::fprintf(stderr, "[RsbsSave] slot %d NOT saved: rename '%s' -> '%s' failed (%s)\n",
                     slot, tmpPath.c_str(), finalPath.c_str(), ec.message().c_str());
        std::filesystem::remove(tmpPath, ec);
        return false;
    }
    return true;
}

bool SaveManager::DeserializeHeader(std::istream& in, RsbsSaveHeader& outHeader,
                                    bool verbose) const {
    RsbsSaveHeader h;
    in.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (!in || in.gcount() != static_cast<std::streamsize>(sizeof(h))) {
        SaveLogReject(verbose, "short header read", static_cast<unsigned long long>(in.gcount()),
                      sizeof(h));
        return false;
    }

    if (std::memcmp(h.magic, RSBS_SAVE_MAGIC, sizeof(h.magic)) != 0) {
        SaveLogReject(verbose, "bad magic (not a .redsave)", 0, 0);
        return false;
    }
    // A version WINDOW, not an equality test. Bumping RSBS_SAVE_VERSION against
    // an equality test is precisely how a format change orphans every existing
    // save; older versions inside the window are prefix-compatible and load.
    if (h.version < RSBS_SAVE_VERSION_MIN || h.version > RSBS_SAVE_VERSION) {
        SaveLogReject(verbose, "unsupported format version", h.version, RSBS_SAVE_VERSION);
        return false;
    }
    if (h.endian != RSBS_SAVE_ENDIAN_LE) {
        SaveLogReject(verbose, "wrong byte order", h.endian, RSBS_SAVE_ENDIAN_LE);
        return false;
    }
    if (h.headerSize != sizeof(RsbsSaveHeader)) {
        SaveLogReject(verbose, "unexpected header size", h.headerSize, sizeof(RsbsSaveHeader));
        return false;
    }
    // ALL THREE tiers are size-field-driven. A STORED size smaller than this
    // build's capacity is a file from an older build — for the game tiers, one
    // written before the capacities covered the ports' full runtime structs
    // (OoT was the N64 0x1428); for Tier-1, one written before ComboContext got
    // a fixed record size, or by any build whose ComboContext simply had fewer
    // trailing fields. Either way the stored bytes are a PREFIX of the current
    // layout, so Load accepts them and zero-extends. Larger than capacity means
    // a newer/foreign layout that cannot fit our buffers: refuse rather than
    // truncate, because truncating Tier-1 would silently drop cross-game state.
    if (h.comboSize == 0 || h.comboSize > kComboSize) {
        SaveLogReject(verbose, "Tier-1 (ComboContext) size out of range", h.comboSize, kComboSize);
        return false;
    }
    if (h.ootSize == 0 || h.ootSize > kOoTSize) {
        SaveLogReject(verbose, "Tier-2 (OoT) size out of range", h.ootSize, kOoTSize);
        return false;
    }
    if (h.mmSize == 0 || h.mmSize > kMMSize) {
        SaveLogReject(verbose, "Tier-3 (MM) size out of range", h.mmSize, kMMSize);
        return false;
    }

    outHeader = h;
    return true;
}

bool SaveManager::Load(int slot) {
    if (!SlotInRange(slot)) {
        return false;
    }

    std::ifstream in(SlotPath(slot), std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "[RsbsSave] cannot open slot %d for load\n", slot);
        return false;
    }

    RsbsSaveHeader header;
    if (!DeserializeHeader(in, header, /*verbose=*/true)) {
        return false;
    }

    // Read the whole payload into locals FIRST and validate everything before
    // committing — a bad file (short read, CRC mismatch, bad inner magic) must
    // not clobber live state. Reads are driven by the header's STORED tier
    // sizes; the blobs are allocated at full capacity and zero-filled so a
    // shorter tier from an older build is zero-extended.
    //
    // Tier-1 gets the same treatment via a full-record staging buffer: the
    // struct is copied out of the FRONT of it, so fields appended since the file
    // was written read as zero — exactly the value ComboContext_Init gives them
    // — instead of whatever was on the stack.
    std::vector<uint8_t> comboRecord(kComboSize, 0);
    std::vector<uint8_t> ootBlob(kOoTSize, 0);
    std::vector<uint8_t> mmBlob(kMMSize, 0);

    in.read(reinterpret_cast<char*>(comboRecord.data()), header.comboSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(header.comboSize)) {
        std::fprintf(stderr, "[RsbsSave] slot %d truncated in Tier-1\n", slot);
        return false;
    }
    in.read(reinterpret_cast<char*>(ootBlob.data()), header.ootSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(header.ootSize)) {
        std::fprintf(stderr, "[RsbsSave] slot %d truncated in Tier-2 (OoT)\n", slot);
        return false;
    }
    in.read(reinterpret_cast<char*>(mmBlob.data()), header.mmSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(header.mmSize)) {
        std::fprintf(stderr, "[RsbsSave] slot %d truncated in Tier-3 (MM)\n", slot);
        return false;
    }

    ComboContext combo;
    std::memcpy(&combo, comboRecord.data(), sizeof(ComboContext));

    // CRC over Tiers 1..3 exactly as stored (not the zero-extended tails), in
    // the same contiguous order they were written.
    std::vector<uint8_t> payload;
    payload.reserve(header.comboSize + header.ootSize + header.mmSize);
    payload.insert(payload.end(), comboRecord.begin(), comboRecord.begin() + header.comboSize);
    payload.insert(payload.end(), ootBlob.begin(), ootBlob.begin() + header.ootSize);
    payload.insert(payload.end(), mmBlob.begin(), mmBlob.begin() + header.mmSize);
    if (Crc32(payload.data(), payload.size()) != header.crc32) {
        std::fprintf(stderr, "[RsbsSave] slot %d failed CRC — file is corrupt, load refused\n", slot);
        return false;
    }

    // Inner ComboContext magic guards against a structurally-valid file whose
    // Tier-1 contents are not actually a ComboContext.
    if (std::memcmp(combo.magic, COMBO_CONTEXT_MAGIC, sizeof(combo.magic)) != 0) {
        std::fprintf(stderr, "[RsbsSave] slot %d Tier-1 is not a ComboContext, load refused\n", slot);
        return false;
    }

    // All checks passed — commit. gComboCtx and both shadows are updated.
    std::memcpy(&gComboCtx, &combo, sizeof(ComboContext));
    Context_UpdateShadowCopy(GAME_OOT, ootBlob.data(), kOoTSize);
    Context_UpdateShadowCopy(GAME_MM, mmBlob.data(), kMMSize);

    // ARM the MM half so it is actually reachable.
    //
    // UpdateShadowCopy writes bytes without setting hasBeenFrozen, and every
    // consumer that can move a blob into a live gSaveContext gates on that flag
    // — so before this, a faithfully loaded MM save sat in memory byte-exact
    // and structurally unreachable, then got overwritten by the bootstrap file
    // the next crossing produced. That is the read-side half of "MM will be
    // reset after game restart".
    //
    // MM ONLY, deliberately. OoT's own file{N+1}.sav is the authority for OoT
    // state and is applied by Sram_OpenSave on the normal load path; arming
    // Tier-2 as well would race a second, staler copy of OoT's world against
    // it. MM has no such per-game file in single-exe — the unified slot is its
    // only persistence — so Tier-3 is the one that needs delivering.
    //
    // Arming is refused for an all-zero tier (see Context_ArmShadowAsFrozen),
    // which is exactly a slot saved before the player ever entered MM: that
    // must keep cold-booting MM's own bootstrap rather than restoring a zeroed
    // SaveContext over it. Entrance 0 is a placeholder, not a spawn decision —
    // where a resumed MM session actually lands is MM's own owl / new-cycle
    // policy.
    const int armed = Context_ArmShadowAsFrozen(GAME_MM, 0);
    std::fprintf(stderr, "[RsbsSave] slot %d loaded; MM half %s\n", slot,
                 armed ? "armed for restore" : "empty (MM will cold-boot)");
    return true;
}

bool SaveManager::HasSave(int slot) const {
    if (!SlotInRange(slot)) {
        return false;
    }
    std::ifstream in(SlotPath(slot), std::ios::binary);
    if (!in) {
        return false;
    }
    RsbsSaveHeader header;
    return DeserializeHeader(in, header, /*verbose=*/false);
}

void SaveManager::DeleteSave(int slot) {
    if (!SlotInRange(slot)) {
        return;
    }
    std::error_code ec;
    const std::string path = SlotPath(slot);
    std::filesystem::remove(path, ec);
    std::filesystem::remove(path + ".tmp", ec);
    std::filesystem::remove(path + ".bak", ec);
}

void SaveManager::RegisterGameMeta(GameId game, const RsbsGameMetaDesc* desc) {
    if (desc == nullptr) {
        return;
    }
    if (game != GAME_OOT && game != GAME_MM) {
        return;
    }
    const size_t idx = static_cast<size_t>(game);
    mMetaDescs[idx] = *desc;
    mMetaPresent[idx] = true;
}

namespace {

// Pulls game-specific metadata fields out of a blob using the registered
// descriptor. `blob` is the raw bytes of either the OoT or MM SaveContext as
// laid down in the slot file; `blobSize` lets us bounds-check every offset
// independently rather than trusting the caller. `outName` must point at a
// 9-byte buffer (we always NUL-terminate); `outPlayTime`/`outStarted` are
// written even on the unregistered path (zero / false) so callers don't have
// to clear them themselves.
void ExtractGameMeta(const RsbsGameMetaDesc& desc, const uint8_t* blob, size_t blobSize,
                     char outName[9], uint32_t& outPlayTime, bool& outStarted) {
    std::memset(outName, 0, 9);
    outPlayTime = 0;
    outStarted = false;

    uint32_t nameLen = desc.playerNameLen;
    if (nameLen > 8) {
        nameLen = 8;
    }
    if (nameLen > 0 && static_cast<size_t>(desc.playerNameOffset) + nameLen <= blobSize) {
        for (uint32_t i = 0; i < nameLen; i++) {
            char c = static_cast<char>(blob[desc.playerNameOffset + i]);
            // Treat 0x00, the N64 0xDF "space", and stray high-bit bytes as
            // end-of-name so the panel doesn't render control gibberish from a
            // newly-allocated slot.
            if (c == '\0') {
                break;
            }
            outName[i] = c;
        }
    }

    if (static_cast<size_t>(desc.playTimeOffset) + sizeof(uint32_t) <= blobSize) {
        uint32_t pt = 0;
        std::memcpy(&pt, blob + desc.playTimeOffset, sizeof(pt));
        outPlayTime = pt;
    }

    if (desc.validMarkerLen == 0) {
        // No marker registered → assume any slot is "started" for this game.
        outStarted = true;
    } else if (desc.validMarkerLen <= sizeof(desc.validMarker) &&
               static_cast<size_t>(desc.validMarkerOffset) + desc.validMarkerLen <= blobSize) {
        outStarted = std::memcmp(blob + desc.validMarkerOffset, desc.validMarker,
                                 desc.validMarkerLen) == 0;
    }
}

}  // namespace

SlotMeta SaveManager::ReadMeta(int slot) const {
    SlotMeta meta{};
    meta.slot = static_cast<uint8_t>(slot < 0 ? 0 : slot);
    meta.lastGame = GAME_NONE;

    if (!SlotInRange(slot)) {
        return meta;
    }

    std::ifstream in(SlotPath(slot), std::ios::binary);
    if (!in) {
        return meta;
    }
    meta.exists = true;

    RsbsSaveHeader header;
    if (!DeserializeHeader(in, header, /*verbose=*/false)) {
        return meta;  // exists=true, valid=false
    }
    meta.valid = true;
    meta.slot = header.slot;

    // Read Tier-1 (ComboContext) and the game tiers at their STORED sizes to
    // pull the registered metadata bytes (offsets past a shorter legacy blob
    // simply read as "absent"). We deliberately do NOT CRC the file here:
    // ReadMeta is called for every slot every menu frame, and CRC over ~200KB
    // each time would dominate the file-select draw. Load() still CRC-checks
    // when the user actually picks a slot.
    // Same zero-filled staging + prefix-copy as Load: a legacy short Tier-1
    // must still yield a readable sourceGame, or the file-select panel would
    // label every pre-headroom slot as belonging to no game.
    std::vector<uint8_t> comboRecord(kComboSize, 0);
    in.read(reinterpret_cast<char*>(comboRecord.data()), header.comboSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(header.comboSize)) {
        meta.valid = false;
        return meta;
    }
    ComboContext combo;
    std::memcpy(&combo, comboRecord.data(), sizeof(ComboContext));
    if (std::memcmp(combo.magic, COMBO_CONTEXT_MAGIC, sizeof(combo.magic)) == 0) {
        meta.lastGame = combo.sourceGame;
    }

    std::vector<uint8_t> ootBlob(header.ootSize);
    in.read(reinterpret_cast<char*>(ootBlob.data()), header.ootSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(header.ootSize)) {
        meta.valid = false;
        return meta;
    }
    std::vector<uint8_t> mmBlob(header.mmSize);
    in.read(reinterpret_cast<char*>(mmBlob.data()), header.mmSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(header.mmSize)) {
        meta.valid = false;
        return meta;
    }

    if (mMetaPresent[GAME_OOT]) {
        ExtractGameMeta(mMetaDescs[GAME_OOT], ootBlob.data(), ootBlob.size(),
                        meta.ootName, meta.ootPlayTime, meta.ootStarted);
    }
    if (mMetaPresent[GAME_MM]) {
        ExtractGameMeta(mMetaDescs[GAME_MM], mmBlob.data(), mmBlob.size(),
                        meta.mmName, meta.mmPlayTime, meta.mmStarted);
    }

    return meta;
}

}  // namespace rsbs

// ============================================================================
// C shim
// ============================================================================

extern "C" {

int RsbsSave_Save(int slot) {
    return rsbs::SaveManager::Instance().Save(slot) ? 1 : 0;
}

int RsbsSave_Load(int slot) {
    return rsbs::SaveManager::Instance().Load(slot) ? 1 : 0;
}

int RsbsSave_HasSave(int slot) {
    return rsbs::SaveManager::Instance().HasSave(slot) ? 1 : 0;
}

void RsbsSave_DeleteSave(int slot) {
    rsbs::SaveManager::Instance().DeleteSave(slot);
}

void RsbsSave_SetActiveSlot(int slot) {
    rsbs::SaveManager::Instance().SetActiveSlot(slot);
}

int RsbsSave_GetActiveSlot(void) {
    return rsbs::SaveManager::Instance().GetActiveSlot();
}

void RsbsSave_RegisterGameMeta(GameId game, const RsbsGameMetaDesc* desc) {
    rsbs::SaveManager::Instance().RegisterGameMeta(game, desc);
}

}  // extern "C"
