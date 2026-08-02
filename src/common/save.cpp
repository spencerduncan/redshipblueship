/**
 * @file save.cpp
 * @brief Unified cross-game save file (.redsave) implementation (Phase 2 T6, #35).
 *
 * See save.h for the format. This file depends only on src/common (context +
 * game headers) and the standard library — never on either game's z64save.h.
 */

#include "save.h"

#include "context.h"
#include "entrance.h" // MM_ENTR_SOUTH_CLOCK_TOWN_0 — the armed blob's return entrance
#include "game.h"
#include "shared_resources.h"

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

// Filename-safe tag for the quarantine rename, so the renamed-aside evidence
// names its own diagnosis: redship_slot0.redsave.refused-crc.bak.
const char* RefuseReasonSlug(RsbsRefuseReason reason) {
    switch (reason) {
        case RSBS_REFUSE_UNREADABLE:
            return "unreadable";
        case RSBS_REFUSE_HEADER:
            return "header";
        case RSBS_REFUSE_VERSION:
            return "version";
        case RSBS_REFUSE_TIER_SIZE:
            return "tiersize";
        case RSBS_REFUSE_WRONG_SLOT:
            return "wrongslot";
        case RSBS_REFUSE_TRUNCATED:
            return "truncated";
        case RSBS_REFUSE_CRC:
            return "crc";
        case RSBS_REFUSE_COMBO_MAGIC:
            return "combomagic";
        case RSBS_REFUSE_COMMIT_SKEW:
            return "commitskew";
        case RSBS_REFUSE_IDENTITY:
            // Never actually used in a quarantine name today — an identity
            // refusal leaves the healthy slot file in place (RefuseSlotIdentity
            // quarantines nothing) — but kept total so a future producer that
            // does rename gets a truthful tag instead of "unknown".
            return "identity";
        case RSBS_REFUSE_GENERATION:
            // Same situation as RSBS_REFUSE_IDENTITY: a session refusal that
            // quarantines nothing, tagged totally for the same reason.
            return "generation";
        case RSBS_REFUSE_NONE:
        default:
            return "unknown";
    }
}

}  // namespace

const char* SaveManager::RefuseReasonLabel(RsbsRefuseReason reason) {
    switch (reason) {
        case RSBS_REFUSE_UNREADABLE:
            return "file unreadable";
        case RSBS_REFUSE_HEADER:
            return "not a .redsave (bad header)";
        case RSBS_REFUSE_VERSION:
            return "unsupported format version";
        case RSBS_REFUSE_TIER_SIZE:
            return "tier larger than this build supports";
        case RSBS_REFUSE_WRONG_SLOT:
            return "file claims a different slot";
        case RSBS_REFUSE_TRUNCATED:
            return "file truncated";
        case RSBS_REFUSE_CRC:
            return "corrupt (CRC mismatch)";
        case RSBS_REFUSE_COMBO_MAGIC:
            return "cross-game record damaged";
        case RSBS_REFUSE_COMMIT_SKEW:
            return "older than the OoT save (a commit is missing)";
        case RSBS_REFUSE_IDENTITY:
            return "options differ from this pair's creation";
        case RSBS_REFUSE_GENERATION:
            return "the paired Termina world could not be generated";
        case RSBS_REFUSE_NONE:
        default:
            return "";
    }
}

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

uint32_t SaveManager::StageCommit() {
    // GAME THREAD ONLY — this is the marshalling half of the commit choke
    // point (#537). Serialization source = the context-layer shadow copies.
    // Both must be present (Context_InitFrozenStates run); otherwise there is
    // nothing to capture and we refuse rather than stage a half-empty commit.
    const void* ootShadow = Context_GetOoTSaveContext();
    const void* mmShadow = Context_GetMMSaveContext();
    if (ootShadow == nullptr || mmShadow == nullptr) {
        std::fprintf(stderr, "[RsbsSave] commit NOT staged: context shadows are absent "
                             "(Context_InitFrozenStates has not run)\n");
        // INVALIDATE rather than just refuse. WriteStagedCommit serializes
        // "the most recently staged snapshot" for whatever slot its caller
        // names, and OoT's worker hook fires unconditionally after the .sav
        // write — so leaving an EARLIER stage addressable here would let a
        // save whose own marshalling failed publish a previous commit's
        // snapshot, potentially into a different slot. A failed stage must
        // leave nothing to write.
        std::lock_guard<std::mutex> lock(mStageMtx);
        mStaged.valid = false;
        return 0;
    }

    // Stamp the monotonic commit generation BEFORE copying, so the staged
    // Tier-1 (and therefore the artifact) carries it. gComboCtx is game-thread
    // state; mutating it here is legal precisely because staging is
    // game-thread-only. A loaded slot resumes its own counter (Load memcpys
    // the whole struct back), so the sequence is monotonic across sessions.
    gComboCtx.commitGeneration += 1;
    const uint32_t generation = gComboCtx.commitGeneration;

    {
        std::lock_guard<std::mutex> lock(mStageMtx);
        std::memcpy(&mStaged.combo, &gComboCtx, sizeof(ComboContext));
        mStaged.oot.assign(static_cast<const uint8_t*>(ootShadow),
                           static_cast<const uint8_t*>(ootShadow) + kOoTSize);
        mStaged.mm.assign(static_cast<const uint8_t*>(mmShadow),
                          static_cast<const uint8_t*>(mmShadow) + kMMSize);
        mStaged.generation = generation;
        mStaged.valid = true;
    }
    return generation;
}

bool SaveManager::CheckWriteAllowed(int slot) const {
    // #533 armed-session latch. Writing is legal only after THIS SESSION
    // successfully loaded, created, or erased the slot. Without this gate, a
    // session that refused (or simply never looked at) a slot's .redsave could
    // rename-overwrite the only copy of the player's MM half with a blank
    // Tier-1 + all-zero Tier-3 — the #533 data-loss conversion this latch
    // exists to prevent. The refusal-specific message names the evidence.
    if (!mSlotArmed[slot]) {
        if (mSlotRefused[slot] != RSBS_REFUSE_NONE) {
            return SaveLogFail(slot, "write latched: this session REFUSED the slot's .redsave "
                                     "(evidence quarantined beside it); erase the slot to write");
        }
        return SaveLogFail(slot, "write latched: slot was not loaded, created, or erased this session");
    }
    return true;
}

bool SaveManager::WriteStagedCommit(int slot) {
    if (!SlotInRange(slot)) {
        return SaveLogFail(slot, "slot index out of range");
    }
    // The choke point honors the #533 latch: a REFUSED (or never-established)
    // slot stays unwritten even when a perfectly coherent snapshot is staged.
    if (!CheckWriteAllowed(slot)) {
        return false;
    }

    // Copy the staged snapshot out under the lock, then serialize from the
    // LOCAL copy only. This function must never read gComboCtx or the live
    // shadows: it runs on SoH's save worker thread while the game thread keeps
    // mutating both, which is exactly the #537 tear this choke point removes.
    ComboContext combo;
    std::vector<uint8_t> ootBlob;
    std::vector<uint8_t> mmBlob;
    {
        std::lock_guard<std::mutex> lock(mStageMtx);
        if (!mStaged.valid) {
            return SaveLogFail(slot, "no commit has been staged this session (StageCommit not run)");
        }
        std::memcpy(&combo, &mStaged.combo, sizeof(ComboContext));
        ootBlob = mStaged.oot;
        mmBlob = mStaged.mm;
    }
    return WriteSlotFile(slot, combo, ootBlob.data(), mmBlob.data());
}

bool SaveManager::Save(int slot) {
    // Latch check + stage + write on the calling thread. Game thread only (it
    // stages). The latch is checked BEFORE staging: a refused write must not
    // advance the monotonic commit generation (the stamp belongs to durable
    // commits only, and a phantom advance would fake cross-artifact skew).
    if (!SlotInRange(slot)) {
        return SaveLogFail(slot, "slot index out of range");
    }
    if (!CheckWriteAllowed(slot)) {
        return false;
    }
    if (StageCommit() == 0) {
        return SaveLogFail(slot, "commit staging refused (see above)");
    }
    return WriteStagedCommit(slot);
}

bool SaveManager::WriteSlotFile(int slot, const ComboContext& combo, const uint8_t* ootBlob,
                                const uint8_t* mmBlob) {
    // One writer at a time: the OnSaveFile worker (WriteStagedCommit) and a
    // synchronous game-thread commit (MM's capture, OnExitGame) share the same
    // `.tmp` staging path per slot, and interleaved temp writes would produce
    // a CRC-invalid file. The snapshot arguments are already immutable, so
    // holding the lock across serialization stays deadlock-free (mStageMtx is
    // never taken here).
    std::lock_guard<std::mutex> writeLock(mWriteMtx);

    // Assemble Tiers 1..3 contiguously so the CRC covers exactly the bytes we
    // write, in write order: ComboContext, OoT blob, MM blob.
    std::vector<uint8_t> payload;
    payload.reserve(kComboSize + kOoTSize + kMMSize);
    // Tier-1 goes out at the FIXED record size: the snapshot struct, then
    // zeros to the budget. The padding is what gives ComboContext room to grow
    // later without the serialized size — and therefore every existing save
    // file's validity — moving.
    const uint8_t* comboBytes = reinterpret_cast<const uint8_t*>(&combo);
    payload.insert(payload.end(), comboBytes, comboBytes + sizeof(ComboContext));
    payload.insert(payload.end(), kComboSize - sizeof(ComboContext), uint8_t{0});
    payload.insert(payload.end(), ootBlob, ootBlob + kOoTSize);
    payload.insert(payload.end(), mmBlob, mmBlob + kMMSize);

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

bool SaveManager::DeserializeHeader(std::istream& in, int expectedSlot, RsbsSaveHeader& outHeader,
                                    bool verbose, RsbsRefuseReason* outReason) const {
    RsbsRefuseReason scratch = RSBS_REFUSE_NONE;
    RsbsRefuseReason& reason = outReason != nullptr ? *outReason : scratch;
    reason = RSBS_REFUSE_NONE;

    RsbsSaveHeader h;
    in.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (!in || in.gcount() != static_cast<std::streamsize>(sizeof(h))) {
        SaveLogReject(verbose, "short header read", static_cast<unsigned long long>(in.gcount()),
                      sizeof(h));
        reason = RSBS_REFUSE_HEADER;
        return false;
    }

    if (std::memcmp(h.magic, RSBS_SAVE_MAGIC, sizeof(h.magic)) != 0) {
        SaveLogReject(verbose, "bad magic (not a .redsave)", 0, 0);
        reason = RSBS_REFUSE_HEADER;
        return false;
    }
    // A version WINDOW, not an equality test. Bumping RSBS_SAVE_VERSION against
    // an equality test is precisely how a format change orphans every existing
    // save; older versions inside the window are prefix-compatible and load.
    if (h.version < RSBS_SAVE_VERSION_MIN || h.version > RSBS_SAVE_VERSION) {
        SaveLogReject(verbose, "unsupported format version", h.version, RSBS_SAVE_VERSION);
        reason = RSBS_REFUSE_VERSION;
        return false;
    }
    if (h.endian != RSBS_SAVE_ENDIAN_LE) {
        SaveLogReject(verbose, "wrong byte order", h.endian, RSBS_SAVE_ENDIAN_LE);
        reason = RSBS_REFUSE_HEADER;
        return false;
    }
    if (h.headerSize != sizeof(RsbsSaveHeader)) {
        SaveLogReject(verbose, "unexpected header size", h.headerSize, sizeof(RsbsSaveHeader));
        reason = RSBS_REFUSE_HEADER;
        return false;
    }
    // header.slot was stamped at write time but never compared until #533
    // (V13): a cloud-sync conflict or hand copy of slot 2's file sitting at
    // slot 0's path would load silently, attaching one pair's identity and MM
    // world to a different OoT file. Same treatment as a CRC failure: refuse.
    if (expectedSlot >= 0 && h.slot != static_cast<uint8_t>(expectedSlot)) {
        SaveLogReject(verbose, "header.slot does not match this slot path", h.slot,
                      static_cast<unsigned long long>(expectedSlot));
        reason = RSBS_REFUSE_WRONG_SLOT;
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
        reason = RSBS_REFUSE_TIER_SIZE;
        return false;
    }
    if (h.ootSize == 0 || h.ootSize > kOoTSize) {
        SaveLogReject(verbose, "Tier-2 (OoT) size out of range", h.ootSize, kOoTSize);
        reason = RSBS_REFUSE_TIER_SIZE;
        return false;
    }
    if (h.mmSize == 0 || h.mmSize > kMMSize) {
        SaveLogReject(verbose, "Tier-3 (MM) size out of range", h.mmSize, kMMSize);
        reason = RSBS_REFUSE_TIER_SIZE;
        return false;
    }

    outHeader = h;
    return true;
}

struct SaveManager::SlotFileData {
    RsbsSaveHeader header{};
    std::vector<uint8_t> comboRecord;
    std::vector<uint8_t> ootBlob;
    std::vector<uint8_t> mmBlob;
};

SaveManager::SlotReadResult SaveManager::ReadSlotFile(int slot, SlotFileData& out,
                                                      RsbsRefuseReason& outReason, bool verbose) const {
    outReason = RSBS_REFUSE_NONE;

    const std::string path = SlotPath(slot);
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            return SlotReadResult::Absent;
        }
        // Present but unopenable is NOT absence — treating it as absence is
        // exactly the #533 collapse this type exists to prevent.
        if (verbose) {
            std::fprintf(stderr, "[RsbsSave] slot %d exists but cannot be opened\n", slot);
        }
        outReason = RSBS_REFUSE_UNREADABLE;
        return SlotReadResult::Refused;
    }

    if (!DeserializeHeader(in, slot, out.header, verbose, &outReason)) {
        return SlotReadResult::Refused;
    }
    const RsbsSaveHeader& header = out.header;

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
    out.comboRecord.assign(kComboSize, 0);
    out.ootBlob.assign(kOoTSize, 0);
    out.mmBlob.assign(kMMSize, 0);

    in.read(reinterpret_cast<char*>(out.comboRecord.data()), header.comboSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(header.comboSize)) {
        if (verbose) {
            std::fprintf(stderr, "[RsbsSave] slot %d truncated in Tier-1\n", slot);
        }
        outReason = RSBS_REFUSE_TRUNCATED;
        return SlotReadResult::Refused;
    }
    in.read(reinterpret_cast<char*>(out.ootBlob.data()), header.ootSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(header.ootSize)) {
        if (verbose) {
            std::fprintf(stderr, "[RsbsSave] slot %d truncated in Tier-2 (OoT)\n", slot);
        }
        outReason = RSBS_REFUSE_TRUNCATED;
        return SlotReadResult::Refused;
    }
    in.read(reinterpret_cast<char*>(out.mmBlob.data()), header.mmSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(header.mmSize)) {
        if (verbose) {
            std::fprintf(stderr, "[RsbsSave] slot %d truncated in Tier-3 (MM)\n", slot);
        }
        outReason = RSBS_REFUSE_TRUNCATED;
        return SlotReadResult::Refused;
    }

    // CRC over Tiers 1..3 exactly as stored (not the zero-extended tails), in
    // the same contiguous order they were written.
    std::vector<uint8_t> payload;
    payload.reserve(header.comboSize + header.ootSize + header.mmSize);
    payload.insert(payload.end(), out.comboRecord.begin(), out.comboRecord.begin() + header.comboSize);
    payload.insert(payload.end(), out.ootBlob.begin(), out.ootBlob.begin() + header.ootSize);
    payload.insert(payload.end(), out.mmBlob.begin(), out.mmBlob.begin() + header.mmSize);
    if (Crc32(payload.data(), payload.size()) != header.crc32) {
        if (verbose) {
            std::fprintf(stderr, "[RsbsSave] slot %d failed CRC — file is corrupt, load refused\n", slot);
        }
        outReason = RSBS_REFUSE_CRC;
        return SlotReadResult::Refused;
    }

    // Inner ComboContext magic guards against a structurally-valid file whose
    // Tier-1 contents are not actually a ComboContext.
    ComboContext combo;
    std::memcpy(&combo, out.comboRecord.data(), sizeof(ComboContext));
    if (std::memcmp(combo.magic, COMBO_CONTEXT_MAGIC, sizeof(combo.magic)) != 0) {
        if (verbose) {
            std::fprintf(stderr, "[RsbsSave] slot %d Tier-1 is not a ComboContext, load refused\n", slot);
        }
        outReason = RSBS_REFUSE_COMBO_MAGIC;
        return SlotReadResult::Refused;
    }

    return SlotReadResult::Ok;
}

void SaveManager::QuarantineSlotFile(int slot, RsbsRefuseReason reason) {
    const std::string path = SlotPath(slot);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return;
    }
    // Reason-suffixed, and NEVER overwriting existing evidence: a second
    // refusal of the same kind dedupes with a numeric suffix instead of
    // replacing the first quarantined file.
    const std::string base = path + ".refused-" + RefuseReasonSlug(reason);
    std::string target = base + ".bak";
    for (int n = 2; std::filesystem::exists(target, ec); n++) {
        target = base + "-" + std::to_string(n) + ".bak";
    }
    std::filesystem::rename(path, target, ec);
    if (ec) {
        // Rename failed (locked file, permissions). The evidence stays IN
        // PLACE, which is still safe: the caller latches the slot and Save()
        // refuses to touch an unarmed slot, so nothing overwrites it.
        std::fprintf(stderr, "[RsbsSave] slot %d quarantine FAILED (%s); refused file left in place\n",
                     slot, ec.message().c_str());
        return;
    }
    std::fprintf(stderr, "[RsbsSave] slot %d refused file quarantined to '%s'\n", slot, target.c_str());
}

int SaveManager::CompareCommitGenerations(uint32_t redsaveGeneration, uint32_t ootSavGeneration) {
    // Either side at 0 predates the stamp (legacy artifact, or a .sav that
    // never rode a choke-point commit); exempt rather than false-positive on
    // every upgraded install's first load.
    if (redsaveGeneration == 0 || ootSavGeneration == 0 || redsaveGeneration == ootSavGeneration) {
        return 0;
    }
    return redsaveGeneration > ootSavGeneration ? 1 : -1;
}

RsbsLoadOutcome SaveManager::LoadSlot(int slot, uint32_t ootSavGeneration) {
    if (!SlotInRange(slot)) {
        return RSBS_LOAD_REFUSED;
    }
    // The skew record describes what THIS load attempt observed; stale
    // observations from an earlier load of the slot do not carry over.
    mSlotSkew[slot] = 0;

    SlotFileData data;
    RsbsRefuseReason reason = RSBS_REFUSE_NONE;
    const SlotReadResult result = ReadSlotFile(slot, data, reason, /*verbose=*/true);

    if (result == SlotReadResult::Absent) {
        if (mSlotRefused[slot] != RSBS_REFUSE_NONE) {
            // Sticky refusal: this session already refused (and quarantined)
            // this slot's file. The now-empty slot path must NOT quietly
            // become writable — the refusal stands until the player
            // explicitly erases the slot or a load actually succeeds.
            std::fprintf(stderr, "[RsbsSave] slot %d still REFUSED this session (%s); writes stay latched\n",
                         slot, RefuseReasonLabel(mSlotRefused[slot]));
            return RSBS_LOAD_REFUSED;
        }
        // Opening an empty slot IS the create path: the session legitimately
        // established the slot and there is nothing on disk to destroy, so
        // the first write is armed.
        mSlotArmed[slot] = true;
        std::fprintf(stderr, "[RsbsSave] slot %d has no .redsave; armed for first write\n", slot);
        return RSBS_LOAD_ABSENT;
    }

    if (result == SlotReadResult::Refused) {
        // REFUSED, first-class (#533): quarantine the evidence aside, record
        // why, and latch the slot so no later autosave/capture can destroy
        // what is left. Every refusal path used to fall through to a state
        // indistinguishable from "no save at all".
        QuarantineSlotFile(slot, reason);
        mSlotRefused[slot] = reason;
        mSlotArmed[slot] = false;
        std::fprintf(stderr, "[RsbsSave] slot %d REFUSED (%s); slot latched against writes this session\n",
                     slot, RefuseReasonLabel(reason));
        return RSBS_LOAD_REFUSED;
    }

    // Structurally valid. Before committing, compare the two durable
    // artifacts' freshness stamps (#531/#564 V16 interim): the commit choke
    // point authors the same monotonic generation into the .redsave Tier-1
    // and (mirrored) into OoT's .sav JSON, so load is where a torn PAIR —
    // both files individually valid, describing different instants of the
    // ONE save — becomes detectable.
    ComboContext combo;
    std::memcpy(&combo, data.comboRecord.data(), sizeof(ComboContext));
    const int skew = CompareCommitGenerations(combo.commitGeneration, ootSavGeneration);
    if (skew < 0) {
        // OoT's .sav carries a NEWER commit generation than this .redsave: at
        // least one durable .redsave commit is missing (a write failed, or an
        // older copy was swapped in from cloud sync / a manual restore).
        // Committing the rolled-back Tier-1 would resurrect shared-item
        // records the newer commits already consumed (cross-game duplication)
        // and roll MM's ONLY persistence back — under the one-game ruling
        // that is corruption to refuse, not freshness to arbitrate. #533
        // machinery, full strength: quarantine the stale file as evidence,
        // latch the slot, surface the reason. Nothing was committed: live
        // state is untouched, exactly like every other refusal.
        std::fprintf(stderr,
                     "[RsbsSave] slot %d REFUSED: COMMIT SKEW — OoT's .sav mirrors commit generation %u "
                     "but the .redsave carries %u (a .redsave commit is missing). Loading it would roll "
                     "back the cross-game records and the MM world. Evidence quarantined; erase the slot "
                     "to release it.\n",
                     slot, ootSavGeneration, combo.commitGeneration);
        QuarantineSlotFile(slot, RSBS_REFUSE_COMMIT_SKEW);
        mSlotRefused[slot] = RSBS_REFUSE_COMMIT_SKEW;
        mSlotArmed[slot] = false;
        return RSBS_LOAD_REFUSED;
    }
    if (skew > 0) {
        // The .redsave is NEWER than OoT's .sav: an MM-side commit (owl save,
        // exit capture) landed after OoT's last save point. This is the
        // designed post-MM-commit state — an MM-side commit cannot rewrite
        // OoT's own file — so the load proceeds, but it is exactly the #531
        // loss shape: OoT's world resumes older than the cross-game records
        // that account for it (an item delivered to OoT after its last save
        // may be missing while marked REDEEMED). Recorded and surfaced in the
        // file panel via SlotMeta.commitSkew; the record-staging fix that
        // retires the loss itself is #531's residue.
        std::fprintf(stderr,
                     "[RsbsSave] slot %d COMMIT SKEW: .redsave generation %u is NEWER than OoT's .sav "
                     "generation %u — OoT's world is stale relative to the cross-game records (#531).\n",
                     slot, combo.commitGeneration, ootSavGeneration);
        mSlotSkew[slot] = 1;
    }

    // All checks passed — commit. gComboCtx and both shadows are updated.
    std::memcpy(&gComboCtx, &combo, sizeof(ComboContext));

    const std::vector<uint8_t>& ootBlob = data.ootBlob;
    const std::vector<uint8_t>& mmBlob = data.mmBlob;

    // The shared-resource watermarks (#525) are RAM-only and describe the
    // PREVIOUS session's live save; the pool we just loaded belongs to a
    // different one. Dropping them is what arms the first-harvest seed: an
    // occupied slot now seeds at the loaded game's live balance (delta zero)
    // instead of contributing money this pool already counted, which is the
    // difference between "rupees load correctly" and "rupees double on the
    // first switch after a load". See shared_resources.h.
    Combo_ResetSharedResourceWatermarks();
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
    // SaveContext over it.
    //
    // The return entrance is NOT a free placeholder. An earlier revision passed
    // 0 and called it inert; that was wrong the moment arming made this blob
    // reachable. rsbs/src/main.cpp's hot-swap path sets the arriving game's
    // startup entrance from Context_GetFrozenReturnEntrance, entrance presence
    // is tracked by a separate flag so 0 is a real consumable value, and
    // ENTR_SCENE_MAYORS_RESIDENCE is 0 — so F10 into a freshly loaded MM half
    // spawned Link inside the Mayor's Residence. Use the same safe arrival
    // entrance the real hot-swap freeze records (Switch_GetHotSwapReturnEntrance),
    // so the two armers agree. A slot-resume that wants MM's own owl /
    // new-cycle spawn policy overrides this later; this is the floor, not the
    // policy.
    const int armed = Context_ArmShadowAsFrozen(GAME_MM, MM_ENTR_SOUTH_CLOCK_TOWN_0);
    std::fprintf(stderr, "[RsbsSave] slot %d loaded; MM half %s\n", slot,
                 armed ? "armed for restore" : "empty (MM will cold-boot)");

    // A successful load is one of the three legitimate arming events, and it
    // retires any earlier refusal record — if a loadable file is back at the
    // slot path (say, the player restored the quarantined .bak by hand), the
    // slot is theirs again.
    mSlotArmed[slot] = true;
    mSlotRefused[slot] = RSBS_REFUSE_NONE;
    return RSBS_LOAD_OK;
}

bool SaveManager::Load(int slot) {
    return LoadSlot(slot) == RSBS_LOAD_OK;
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
    return DeserializeHeader(in, slot, header, /*verbose=*/false, nullptr);
}

void SaveManager::DeleteSave(int slot) {
    if (!SlotInRange(slot)) {
        return;
    }
    std::error_code ec;
    const std::filesystem::path path = SlotPath(slot);
    std::filesystem::remove(path, ec);
    std::filesystem::remove(path.string() + ".tmp", ec);
    // Quarantined evidence goes with the slot on an EXPLICIT erase — this is
    // the sanctioned disposal path (the .bak removal DeleteSave always
    // promised). Quarantine names carry a reason suffix and a dedupe counter,
    // so match by prefix instead of hard-coding one name.
    const std::string prefix = path.filename().string();
    std::filesystem::directory_iterator it(path.parent_path(), ec);
    if (!ec) {
        for (const auto& entry : it) {
            const std::string name = entry.path().filename().string();
            if (name.size() > prefix.size() && name.compare(0, prefix.size(), prefix) == 0 &&
                name.size() >= 4 && name.compare(name.size() - 4, 4, ".bak") == 0) {
                std::error_code rmEc;
                std::filesystem::remove(entry.path(), rmEc);
            }
        }
    }
    // Erasing the slot this session is an explicit player decision: the slot
    // is legitimately empty and writable, and any refusal record is retired
    // with the evidence.
    mSlotArmed[slot] = true;
    mSlotRefused[slot] = RSBS_REFUSE_NONE;
    mSlotSkew[slot] = 0;
}

void SaveManager::ArmSlotOnCreate(int slot) {
    if (!SlotInRange(slot)) {
        return;
    }
    // Creating a file over a slot whose .redsave fails validation must
    // preserve the evidence BEFORE the new file's first Save_SaveFile
    // rename-overwrites it. Full validation (CRC included) — this runs once
    // per file creation, not per frame.
    SlotFileData data;
    RsbsRefuseReason reason = RSBS_REFUSE_NONE;
    if (ReadSlotFile(slot, data, reason, /*verbose=*/false) == SlotReadResult::Refused) {
        std::fprintf(stderr, "[RsbsSave] slot %d create: existing .redsave fails validation (%s); quarantining\n",
                     slot, RefuseReasonLabel(reason));
        QuarantineSlotFile(slot, reason);
    }
    mSlotArmed[slot] = true;
    mSlotRefused[slot] = RSBS_REFUSE_NONE;
    mSlotSkew[slot] = 0;
}

bool SaveManager::IsSlotWritable(int slot) const {
    return SlotInRange(slot) && mSlotArmed[slot];
}

void SaveManager::RefuseSlotIdentity(int slot) {
    if (!SlotInRange(slot)) {
        // -1 is the legitimate "no active slot" value; a divergent session with
        // no slot has nothing durable to protect, and the caller's own log line
        // is the surface.
        return;
    }
    // Deliberately NO quarantine: the .redsave is healthy — it is the running
    // session (its live CVar/config state) that diverged from the creation
    // identity. The latch is what matters: without it, the divergent session's
    // next capture would freeze its un-paired world into the healthy pair's
    // Tier-3 under the pair's identity.
    mSlotArmed[slot] = false;
    mSlotRefused[slot] = RSBS_REFUSE_IDENTITY;
    std::fprintf(stderr,
                 "[RsbsSave] slot %d REFUSED (%s); slot latched against writes this session — the on-disk "
                 ".redsave is intact and untouched\n",
                 slot, RefuseReasonLabel(RSBS_REFUSE_IDENTITY));
}

void SaveManager::RefuseSlotGeneration(int slot) {
    if (!SlotInRange(slot)) {
        // Same -1 semantics as RefuseSlotIdentity: no slot, nothing durable to
        // protect, the caller's log line is the surface.
        return;
    }
    // Same NO-quarantine reasoning as RefuseSlotIdentity: the .redsave is
    // healthy — it is this session that could not AUTHOR the paired MM world
    // (attempt ladder exhausted, or generation threw; ADR 0010 increment 1.2).
    // The session falls back to an unpaired vanilla Termina, and the latch is
    // what keeps that world's captures out of the pair's .redsave.
    mSlotArmed[slot] = false;
    mSlotRefused[slot] = RSBS_REFUSE_GENERATION;
    std::fprintf(stderr,
                 "[RsbsSave] slot %d REFUSED (%s); slot latched against writes this session — the on-disk "
                 ".redsave is intact and untouched\n",
                 slot, RefuseReasonLabel(RSBS_REFUSE_GENERATION));
}

RsbsSlotState SaveManager::GetSlotState(int slot) const {
    if (!SlotInRange(slot)) {
        return RSBS_SLOT_ABSENT;
    }
    // The session's refusal record wins: after a quarantine the slot path is
    // empty, but the slot is REFUSED, not absent — that distinction is the
    // whole point (#533).
    if (mSlotRefused[slot] != RSBS_REFUSE_NONE) {
        return RSBS_SLOT_REFUSED;
    }
    std::ifstream in(SlotPath(slot), std::ios::binary);
    if (!in) {
        return RSBS_SLOT_ABSENT;
    }
    RsbsSaveHeader header;
    return DeserializeHeader(in, slot, header, /*verbose=*/false, nullptr) ? RSBS_SLOT_VALID
                                                                          : RSBS_SLOT_REFUSED;
}

RsbsRefuseReason SaveManager::GetSlotRefuseReason(int slot) const {
    if (!SlotInRange(slot)) {
        return RSBS_REFUSE_NONE;
    }
    if (mSlotRefused[slot] != RSBS_REFUSE_NONE) {
        return mSlotRefused[slot];
    }
    // No session record: probe the on-disk header so an in-place failing file
    // (not yet load-attempted) still names its reason in the panel.
    std::ifstream in(SlotPath(slot), std::ios::binary);
    if (!in) {
        return RSBS_REFUSE_NONE;
    }
    RsbsSaveHeader header;
    RsbsRefuseReason reason = RSBS_REFUSE_NONE;
    DeserializeHeader(in, slot, header, /*verbose=*/false, &reason);
    return reason;
}

bool SaveManager::HasQuarantine(int slot) const {
    if (!SlotInRange(slot)) {
        return false;
    }
    std::error_code ec;
    const std::filesystem::path path = SlotPath(slot);
    const std::string prefix = path.filename().string();
    std::filesystem::directory_iterator it(path.parent_path(), ec);
    if (ec) {
        return false;
    }
    for (const auto& entry : it) {
        const std::string name = entry.path().filename().string();
        if (name.size() > prefix.size() && name.compare(0, prefix.size(), prefix) == 0 &&
            name.size() >= 4 && name.compare(name.size() - 4, 4, ".bak") == 0) {
            return true;
        }
    }
    return false;
}

void SaveManager::ResetSlotSessionState() {
    for (int i = 0; i < RSBS_SAVE_MAX_SLOTS; i++) {
        mSlotArmed[i] = false;
        mSlotRefused[i] = RSBS_REFUSE_NONE;
        mSlotSkew[i] = 0;
    }
}

int SaveManager::GetSlotCommitSkew(int slot) const {
    return SlotInRange(slot) ? mSlotSkew[slot] : 0;
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
    meta.state = RSBS_SLOT_ABSENT;
    meta.refuseReason = RSBS_REFUSE_NONE;

    if (!SlotInRange(slot)) {
        return meta;
    }

    // Session refusal record + on-disk quarantine evidence. The record wins
    // over whatever is (or is not) at the slot path: a quarantined slot's
    // path is empty, but the slot is REFUSED, not "[empty]" (#533).
    meta.hasQuarantine = HasQuarantine(slot);
    meta.commitSkew = mSlotSkew[slot];
    if (mSlotRefused[slot] != RSBS_REFUSE_NONE) {
        meta.state = RSBS_SLOT_REFUSED;
        meta.refuseReason = mSlotRefused[slot];
    }

    std::ifstream in(SlotPath(slot), std::ios::binary);
    if (!in) {
        return meta;
    }
    meta.exists = true;

    RsbsSaveHeader header;
    RsbsRefuseReason headerReason = RSBS_REFUSE_NONE;
    if (!DeserializeHeader(in, slot, header, /*verbose=*/false, &headerReason)) {
        // exists=true, valid=false: a file is present but this build refuses
        // it. Surface it as REFUSED even before any load attempt.
        meta.state = RSBS_SLOT_REFUSED;
        if (meta.refuseReason == RSBS_REFUSE_NONE) {
            meta.refuseReason = headerReason;
        }
        return meta;
    }
    meta.valid = true;
    if (meta.state != RSBS_SLOT_REFUSED) {
        meta.state = RSBS_SLOT_VALID;
    }
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
        meta.state = RSBS_SLOT_REFUSED;
        meta.refuseReason = RSBS_REFUSE_TRUNCATED;
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
        meta.state = RSBS_SLOT_REFUSED;
        meta.refuseReason = RSBS_REFUSE_TRUNCATED;
        return meta;
    }
    std::vector<uint8_t> mmBlob(header.mmSize);
    in.read(reinterpret_cast<char*>(mmBlob.data()), header.mmSize);
    if (!in || in.gcount() != static_cast<std::streamsize>(header.mmSize)) {
        meta.valid = false;
        meta.state = RSBS_SLOT_REFUSED;
        meta.refuseReason = RSBS_REFUSE_TRUNCATED;
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

uint32_t RsbsSave_StageCommit(void) {
    return rsbs::SaveManager::Instance().StageCommit();
}

int RsbsSave_WriteStagedCommit(int slot) {
    return rsbs::SaveManager::Instance().WriteStagedCommit(slot) ? 1 : 0;
}

int RsbsSave_LoadSlotChecked(int slot, uint32_t ootSavGeneration) {
    return static_cast<int>(rsbs::SaveManager::Instance().LoadSlot(slot, ootSavGeneration));
}

int RsbsSave_GetSlotCommitSkew(int slot) {
    return rsbs::SaveManager::Instance().GetSlotCommitSkew(slot);
}

int RsbsSave_CompareCommitGenerations(uint32_t redsaveGeneration, uint32_t ootSavGeneration) {
    return rsbs::SaveManager::CompareCommitGenerations(redsaveGeneration, ootSavGeneration);
}

int RsbsSave_Load(int slot) {
    return rsbs::SaveManager::Instance().Load(slot) ? 1 : 0;
}

int RsbsSave_LoadSlot(int slot) {
    return static_cast<int>(rsbs::SaveManager::Instance().LoadSlot(slot));
}

void RsbsSave_ArmSlotOnCreate(int slot) {
    rsbs::SaveManager::Instance().ArmSlotOnCreate(slot);
}

int RsbsSave_IsSlotWritable(int slot) {
    return rsbs::SaveManager::Instance().IsSlotWritable(slot) ? 1 : 0;
}

void RsbsSave_RefuseSlotIdentity(int slot) {
    rsbs::SaveManager::Instance().RefuseSlotIdentity(slot);
}

void RsbsSave_RefuseSlotGeneration(int slot) {
    rsbs::SaveManager::Instance().RefuseSlotGeneration(slot);
}

int RsbsSave_GetSlotState(int slot) {
    return static_cast<int>(rsbs::SaveManager::Instance().GetSlotState(slot));
}

int RsbsSave_GetSlotRefuseReason(int slot) {
    return static_cast<int>(rsbs::SaveManager::Instance().GetSlotRefuseReason(slot));
}

int RsbsSave_HasQuarantine(int slot) {
    return rsbs::SaveManager::Instance().HasQuarantine(slot) ? 1 : 0;
}

void RsbsSave_ResetSlotSessionState(void) {
    rsbs::SaveManager::Instance().ResetSlotSessionState();
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
