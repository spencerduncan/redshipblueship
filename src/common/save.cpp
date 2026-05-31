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

// Tier sizes as this build understands them. A loaded file must match these
// exactly (guarded in Load); the constants come from game.h, which is also what
// the context-layer shadow buffers are sized at, so reading the shadows here is
// always in-bounds.
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
    // Tier sizes must match this build exactly; a mismatch means the struct
    // layout changed, so the blobs are not safe to memcpy back.
    if (h.comboSize != kComboSize || h.ootSize != kOoTSize || h.mmSize != kMMSize) {
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
    // not clobber live state.
    ComboContext combo;
    std::vector<uint8_t> ootBlob(kOoTSize);
    std::vector<uint8_t> mmBlob(kMMSize);

    in.read(reinterpret_cast<char*>(&combo), kComboSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(kComboSize)) {
        return false;
    }
    in.read(reinterpret_cast<char*>(ootBlob.data()), kOoTSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(kOoTSize)) {
        return false;
    }
    in.read(reinterpret_cast<char*>(mmBlob.data()), kMMSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(kMMSize)) {
        return false;
    }

    // CRC over Tiers 1..3, in the same contiguous order they were written.
    std::vector<uint8_t> payload;
    payload.reserve(kComboSize + kOoTSize + kMMSize);
    const uint8_t* comboBytes = reinterpret_cast<const uint8_t*>(&combo);
    payload.insert(payload.end(), comboBytes, comboBytes + kComboSize);
    payload.insert(payload.end(), ootBlob.begin(), ootBlob.end());
    payload.insert(payload.end(), mmBlob.begin(), mmBlob.end());
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

}  // extern "C"
