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

// Tier sizes as this build writes them. The game-tier constants are the blob
// CAPACITIES from game.h — the same values the context-layer shadow buffers
// are allocated at, so reading the shadows here is always in-bounds. On Load
// the header's stored sizes drive the reads instead: older files with shorter
// game tiers are accepted and zero-extended (see DeserializeHeader).
constexpr uint32_t kComboSize = static_cast<uint32_t>(sizeof(ComboContext));
constexpr uint32_t kOoTSize = static_cast<uint32_t>(OOT_SAVE_CONTEXT_SIZE);
constexpr uint32_t kMMSize = static_cast<uint32_t>(MM_SAVE_CONTEXT_SIZE);

bool SlotInRange(int slot) {
    return slot >= 0 && slot < RSBS_SAVE_MAX_SLOTS;
}

}  // namespace

SaveManager& SaveManager::Instance() {
    static SaveManager sInstance;
    return sInstance;
}

void SaveManager::SetSaveDirectory(const std::string& dir) {
    mSaveDir = dir.empty() ? std::string(".") : dir;
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
        return false;
    }

    // Serialization source = the context-layer shadow copies. Both must be
    // present (Context_InitFrozenStates run); otherwise there is nothing to
    // capture and we refuse rather than write a half-empty file.
    const void* ootShadow = Context_GetOoTSaveContext();
    const void* mmShadow = Context_GetMMSaveContext();
    if (ootShadow == nullptr || mmShadow == nullptr) {
        return false;
    }

    // Assemble Tiers 1..3 contiguously so the CRC covers exactly the bytes we
    // write, in write order: ComboContext, OoT blob, MM blob.
    std::vector<uint8_t> payload;
    payload.reserve(kComboSize + kOoTSize + kMMSize);
    const uint8_t* comboBytes = reinterpret_cast<const uint8_t*>(&gComboCtx);
    payload.insert(payload.end(), comboBytes, comboBytes + kComboSize);
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
            return false;
        }
        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
        out.write(reinterpret_cast<const char*>(payload.data()),
                  static_cast<std::streamsize>(payload.size()));
        out.flush();
        if (!out) {
            out.close();
            std::filesystem::remove(tmpPath, ec);
            return false;
        }
    }

    std::filesystem::rename(tmpPath, finalPath, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        return false;
    }
    return true;
}

bool SaveManager::DeserializeHeader(std::istream& in, RsbsSaveHeader& outHeader) const {
    RsbsSaveHeader h;
    in.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (!in || in.gcount() != static_cast<std::streamsize>(sizeof(h))) {
        return false;
    }

    if (std::memcmp(h.magic, RSBS_SAVE_MAGIC, sizeof(h.magic)) != 0) {
        return false;
    }
    if (h.version != RSBS_SAVE_VERSION) {
        return false;  // older/newer layout — refuse rather than guess
    }
    if (h.endian != RSBS_SAVE_ENDIAN_LE) {
        return false;
    }
    if (h.headerSize != sizeof(RsbsSaveHeader)) {
        return false;
    }
    // Tier-1 must match exactly: ComboContext has no capacity headroom, and a
    // different size means a different struct layout.
    if (h.comboSize != kComboSize) {
        return false;
    }
    // Game tiers are size-field-driven: a STORED size smaller than this
    // build's blob capacity is a file from an older build (e.g. the OoT tier
    // was the N64 0x1428 before the capacity covered SoH's full runtime
    // struct) — its bytes are a prefix of the same layout, so Load accepts it
    // and zero-extends. Larger than capacity means a newer/foreign layout that
    // cannot fit the shadow buffers: refuse rather than truncate.
    if (h.ootSize == 0 || h.ootSize > kOoTSize || h.mmSize == 0 || h.mmSize > kMMSize) {
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
        return false;
    }

    RsbsSaveHeader header;
    if (!DeserializeHeader(in, header)) {
        return false;
    }

    // Read the whole payload into locals FIRST and validate everything before
    // committing — a bad file (short read, CRC mismatch, bad inner magic) must
    // not clobber live state. Reads are driven by the header's STORED tier
    // sizes; the blobs are allocated at full capacity and zero-filled so a
    // shorter tier from an older build is zero-extended.
    ComboContext combo;
    std::vector<uint8_t> ootBlob(kOoTSize, 0);
    std::vector<uint8_t> mmBlob(kMMSize, 0);

    in.read(reinterpret_cast<char*>(&combo), kComboSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(kComboSize)) {
        return false;
    }
    in.read(reinterpret_cast<char*>(ootBlob.data()), header.ootSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(header.ootSize)) {
        return false;
    }
    in.read(reinterpret_cast<char*>(mmBlob.data()), header.mmSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(header.mmSize)) {
        return false;
    }

    // CRC over Tiers 1..3 exactly as stored (not the zero-extended tails), in
    // the same contiguous order they were written.
    std::vector<uint8_t> payload;
    payload.reserve(kComboSize + header.ootSize + header.mmSize);
    const uint8_t* comboBytes = reinterpret_cast<const uint8_t*>(&combo);
    payload.insert(payload.end(), comboBytes, comboBytes + kComboSize);
    payload.insert(payload.end(), ootBlob.begin(), ootBlob.begin() + header.ootSize);
    payload.insert(payload.end(), mmBlob.begin(), mmBlob.begin() + header.mmSize);
    if (Crc32(payload.data(), payload.size()) != header.crc32) {
        return false;
    }

    // Inner ComboContext magic guards against a structurally-valid file whose
    // Tier-1 contents are not actually a ComboContext.
    if (std::memcmp(combo.magic, COMBO_CONTEXT_MAGIC, sizeof(combo.magic)) != 0) {
        return false;
    }

    // All checks passed — commit. gComboCtx and both shadows are updated; the
    // active game's live SaveContext is re-hydrated from its shadow by the
    // existing freeze/restore path on the next cross-game switch.
    std::memcpy(&gComboCtx, &combo, sizeof(ComboContext));
    Context_UpdateShadowCopy(GAME_OOT, ootBlob.data(), kOoTSize);
    Context_UpdateShadowCopy(GAME_MM, mmBlob.data(), kMMSize);
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
    return DeserializeHeader(in, header);
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
    if (!DeserializeHeader(in, header)) {
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
    ComboContext combo;
    in.read(reinterpret_cast<char*>(&combo), kComboSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(kComboSize)) {
        meta.valid = false;
        return meta;
    }
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

void RsbsSave_RegisterGameMeta(GameId game, const RsbsGameMetaDesc* desc) {
    rsbs::SaveManager::Instance().RegisterGameMeta(game, desc);
}

}  // extern "C"
