/**
 * @file save.cpp
 * @brief Unified cross-game save file (.redsave) implementation (Phase 2 T6, #35).
 *
 * See save.h for the format. This file depends only on src/common (context +
 * game headers) and the standard library — never on either game's z64save.h.
 */

#include "save.h"

#include "combo_mm_options_view.h" // MM_Rando_RestoreProfileForLoad: the load-time MM profile compare (#781)
#include "combo_save_files_view.h"  // Combo_SaveFiles_RefuseText: the player's words for a refusal (#836)
#include "combo_settings_view.h"   // Combo_ComboSettingsRestoreLive: frozen wins at load (#781)
#include "context.h"
#include "crossing_store.h" // the v3 Tier-4 crossing block (ADR 0010 O7)
#include "entrance.h" // MM_ENTR_SOUTH_CLOCK_TOWN_0 — the armed blob's return entrance
// Combo_ComboSettingsDivergenceFor — the combo-level identity check the load
// runs over the record it just read (ADR 0011 decision 4).
#include "foreign_items.h"
#include "game.h"
#include "notification_bridge.h" // the load's player-visible surface (#781): stderr is not one
#include "shared_resources.h"
#include "triforce_hunt.h" // ADR 0010 O10: the triforce record joins the load-time identity check

#include <algorithm>
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

// Test hook (#781 leg 5): force the post-restore compare to report a
// divergence, so the rollback of a restore that did not take is exercised.
// Unreachable by construction in production (the resolver is a straight
// overlay of the keys the restore writes).
bool gForceLoadRestoreVerifyFail = false;

// The load's player-visible surface (#781). A load that changed the session's
// rules, or refused the pair, used to say so on stderr only, which no player
// reads. Every toast here is SoH's shape (docs/ui-style-guide.md section 10b,
// "Toasts"): default colours, the player's duration, a short prefix and one
// short message. The overlay draws both on ONE line and never wraps. The style
// guide's "about 53 characters" is the 832-px window's width in average glyphs;
// a 52-character toast measured 810 px there and ran off the left edge, so these
// keep to 48 (the UI snapshot's toast/load-* pages measure the pixels at that
// profile). The long explanation, with every field, stays on the stderr line
// beside each toast. Muted: the overlay's ding is OoT's audio, and the load also
// runs in the display-free rows.
constexpr std::size_t kLoadToastBudget = 48;

// The message budget left beside @p prefix and the space between them.
std::size_t LoadToastRoom(const char* prefix) {
    const std::size_t used = std::strlen(prefix) + 1;
    return used < kLoadToastBudget ? kLoadToastBudget - used : 0;
}

// "A, B, C" cut to at most @p budget characters: as many leading names as fit
// whole, then " +N" for the rest, where N counts from @p total (the true number
// of names, which may exceed the ones @p list carries; 0 means "the list is
// complete"). When not even the first name fits whole, it is cut with "..."
// (keeping at least eight of its characters); only when not even that fits is
// the toast a bare count, "1 <singular>" or "N <plural>".
std::string FitNames(const std::string& list, int total, std::size_t budget, const char* singular,
                     const char* plural) {
    std::vector<std::string> names;
    std::size_t start = 0;
    while (!list.empty() && start <= list.size()) {
        const std::size_t comma = list.find(", ", start);
        names.push_back(list.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 2;
    }
    const std::size_t count = std::max<std::size_t>(names.size(), total > 0 ? (std::size_t)total : 0u);
    if (count == 0) {
        return "";
    }
    auto plusFor = [count](std::size_t shown) {
        return shown < count ? " +" + std::to_string(count - shown) : std::string();
    };
    std::string out;
    std::size_t shown = 0;
    for (const std::string& name : names) {
        const std::string candidate = out.empty() ? name : out + ", " + name;
        if (candidate.size() + plusFor(shown + 1).size() > budget) {
            break;
        }
        out = candidate;
        shown++;
    }
    if (shown == 0 && !names.empty()) {
        const std::string plus = plusFor(1);
        constexpr std::size_t kMinKept = 8;
        if (budget >= plus.size() + 3 + kMinKept) {
            std::string cut = names[0].substr(0, budget - plus.size() - 3);
            // At a word boundary when that still keeps enough of the name.
            const std::size_t space = cut.rfind(' ');
            if (names[0].size() > cut.size() && names[0][cut.size()] != ' ' && space != std::string::npos &&
                space >= kMinKept) {
                cut.resize(space);
            }
            while (!cut.empty() && cut.back() == ' ') {
                cut.pop_back();
            }
            return cut + "..." + plus;
        }
        return std::to_string(count) + " " + (count == 1 ? singular : plural);
    }
    return out + plusFor(shown);
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
            // Used only for the DAMAGE half of the load-side combo identity
            // check (an unreadable record, or a fingerprint its own record does
            // not produce). A SESSION divergence — the arrival gate's
            // RefuseSlotIdentity, and the load path's field-only case — leaves
            // the healthy slot file in place and never reaches a rename.
            return "identity";
        case RSBS_REFUSE_GENERATION:
            // Same situation as RSBS_REFUSE_IDENTITY: a session refusal that
            // quarantines nothing, tagged totally for the same reason.
            return "generation";
        case RSBS_REFUSE_CROSSINGS:
            return "crossings";
        case RSBS_REFUSE_MISSING:
            // Never quarantined (there is nothing to set aside); named anyway so
            // every reason has a slug QuarantineReason can read back.
            return "missing";
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
            // Deliberately names the CLASS, not one producer's instance of it.
            // Two paths latch this reason now: #570's arrival gate (the live MM
            // options no longer resolve to the frozen profile) and #610's
            // spoiler-load gate (the loaded spoiler's cross-game section names
            // a different world). "Options differ from this pair's creation"
            // was true of the first and actively misleading about the second.
            // Each producer's own stderr line carries the specific term.
            return "this session diverges from this pair's creation identity";
        case RSBS_REFUSE_GENERATION:
            return "the paired Termina world could not be generated";
        case RSBS_REFUSE_CROSSINGS:
            return "cross-game placement record damaged";
        case RSBS_REFUSE_MISSING:
            return "a randomizer file without its paired cross-game record";
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
    //
    // WHOLE-FILE COMMIT (#589, operator ruling 2026-08-04). Reading BOTH
    // shadows here is not an implementation convenience, it is the ruling:
    // a durable save-and-quit in either half commits the WHOLE file — the
    // saving half live (its caller refreshed its own shadow from the live
    // gSaveContext immediately before staging, on this thread) and the other
    // half from its frozen shadow, which is that half's true state as of when
    // it was last live. One generation, one instant, both halves. That is what
    // makes the REDEEMED shared-item records in Tier-1 and the world they were
    // redeemed INTO inseparable, and it is why a partial stage is a refusal
    // rather than a best-effort write.
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

    // The crossing block, serialized HERE on the game thread for the same
    // reason Tier-1 is copied here: the write phase may run on the save worker
    // and must read only the immutable snapshot, never the live store.
    std::vector<uint8_t> crossings(Combo_Crossings_SerializedSize());
    if (Combo_Crossings_Serialize(crossings.data(), crossings.size()) != crossings.size()) {
        std::fprintf(stderr, "[RsbsSave] commit NOT staged: the crossing block did not serialize\n");
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
        mStaged.crossings.swap(crossings);
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
    std::vector<uint8_t> crossings;
    {
        std::lock_guard<std::mutex> lock(mStageMtx);
        if (!mStaged.valid) {
            return SaveLogFail(slot, "no commit has been staged this session (StageCommit not run)");
        }
        std::memcpy(&combo, &mStaged.combo, sizeof(ComboContext));
        ootBlob = mStaged.oot;
        mmBlob = mStaged.mm;
        crossings = mStaged.crossings;
    }
    return WriteSlotFile(slot, combo, ootBlob.data(), mmBlob.data(), crossings);
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
                                const uint8_t* mmBlob, const std::vector<uint8_t>& crossings) {
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
    // Tier-4 (v3, ADR 0010 O7): the crossing block, self-sized, always present
    // in a v3 file (16 bytes when the world has no crossings), inside the CRC.
    payload.insert(payload.end(), crossings.begin(), crossings.end());

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
    mSlotEpoch.fetch_add(1, std::memory_order_relaxed);
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
    // Tier-4 (v3+). Empty for a v1/v2 file, which carries no crossing block.
    std::vector<uint8_t> crossings;
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

    // Tier-4, the crossing block (v3+, ADR 0010 O7). Its size comes from its
    // OWN 16-byte header, which is validated before a byte of the body is read:
    // an oversized count is a refusal, never an allocation or a truncation.
    out.crossings.clear();
    if (header.version >= RSBS_SAVE_VERSION_CROSSINGS) {
        uint8_t blockHeader[RSBS_CROSSING_BLOCK_HEADER_SIZE];
        in.read(reinterpret_cast<char*>(blockHeader), sizeof(blockHeader));
        if (!in || in.gcount() != static_cast<std::streamsize>(sizeof(blockHeader))) {
            if (verbose) {
                std::fprintf(stderr, "[RsbsSave] slot %d truncated in Tier-4 (crossing block header)\n", slot);
            }
            outReason = RSBS_REFUSE_TRUNCATED;
            return SlotReadResult::Refused;
        }
        size_t blockSize = 0;
        const int sizeRc = Combo_Crossings_BlockSize(blockHeader, sizeof(blockHeader), &blockSize);
        if (sizeRc != RSBS_CROSSING_OK) {
            if (verbose) {
                std::fprintf(stderr, "[RsbsSave] slot %d Tier-4 crossing block header refused (%s)\n", slot,
                             Combo_Crossings_StatusName(sizeRc));
            }
            outReason = RSBS_REFUSE_CROSSINGS;
            return SlotReadResult::Refused;
        }
        out.crossings.assign(blockHeader, blockHeader + sizeof(blockHeader));
        out.crossings.resize(blockSize, 0);
        const size_t body = blockSize - sizeof(blockHeader);
        if (body > 0) {
            in.read(reinterpret_cast<char*>(out.crossings.data() + sizeof(blockHeader)),
                    static_cast<std::streamsize>(body));
            if (!in || in.gcount() != static_cast<std::streamsize>(body)) {
                if (verbose) {
                    std::fprintf(stderr, "[RsbsSave] slot %d truncated in Tier-4 (crossing records)\n", slot);
                }
                outReason = RSBS_REFUSE_TRUNCATED;
                return SlotReadResult::Refused;
            }
        }
    }

    // CRC over Tiers 1..3 (and Tier-4 from v3) exactly as stored (not the
    // zero-extended tails), in the same contiguous order they were written.
    std::vector<uint8_t> payload;
    payload.reserve(header.comboSize + header.ootSize + header.mmSize + out.crossings.size());
    payload.insert(payload.end(), out.comboRecord.begin(), out.comboRecord.begin() + header.comboSize);
    payload.insert(payload.end(), out.ootBlob.begin(), out.ootBlob.begin() + header.ootSize);
    payload.insert(payload.end(), out.mmBlob.begin(), out.mmBlob.begin() + header.mmSize);
    payload.insert(payload.end(), out.crossings.begin(), out.crossings.end());
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

    // Every row of the crossing block, validated BEFORE anything commits: a
    // CRC-clean block can still be one no writer of this build could have
    // produced (a host twice, an own-origin row), and committing it would hand
    // the give path a world that is not the one generated.
    if (header.version >= RSBS_SAVE_VERSION_CROSSINGS) {
        const int rowsRc = Combo_Crossings_ValidateBlock(out.crossings.data(), out.crossings.size());
        if (rowsRc != RSBS_CROSSING_OK) {
            if (verbose) {
                std::fprintf(stderr, "[RsbsSave] slot %d Tier-4 crossing block refused (%s), load refused\n", slot,
                             Combo_Crossings_StatusName(rowsRc));
            }
            outReason = RSBS_REFUSE_CROSSINGS;
            return SlotReadResult::Refused;
        }
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
    mSlotEpoch.fetch_add(1, std::memory_order_relaxed);
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

// What a load of the slot decides, before anything moves (#836). EvaluateSlot
// fills it; ProbeSlotForOpen stops there, and LoadSlotImpl carries out the
// decision. One evaluation for both is what makes "the probe accepted, the load
// refused" reachable only through a file that changed on disk in between, or a
// restore whose after-check failed.
struct SaveManager::SlotVerdict {
    RsbsLoadOutcome outcome = RSBS_LOAD_OK;
    RsbsRefuseReason reason = RSBS_REFUSE_NONE;
    // ABSENT, but this session already refused the slot: the refusal stands and
    // its record (reason and words) is left exactly as it is.
    bool sticky = false;
    // The load renames the refused file aside as evidence (the probe never does).
    bool quarantine = false;
    // RSBS_LOAD_TOAST_REFUSED_* for an identity refusal, -1 otherwise.
    int identityToast = -1;
    // The player's words for a refusal (the Combo > Save Files page and the
    // file-select toast say the same thing).
    std::string words;
    // RSBS_LOAD_OK only.
    ComboContext combo{};
    int skew = 0;
    uint32_t comboDiverged = 0; // a divergence the file restores; the load writes it back
    bool mmProfileChecked = false;
    int mmProfile = RSBS_MM_PROFILE_LOAD_MATCHES;
};

namespace {

// Which caller is evaluating (#836). LEGACY is LoadSlot / RsbsSave_LoadSlotChecked
// (the format-level locks); the two OPEN kinds are the file-open path (the
// file-select probe and OoT's OnLoadFile seam), which knows what kind of OoT
// file is being opened and refuses what that file cannot be opened without.
constexpr int kOpenLegacy = 0;
constexpr int kOpenVanillaFile = 1;
constexpr int kOpenRandoFile = 2;

// The open path's words for the refusals #836 adds (the one table,
// pairing_refusal_toast.c).
const char* WordsMissing() {
    return Combo_RefusalWords(RSBS_REFUSAL_WORDS_RECORD_MISSING);
}
const char* WordsNoMMWorld() {
    return Combo_RefusalWords(RSBS_REFUSAL_WORDS_NO_MM_WORLD);
}
const char* WordsMMOptions() {
    return Combo_RefusalWords(RSBS_REFUSAL_WORDS_MM_OPTIONS_DIFFER);
}

bool RecordIsPaired(const ComboContext& combo) {
    return combo.sourceIsRando && combo.sharedRandoSettingsHash != 0;
}

} // namespace

void SaveManager::EvaluateSlot(int slot, uint32_t ootSavGeneration, int openKind, SlotFileData& data,
                               SlotVerdict& v) const {
    v = SlotVerdict{};
    RsbsRefuseReason reason = RSBS_REFUSE_NONE;
    const SlotReadResult result = ReadSlotFile(slot, data, reason, /*verbose=*/true);

    if (result == SlotReadResult::Absent) {
        if (mSlotRefused[slot] != RSBS_REFUSE_NONE) {
            // Sticky refusal: this session already refused this slot. The
            // now-empty slot path must NOT quietly become writable: the refusal
            // stands until the player explicitly erases the slot or a load
            // actually succeeds.
            v.outcome = RSBS_LOAD_REFUSED;
            v.reason = mSlotRefused[slot];
            v.sticky = true;
            v.words = mSlotRefusedWords[slot][0] != '\0' ? std::string(mSlotRefusedWords[slot])
                                                         : std::string(Combo_SaveFiles_RefuseText(v.reason));
            return;
        }
        if (openKind == kOpenRandoFile) {
            // #836 P3 / #564 V12 gap 2: a randomizer .sav with no .redsave (a
            // lost or deleted record, one quarantined in an earlier session, or
            // SoH's Copy, which copies the .sav only). Opening it used to ARM the
            // slot, so the first base save wrote a .redsave from the dropped
            // session and the file was unpaired for good.
            v.outcome = RSBS_LOAD_REFUSED;
            v.reason = RSBS_REFUSE_MISSING;
            v.words = WordsMissing();
            return;
        }
        v.outcome = RSBS_LOAD_ABSENT;
        return;
    }

    if (result == SlotReadResult::Refused) {
        // REFUSED, first-class (#533). The load quarantines the evidence; the
        // probe leaves it where it is.
        v.outcome = RSBS_LOAD_REFUSED;
        v.reason = reason;
        v.quarantine = true;
        v.words = Combo_SaveFiles_RefuseText(reason);
        return;
    }

    // Structurally valid. Compare the two durable artifacts' freshness stamps
    // (#531/#564 V16): the commit choke point authors the same monotonic
    // generation into the .redsave's Tier-1 and (mirrored) into OoT's .sav, so
    // a torn PAIR becomes detectable here.
    std::memcpy(&v.combo, data.comboRecord.data(), sizeof(ComboContext));
    const ComboContext& combo = v.combo;
    v.skew = CompareCommitGenerations(combo.commitGeneration, ootSavGeneration);
    if (v.skew < 0) {
        // OoT's .sav carries a NEWER generation: at least one durable .redsave
        // commit is missing. Committing the rolled-back Tier-1 would resurrect
        // consumed shared-item records and roll MM's only persistence back:
        // corruption to refuse, not freshness to arbitrate.
        std::fprintf(stderr,
                     "[RsbsSave] slot %d REFUSED: COMMIT SKEW — OoT's .sav mirrors commit generation %u "
                     "but the .redsave carries %u (a .redsave commit is missing). Loading it would roll "
                     "back the cross-game records and the MM world.\n",
                     slot, ootSavGeneration, combo.commitGeneration);
        v.outcome = RSBS_LOAD_REFUSED;
        v.reason = RSBS_REFUSE_COMMIT_SKEW;
        v.quarantine = true;
        v.words = Combo_SaveFiles_RefuseText(v.reason);
        return;
    }

    if (openKind == kOpenRandoFile && !RecordIsPaired(combo)) {
        // #836 P3: a randomizer file whose record carries no pairing (the
        // permanent state the lost write latch used to leave behind). Every
        // randomizer file in this build is a paired file.
        std::fprintf(stderr,
                     "[RsbsSave] slot %d REFUSED: the randomizer file's cross-game record carries no pairing "
                     "identity (sourceIsRando=%d settingsHash=%08X); the file is not opened\n",
                     slot, combo.sourceIsRando ? 1 : 0, (unsigned)combo.sharedRandoSettingsHash);
        v.outcome = RSBS_LOAD_REFUSED;
        v.reason = RSBS_REFUSE_MISSING;
        v.words = WordsMissing();
        return;
    }

    // The COMBO-LEVEL IDENTITY check (ADR 0011 decision 4), over the record just
    // READ (a check that read gComboCtx would be checking the world this load is
    // about to replace). A legacy record (formatVersion 0) is exempt. Damage is
    // evidence and is quarantined by the load; a field-only divergence is a
    // healthy file met by a session that walked away from it: FROZEN WINS AT
    // LOAD (#781), so a divergence every bit of which a key authors is accepted
    // here and restored by the load; what no key can restore refuses.
    uint32_t comboDiverged =
        Combo_ComboSettingsDivergenceFor(&combo.comboSettings, combo.comboSettingsHash, combo.sharedRandoSettingsHash,
                                         combo.mmProfileDigest) |
        Combo_TriforceRecordDivergence(&combo.comboSettings, &combo.comboTriforce);
    if (comboDiverged != 0) {
        char fields[192];
        Combo_ComboSettingsDivergenceDescribe(comboDiverged, fields, sizeof(fields));
        if (Combo_ComboSettingsDivergenceIsDamage(comboDiverged)) {
            std::fprintf(stderr,
                         "[RsbsSave] slot %d REFUSED: COMBO SETTINGS IDENTITY — the stored cross-game identity is "
                         "damaged (%s).\n",
                         slot, fields);
            v.outcome = RSBS_LOAD_REFUSED;
            v.reason = RSBS_REFUSE_IDENTITY;
            v.quarantine = true;
            v.identityToast = RSBS_LOAD_TOAST_REFUSED_DAMAGED;
            v.words = RsbsSave_LoadToastRefusalMessage(v.identityToast);
            return;
        }
        if (Combo_ComboSettingsCanRestoreLive(&combo.comboSettings, comboDiverged) == 0) {
            std::fprintf(stderr,
                         "[RsbsSave] slot %d REFUSED: COMBO SETTINGS IDENTITY — the cross-game rules this file was "
                         "created under do not match this session's and cannot be restored from it: %s. The "
                         "on-disk .redsave is intact and untouched.\n",
                         slot, fields);
            v.outcome = RSBS_LOAD_REFUSED;
            v.reason = RSBS_REFUSE_IDENTITY;
            // A field no key authors (logicRung, an unallocated comboFlags bit,
            // spare1) is one only another build writes; a divergence of keyed
            // rules alone reaches here only when the store could not take them.
            v.identityToast = (comboDiverged & ~Combo_ComboSettingsRestorableMask()) != 0u
                                  ? RSBS_LOAD_TOAST_REFUSED_OTHER_BUILD
                                  : RSBS_LOAD_TOAST_REFUSED_RULES;
            v.words = RsbsSave_LoadToastRefusalMessage(v.identityToast);
            return;
        }
        v.comboDiverged = comboDiverged;
    }

    if (openKind != kOpenLegacy && RecordIsPaired(combo)) {
        // #836 P5: a paired record whose MM half is all zero (a pre-#680 file
        // that never crossed). The load cannot arm such a half, so the first
        // crossing used to be refused mid-play and Termina played vanilla.
        bool mmEmpty = true;
        for (uint8_t b : data.mmBlob) {
            if (b != 0) {
                mmEmpty = false;
                break;
            }
        }
        if (mmEmpty) {
            std::fprintf(stderr,
                         "[RsbsSave] slot %d REFUSED: the paired record's Majora's Mask half is empty; the file has "
                         "no paired Termina world and is not opened\n",
                         slot);
            v.outcome = RSBS_LOAD_REFUSED;
            v.reason = RSBS_REFUSE_GENERATION;
            v.words = WordsNoMMWorld();
            return;
        }

        // #836 PR 2: a half that is not THIS pair's world, judged against the
        // record just read (its master seed and recorded ladder rung), never
        // the live pairing this load is about to replace. A vanilla half (no
        // seed; #564 V7) used to be hydrated under the pairing, unlatched, so
        // Termina played vanilla and the half was committed back; another
        // pair's half (#564 V11) was refused only at the first crossing.
        const int half = MM_Rando_ClassifyHalfForPair(data.mmBlob.data(), data.mmBlob.size(), combo.sharedRandoSeed,
                                                      combo.mmPairedAttempt);
        if (half != RSBS_MM_HALF_PAIR_WORLD) {
            const bool vanilla = half == RSBS_MM_HALF_VANILLA;
            std::fprintf(stderr,
                         "[RsbsSave] slot %d REFUSED: the paired record's Majora's Mask half is %s (master seed %u, "
                         "recorded rung %u); the file has no Majora's Mask world of its own and is not opened\n",
                         slot, vanilla ? "vanilla (no world)" : "another pair's world", (unsigned)combo.sharedRandoSeed,
                         (unsigned)combo.mmPairedAttempt);
            v.outcome = RSBS_LOAD_REFUSED;
            v.reason = vanilla ? RSBS_REFUSE_GENERATION : RSBS_REFUSE_IDENTITY;
            v.words = WordsNoMMWorld();
            return;
        }
    }

    // The MM half of the same rule (#781): the profile digest the arrival gate
    // recomputes, classified here without writing anything. Only for a stamped
    // pair, and only with a CVar store to compare.
    if (combo.sourceIsRando && combo.sharedRandoSettingsHash != 0 && combo.mmProfileDigest != 0 &&
        Combo_ComboSettingStoreAvailable()) {
        v.mmProfileChecked = true;
        v.mmProfile = MM_Rando_ClassifyProfileForLoad(data.mmBlob.data(), data.mmBlob.size(), combo.mmProfileDigest);
        if (openKind != kOpenLegacy && v.mmProfile == RSBS_MM_PROFILE_LOAD_UNRESTORABLE) {
            // #836 P4: an MM profile the file cannot restore used to load with a
            // warning toast, and the first crossing was then refused.
            std::fprintf(stderr,
                         "[RsbsSave] slot %d REFUSED: the live MM profile does not match the file's (%08X) and the "
                         "file cannot restore it; the file is not opened\n",
                         slot, (unsigned)combo.mmProfileDigest);
            v.outcome = RSBS_LOAD_REFUSED;
            v.reason = RSBS_REFUSE_IDENTITY;
            v.words = WordsMMOptions();
            return;
        }
    }

    v.outcome = RSBS_LOAD_OK;
}

RsbsLoadOutcome SaveManager::ProbeSlotForOpen(int slot, uint32_t ootSavGeneration, bool isRandoFile,
                                              std::string* outWords) {
    if (outWords != nullptr) {
        outWords->clear();
    }
    if (!SlotInRange(slot)) {
        return RSBS_LOAD_REFUSED;
    }
    SlotFileData data;
    SlotVerdict v;
    EvaluateSlot(slot, ootSavGeneration, isRandoFile ? kOpenRandoFile : kOpenVanillaFile, data, v);
    if (v.outcome != RSBS_LOAD_REFUSED) {
        std::fprintf(stderr, "[RsbsSave] slot %d: file-select probe ACCEPTED (%s file, %s)\n", slot,
                     isRandoFile ? "randomizer" : "vanilla", v.outcome == RSBS_LOAD_OK ? "record valid" : "no record");
        return v.outcome;
    }
    // The session's refusal record only: no rename, no write, no key, no
    // gComboCtx, no active slot. The latch keeps any later write off the file.
    if (!v.sticky) {
        SetSlotRefused(slot, v.reason);
        NoteSlotRefusalWords(slot, v.words.c_str());
    }
    mSlotArmed[slot] = false;
    std::fprintf(stderr,
                 "[RsbsSave] slot %d: file-select probe REFUSED (%s): \"%s\"; the file is not opened, and nothing was "
                 "written, renamed or deleted\n",
                 slot, RefuseReasonLabel(v.reason), v.words.c_str());
    if (outWords != nullptr) {
        *outWords = v.words;
    }
    return RSBS_LOAD_REFUSED;
}

RsbsLoadOutcome SaveManager::LoadSlot(int slot, uint32_t ootSavGeneration) {
    return LoadSlotImpl(slot, ootSavGeneration, kOpenLegacy);
}

RsbsLoadOutcome SaveManager::LoadSlotForOpen(int slot, uint32_t ootSavGeneration, bool isRandoFile) {
    return LoadSlotImpl(slot, ootSavGeneration, isRandoFile ? kOpenRandoFile : kOpenVanillaFile);
}

RsbsLoadOutcome SaveManager::LoadSlotImpl(int slot, uint32_t ootSavGeneration, int openKind) {
    if (!SlotInRange(slot)) {
        return RSBS_LOAD_REFUSED;
    }
    // The skew record describes what THIS load attempt observed; stale
    // observations from an earlier load of the slot do not carry over. The
    // whole-file authority signal (#589) is dropped for the same reason, and
    // BEFORE the read, so every refusal leaves no authority claim behind.
    mSlotSkew[slot] = 0;
    mOoTHalfAuthoritySlot = -1;

    SlotFileData data;
    SlotVerdict v;
    EvaluateSlot(slot, ootSavGeneration, openKind, data, v);

    // A refusal the load carries out. @p restoreUndo, when given, puts back the
    // rules this load wrote before it refused, so a refused load changes nothing.
    auto refuse = [&](RsbsRefuseReason reason, bool quarantine, int identityToast, const std::string& words) {
        if (quarantine) {
            QuarantineSlotFile(slot, reason);
        }
        SetSlotRefused(slot, reason);
        if (openKind == kOpenLegacy) {
            // The legacy entry keeps its #781 surface: an identity refusal posts
            // its toast and the page repeats it.
            if (identityToast >= 0) {
                RsbsSave_EmitLoadToast(identityToast, nullptr, 0);
                NoteSlotRefusalWords(slot, RsbsSave_LoadToastRefusalMessage(identityToast));
            }
        } else {
            // The open path records the words; the backstop that returns the
            // player to the file select posts the one toast (#836).
            NoteSlotRefusalWords(slot, words.c_str());
        }
        mSlotArmed[slot] = false;
        std::fprintf(stderr, "[RsbsSave] slot %d REFUSED (%s); slot latched against writes this session\n", slot,
                     RefuseReasonLabel(reason));
        return RSBS_LOAD_REFUSED;
    };

    if (v.outcome == RSBS_LOAD_ABSENT) {
        // Opening an empty slot of a VANILLA file IS the create path: the
        // session legitimately established the slot and there is nothing on
        // disk to destroy, so the first write is armed.
        mSlotArmed[slot] = true;
        std::fprintf(stderr, "[RsbsSave] slot %d has no .redsave; armed for first write\n", slot);
        return RSBS_LOAD_ABSENT;
    }
    if (v.outcome == RSBS_LOAD_REFUSED) {
        if (v.sticky) {
            std::fprintf(stderr, "[RsbsSave] slot %d still REFUSED this session (%s); writes stay latched\n", slot,
                         RefuseReasonLabel(v.reason));
            mSlotArmed[slot] = false;
            return RSBS_LOAD_REFUSED;
        }
        return refuse(v.reason, v.quarantine, v.identityToast, v.words);
    }

    const ComboContext& combo = v.combo;
    if (v.skew > 0) {
        // The .redsave carries a whole commit OoT's own file does not (an
        // MM-side save, or OoT's exit-time snapshot). WHOLE-FILE COMMIT (#589):
        // its Tier-2 IS OoT's half at that commit; armed below and delivered by
        // OoT's load seam over the .sav the engine just applied.
        std::fprintf(stderr,
                     "[RsbsSave] slot %d WHOLE-FILE COMMIT: .redsave generation %u is NEWER than OoT's .sav "
                     "generation %u — the .redsave's OoT half is the authority for this load (#531/#589).\n",
                     slot, combo.commitGeneration, ootSavGeneration);
        mSlotSkew[slot] = 1;
    }

    // FROZEN WINS AT LOAD (#781): the file's rules go back into the keys, then
    // the same compare runs again. The after-check cannot fail while the
    // resolver is a straight overlay of these keys; when it does (forced by the
    // test hook), the keys go back exactly as the player left them and the load
    // refuses. On the open path that is the backstop's trigger (#836).
    char restoredRules[192] = { 0 };
    ComboSettingsKeyUndo rulesUndo;
    bool rulesWritten = false;
    if (v.comboDiverged != 0) {
        uint32_t afterRestore = v.comboDiverged;
        if (Combo_ComboSettingsRestoreLive(&combo.comboSettings, v.comboDiverged, restoredRules, sizeof(restoredRules),
                                           &rulesUndo) == 1) {
            rulesWritten = true;
            afterRestore = Combo_ComboSettingsDivergenceFor(&combo.comboSettings, combo.comboSettingsHash,
                                                            combo.sharedRandoSettingsHash, combo.mmProfileDigest) |
                           Combo_TriforceRecordDivergence(&combo.comboSettings, &combo.comboTriforce) |
                           (gForceLoadRestoreVerifyFail ? v.comboDiverged : 0u);
            std::fprintf(stderr,
                         "[RsbsSave] slot %d: the cross-game rules this session held differed from the file's; the "
                         "file's own values were restored (%s) — frozen wins at load (#781)\n",
                         slot, restoredRules);
        }
        if (afterRestore != 0) {
            if (rulesWritten) {
                Combo_ComboSettingsRestoreUndo(&rulesUndo);
            }
            char fields[192];
            Combo_ComboSettingsDivergenceDescribe(afterRestore, fields, sizeof(fields));
            std::fprintf(stderr,
                         "[RsbsSave] slot %d REFUSED: COMBO SETTINGS IDENTITY — the restore of the file's rules did "
                         "not take (%s); every key it wrote was put back. The on-disk .redsave is intact.\n",
                         slot, fields);
            const int toast = (afterRestore & ~Combo_ComboSettingsRestorableMask()) != 0u
                                  ? RSBS_LOAD_TOAST_REFUSED_OTHER_BUILD
                                  : RSBS_LOAD_TOAST_REFUSED_RULES;
            return refuse(RSBS_REFUSE_IDENTITY, false, toast, RsbsSave_LoadToastRefusalMessage(toast));
        }
    }

    // The MM half (#781): a divergence the file can answer is answered by
    // writing the file's own options and tricks back, so an arrival never
    // refuses a file that loaded.
    char restoredMm[256] = { 0 };
    int restoredMmCount = 0;
    int mmProfileOutcome = RSBS_MM_PROFILE_LOAD_MATCHES;
    if (v.mmProfileChecked && v.mmProfile != RSBS_MM_PROFILE_LOAD_MATCHES) {
        mmProfileOutcome = MM_Rando_RestoreProfileForLoad(data.mmBlob.data(), data.mmBlob.size(),
                                                          combo.mmProfileDigest, restoredMm, sizeof(restoredMm),
                                                          &restoredMmCount);
        if (openKind != kOpenLegacy && mmProfileOutcome != RSBS_MM_PROFILE_LOAD_RESTORED) {
            // The probe found the profile restorable and the restore did not
            // take (its own after-check put its keys back): the backstop's
            // trigger. The rules this load restored go back too.
            if (rulesWritten) {
                Combo_ComboSettingsRestoreUndo(&rulesUndo);
            }
            std::fprintf(stderr,
                         "[RsbsSave] slot %d REFUSED: the file's MM profile (%08X) could not be restored into the "
                         "live options; every key the load wrote was put back\n",
                         slot, (unsigned)combo.mmProfileDigest);
            return refuse(RSBS_REFUSE_IDENTITY, false, -1, WordsMMOptions());
        }
    }

    // All checks passed — commit. gComboCtx and both shadows are updated.
    std::memcpy(&gComboCtx, &combo, sizeof(ComboContext));

    // The crossing store travels with Tier-1 (ADR 0010 O7): the slot's own
    // record is authoritative, so it OVERWRITES. A v1/v2 file carries no block
    // and loads as "no crossings". Already validated by ReadSlotFile; checked
    // anyway because a silent miss here would keep the previous session's.
    const int crossingsRc = data.crossings.empty()
                                ? Combo_Crossings_LoadBlock(nullptr, 0)
                                : Combo_Crossings_LoadBlock(data.crossings.data(), data.crossings.size());
    if (crossingsRc != RSBS_CROSSING_OK) {
        Combo_Crossings_Clear();
        std::fprintf(stderr, "[RsbsSave] slot %d: crossing block failed to commit after validation (%s); cleared\n",
                     slot, Combo_Crossings_StatusName(crossingsRc));
    }

    const std::vector<uint8_t>& ootBlob = data.ootBlob;
    const std::vector<uint8_t>& mmBlob = data.mmBlob;

    // The shared-resource watermarks (#525) are RAM-only and describe the
    // PREVIOUS session's live save; dropping them arms the first-harvest seed
    // (see shared_resources.h).
    Combo_ResetSharedResourceWatermarks();
    Context_UpdateShadowCopy(GAME_OOT, ootBlob.data(), kOoTSize);
    Context_UpdateShadowCopy(GAME_MM, mmBlob.data(), kMMSize);

    // ARM the MM half so it is actually reachable (UpdateShadowCopy does not set
    // hasBeenFrozen, and every consumer gates on it). MM ALWAYS; OoT only when
    // the whole commit is newer than OoT's own file (#589). Arming refuses an
    // all-zero tier (Context_ArmShadowAsFrozen). The return entrance is the
    // same safe arrival entrance the real hot-swap freeze records, never 0
    // (ENTR_SCENE_MAYORS_RESIDENCE is 0).
    const int armed = Context_ArmShadowAsFrozen(GAME_MM, MM_ENTR_SOUTH_CLOCK_TOWN_0);
    std::fprintf(stderr, "[RsbsSave] slot %d loaded; MM half %s\n", slot,
                 armed ? "armed for restore" : "empty (MM will cold-boot)");

    // The OoT half of the newest whole commit (#589), with the OoT-side twin of
    // the MM arming's entrance floor (out of the Happy Mask Shop).
    if (mSlotSkew[slot] > 0) {
        if (Context_ArmShadowAsFrozen(GAME_OOT, OOT_ENTR_MARKET_FROM_MASK_SHOP)) {
            mOoTHalfAuthoritySlot = slot;
            std::fprintf(stderr,
                         "[RsbsSave] slot %d OoT half armed from the .redsave: the whole commit in the "
                         ".redsave is newer than OoT's own .sav, so its Tier-2 is the authority (#531/#589)\n",
                         slot);
        } else {
            std::fprintf(stderr,
                         "[RsbsSave] slot %d has a newer whole commit but an EMPTY OoT half; falling back to "
                         "OoT's own .sav for this load (#589)\n",
                         slot);
        }
    }

    // What the load did to the session's rules, where the player can see it
    // (#781). Persisted so the config file agrees with what the pages now show.
    if (restoredRules[0] != '\0' || mmProfileOutcome == RSBS_MM_PROFILE_LOAD_RESTORED) {
        Combo_ComboSettingsPersistStore();
    }
    if (restoredRules[0] != '\0') {
        RsbsSave_EmitLoadToast(RSBS_LOAD_TOAST_RULES_RESTORED, restoredRules, 0);
    }
    if (mmProfileOutcome == RSBS_MM_PROFILE_LOAD_RESTORED) {
        RsbsSave_EmitLoadToast(RSBS_LOAD_TOAST_MM_RESTORED, restoredMm, restoredMmCount);
    } else if (mmProfileOutcome == RSBS_MM_PROFILE_LOAD_UNRESTORABLE) {
        // The legacy entry only: the open path refused this file above.
        std::fprintf(stderr,
                     "[RsbsSave] slot %d: the live MM profile does not match the file's (%08X) and the file cannot "
                     "restore it; the next crossing into Majora's Mask will be refused until it does\n",
                     slot, (unsigned)combo.mmProfileDigest);
        RsbsSave_EmitLoadToast(RSBS_LOAD_TOAST_MM_NOT_RESTORED, nullptr, 0);
    }

    // A successful load is one of the three legitimate arming events, and it
    // retires any earlier refusal record.
    mSlotArmed[slot] = true;
    SetSlotRefused(slot, RSBS_REFUSE_NONE);
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
    SetSlotRefused(slot, RSBS_REFUSE_NONE);
    mSlotSkew[slot] = 0;
    if (mOoTHalfAuthoritySlot == slot) {
        mOoTHalfAuthoritySlot = -1;
    }
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
    SetSlotRefused(slot, RSBS_REFUSE_NONE);
    mSlotSkew[slot] = 0;
    if (mOoTHalfAuthoritySlot == slot) {
        // A brand-new file authors its own OoT half; nothing loaded can be
        // authoritative over it (#589).
        mOoTHalfAuthoritySlot = -1;
    }
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
    SetSlotRefused(slot, RSBS_REFUSE_IDENTITY);
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
    SetSlotRefused(slot, RSBS_REFUSE_GENERATION);
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

RsbsRefuseReason SaveManager::QuarantineReason(int slot) const {
    if (!SlotInRange(slot)) {
        return RSBS_REFUSE_NONE;
    }
    std::error_code ec;
    const std::filesystem::path path = SlotPath(slot);
    const std::string prefix = path.filename().string() + ".refused-";
    std::filesystem::directory_iterator it(path.parent_path(), ec);
    if (ec) {
        return RSBS_REFUSE_NONE;
    }
    // The newest evidence names the latest refusal: `<slot>.refused-<slug>.bak`
    // or `<slot>.refused-<slug>-<N>.bak` (QuarantineSlotFile's dedupe).
    RsbsRefuseReason newest = RSBS_REFUSE_NONE;
    std::filesystem::file_time_type newestTime{};
    bool any = false;
    for (const auto& entry : it) {
        const std::string name = entry.path().filename().string();
        if (name.size() <= prefix.size() + 4 || name.compare(0, prefix.size(), prefix) != 0 ||
            name.compare(name.size() - 4, 4, ".bak") != 0) {
            continue;
        }
        std::string slug = name.substr(prefix.size(), name.size() - prefix.size() - 4);
        const size_t dash = slug.find('-');
        if (dash != std::string::npos) {
            slug.resize(dash);
        }
        RsbsRefuseReason reason = RSBS_REFUSE_NONE;
        for (int r = RSBS_REFUSE_UNREADABLE; r <= RSBS_REFUSE_MISSING; r++) {
            if (slug == RefuseReasonSlug(static_cast<RsbsRefuseReason>(r))) {
                reason = static_cast<RsbsRefuseReason>(r);
                break;
            }
        }
        std::error_code timeEc;
        const std::filesystem::file_time_type when = entry.last_write_time(timeEc);
        if (!any || (!timeEc && when > newestTime)) {
            newest = reason;
            if (!timeEc) {
                newestTime = when;
            }
            any = true;
        }
    }
    return newest;
}

void SaveManager::SetSlotRefused(int slot, RsbsRefuseReason reason) {
    if (!SlotInRange(slot)) {
        return;
    }
    mSlotRefused[slot] = reason;
    mSlotRefusedWords[slot][0] = '\0';
    mSlotEpoch.fetch_add(1, std::memory_order_relaxed);
}

void SaveManager::NoteSlotRefusalWords(int slot, const char* words) {
    if (!SlotInRange(slot) || mSlotRefused[slot] == RSBS_REFUSE_NONE || words == nullptr) {
        return;
    }
    std::snprintf(mSlotRefusedWords[slot], sizeof(mSlotRefusedWords[slot]), "%s", words);
    mSlotEpoch.fetch_add(1, std::memory_order_relaxed);
}

const char* SaveManager::GetSlotRefusalWords(int slot) const {
    if (!SlotInRange(slot) || mSlotRefused[slot] == RSBS_REFUSE_NONE) {
        return "";
    }
    return mSlotRefusedWords[slot];
}

uint32_t SaveManager::SlotStateEpoch() const {
    return mSlotEpoch.load(std::memory_order_relaxed);
}

bool SaveManager::TryReadMetaAll(SlotMeta* out, int count) const {
    std::unique_lock<std::mutex> writeLock(mWriteMtx, std::try_to_lock);
    if (!writeLock.owns_lock()) {
        return false;
    }
    for (int slot = 0; out != nullptr && slot < count; slot++) {
        out[slot] = ReadMeta(slot);
    }
    return true;
}

void SaveManager::ResetSlotSessionState() {
    for (int i = 0; i < RSBS_SAVE_MAX_SLOTS; i++) {
        mSlotArmed[i] = false;
        SetSlotRefused(i, RSBS_REFUSE_NONE);
        mSlotSkew[i] = 0;
    }
    mOoTHalfAuthoritySlot = -1;
}

int SaveManager::GetSlotCommitSkew(int slot) const {
    return SlotInRange(slot) ? mSlotSkew[slot] : 0;
}

bool SaveManager::OoTHalfIsAuthoritative() const {
    return mOoTHalfAuthoritySlot >= 0;
}

bool SaveManager::TakeOoTHalfAuthority() {
    // One-shot (#589). The armed Tier-2 blob is single-use by the #364 frozen
    // -blob contract, so the signal that drives its consumption retires with
    // it: a second caller must not be told to re-apply a blob that has already
    // been consumed and retired.
    const bool authoritative = mOoTHalfAuthoritySlot >= 0;
    mOoTHalfAuthoritySlot = -1;
    return authoritative;
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
    if (desc.decodePlayerName != nullptr) {
        // The game's own charset (#773); the decoder bounds-checks its reads.
        desc.decodePlayerName(blob, blobSize, outName);
        outName[8] = '\0';
    } else if (nameLen > 0 && static_cast<size_t>(desc.playerNameOffset) + nameLen <= blobSize) {
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
    meta.quarantineReason = meta.hasQuarantine ? QuarantineReason(slot) : RSBS_REFUSE_NONE;
    meta.commitSkew = mSlotSkew[slot];
    if (mSlotRefused[slot] != RSBS_REFUSE_NONE) {
        meta.state = RSBS_SLOT_REFUSED;
        meta.refuseReason = mSlotRefused[slot];
        std::snprintf(meta.refuseWords, sizeof(meta.refuseWords), "%s", mSlotRefusedWords[slot]);
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
    // ReadMeta is a listing, not a load, and Load() still CRC-checks when the
    // player actually picks the slot. It still reads both game tiers whole (a
    // name decoder is handed the whole blob), so callers read on demand, never
    // per frame (combo_save_files_view.cpp caches on SlotStateEpoch).
    // Same zero-filled staging + prefix-copy as Load: a legacy short Tier-1
    // must still yield a readable sourceGame, or the Save Files page would
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

std::string SlotNameLine(const SlotMeta& meta) {
    if (meta.ootStarted && meta.mmStarted && meta.mmName[0] != '\0' && std::strcmp(meta.ootName, meta.mmName) != 0) {
        return std::string("OoT: ") + meta.ootName + "  MM: " + meta.mmName;
    }
    if (meta.ootStarted) {
        return std::string("OoT: ") + meta.ootName;
    }
    if (meta.mmStarted) {
        return std::string("MM: ") + meta.mmName;
    }
    return "(no per-game progress)";
}

// ============================================================================
// In-session reset from the last whole commit (#785; see save.h)
// ============================================================================

RsbsResetOutcome SaveManager::RestoreLastCommitForReset(int slot, uint8_t* mmOut, size_t mmOutSize,
                                                        RsbsResetAcceptHalf accept, void* acceptCtx,
                                                        bool* outOoTHalfMoved) {
    if (outOoTHalfMoved != nullptr) {
        *outOoTHalfMoved = false;
    }
    if (!SlotInRange(slot)) {
        return RSBS_RESET_NO_SLOT;
    }
    // A latched slot is a file this session does not own: refused for identity
    // or generation at the arrival, refused at load, or never established. Its
    // bytes are exactly what the latch exists to keep out of this session.
    if (!IsSlotWritable(slot)) {
        return RSBS_RESET_NOT_WRITABLE;
    }
    if (mmOut == nullptr || mmOutSize == 0 || mmOutSize > kMMSize) {
        return RSBS_RESET_REJECTED;
    }

    SlotFileData data;
    RsbsRefuseReason reason = RSBS_REFUSE_NONE;
    const SlotReadResult result = ReadSlotFile(slot, data, reason, /*verbose=*/true);
    if (result == SlotReadResult::Absent) {
        return RSBS_RESET_NO_COMMIT;
    }
    if (result == SlotReadResult::Refused) {
        // Named, never quarantined: the reset reads, it does not load. The next
        // LoadSlot of this slot runs the refusal machinery at full strength.
        std::fprintf(stderr, "[RsbsSave] slot %d: last commit unreadable for a reset (%s); nothing restored\n", slot,
                     RefuseReasonLabel(reason));
        return RSBS_RESET_UNREADABLE;
    }

    ComboContext combo;
    std::memcpy(&combo, data.comboRecord.data(), sizeof(ComboContext));

    // The file must BE the session's last commit. Every commit this session
    // made stamped gComboCtx.commitGeneration and wrote that record, and a load
    // copies the file's generation in, so equality is the ordinary state. A
    // mismatch means a staged commit never reached disk (or the file changed
    // underneath the session); restoring an older record would resurrect
    // shared-item records a newer, lost commit had consumed.
    if (combo.commitGeneration != gComboCtx.commitGeneration) {
        std::fprintf(stderr,
                     "[RsbsSave] slot %d: the .redsave carries commit generation %u but this session's last commit "
                     "is %u; nothing restored\n",
                     slot, (unsigned)combo.commitGeneration, (unsigned)gComboCtx.commitGeneration);
        return RSBS_RESET_STALE;
    }

    // One game, one identity (ADR 0011 decision 4): a reset never changes which
    // world is being played. Every field frozen at creation must already agree.
    const bool sameIdentity = combo.sharedRandoSeed == gComboCtx.sharedRandoSeed &&
                              combo.sourceIsRando == gComboCtx.sourceIsRando &&
                              combo.sharedRandoSettingsHash == gComboCtx.sharedRandoSettingsHash &&
                              combo.mmProfileDigest == gComboCtx.mmProfileDigest &&
                              combo.mmPairedAttempt == gComboCtx.mmPairedAttempt &&
                              combo.comboSettingsHash == gComboCtx.comboSettingsHash &&
                              std::memcmp(&combo.comboTriforce, &gComboCtx.comboTriforce,
                                          sizeof(ComboTriforceRecord)) == 0;
    // The crossing store is the only truth for foreign placements, frozen at
    // creation; the commit's block must be the live store byte for byte (a
    // v1/v2 file carries none, which is true only of a world with none).
    bool sameCrossings = false;
    if (data.crossings.empty()) {
        sameCrossings = Combo_Crossings_Count(GAME_OOT) == 0 && Combo_Crossings_Count(GAME_MM) == 0;
    } else {
        std::vector<uint8_t> live(Combo_Crossings_SerializedSize());
        sameCrossings = Combo_Crossings_Serialize(live.data(), live.size()) == live.size() && live == data.crossings;
    }
    if (!sameIdentity || !sameCrossings) {
        std::fprintf(stderr,
                     "[RsbsSave] slot %d: the last commit names a different world than the live one (%s); nothing "
                     "restored\n",
                     slot, sameIdentity ? "crossing set differs" : "identity differs");
        return RSBS_RESET_OTHER_WORLD;
    }

    bool mmEmpty = true;
    for (uint8_t b : data.mmBlob) {
        if (b != 0) {
            mmEmpty = false;
            break;
        }
    }
    if (mmEmpty || (accept != nullptr && accept(data.mmBlob.data(), data.mmBlob.size(), acceptCtx) == 0)) {
        std::fprintf(stderr, "[RsbsSave] slot %d: the last commit's MM half %s; nothing restored\n", slot,
                     mmEmpty ? "is empty" : "was refused by the caller");
        return RSBS_RESET_REJECTED;
    }

    // ---- every check passed: restore, all tiers together --------------------
    ComboContext merged;
    std::memcpy(&merged, &combo, sizeof(ComboContext));
    merged.switchRequested = gComboCtx.switchRequested;
    merged.targetGame = gComboCtx.targetGame;
    merged.targetEntrance = gComboCtx.targetEntrance;
    merged.sourceGame = gComboCtx.sourceGame;
    merged.sourceEntrance = gComboCtx.sourceEntrance;
    merged.saveSlot = gComboCtx.saveSlot;
    merged.commitGeneration = gComboCtx.commitGeneration;
    std::memcpy(&gComboCtx, &merged, sizeof(ComboContext));

    // The pool is the commit's now; a watermark taken against the pre-reset
    // half would re-count (or drain) it at the next harvest. Same reset a load
    // performs; the caller re-seeds from the restored half.
    Combo_ResetSharedResourceWatermarks();

    const void* ootLive = Context_GetOoTSaveContext();
    const bool ootMoved = ootLive == nullptr || std::memcmp(ootLive, data.ootBlob.data(), kOoTSize) != 0;
    Context_UpdateShadowCopy(GAME_OOT, data.ootBlob.data(), kOoTSize);
    if (outOoTHalfMoved != nullptr) {
        *outOoTHalfMoved = ootMoved;
    }

    std::memcpy(mmOut, data.mmBlob.data(), mmOutSize);
    std::fprintf(stderr,
                 "[RsbsSave] slot %d: restored the last whole commit (generation %u) in place; OoT half %s\n", slot,
                 (unsigned)combo.commitGeneration,
                 ootMoved ? "ROLLED BACK too (OoT progress after that commit was never committed)"
                          : "unchanged (already that commit's)");
    return RSBS_RESET_RESTORED;
}

}  // namespace rsbs

// ============================================================================
// C shim
// ============================================================================

extern "C" {

int RsbsSave_RestoreLastCommitForReset(int slot, uint8_t* mmOut, size_t mmOutSize, RsbsResetAcceptHalf accept,
                                       void* acceptCtx, int* outOoTHalfMoved) {
    bool moved = false;
    const RsbsResetOutcome outcome =
        rsbs::SaveManager::Instance().RestoreLastCommitForReset(slot, mmOut, mmOutSize, accept, acceptCtx, &moved);
    if (outOoTHalfMoved != nullptr) {
        *outOoTHalfMoved = moved ? 1 : 0;
    }
    return static_cast<int>(outcome);
}

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

int RsbsSave_ProbeSlotForOpen(int slot, uint32_t ootSavGeneration, int isRandoFile, char* words, size_t wordsLen) {
    std::string out;
    const RsbsLoadOutcome outcome =
        rsbs::SaveManager::Instance().ProbeSlotForOpen(slot, ootSavGeneration, isRandoFile != 0, &out);
    if (words != nullptr && wordsLen > 0) {
        std::snprintf(words, wordsLen, "%s", out.c_str());
    }
    return static_cast<int>(outcome);
}

int RsbsSave_LoadSlotForOpen(int slot, uint32_t ootSavGeneration, int isRandoFile) {
    return static_cast<int>(rsbs::SaveManager::Instance().LoadSlotForOpen(slot, ootSavGeneration, isRandoFile != 0));
}

const char* RsbsSave_SlotRefusalWords(int slot) {
    const rsbs::SaveManager& mgr = rsbs::SaveManager::Instance();
    const char* words = mgr.GetSlotRefusalWords(slot);
    if (words[0] != '\0') {
        return words;
    }
    const RsbsRefuseReason reason = mgr.GetSlotRefuseReason(slot);
    return reason == RSBS_REFUSE_NONE ? "" : Combo_SaveFiles_RefuseText(reason);
}

void RsbsSave_EmitFileSelectRefusalToast(const char* words) {
    // The refusal class's one prefix and the page's words (#836), muted like
    // every load toast (the gate also runs in the display-free rows).
    if (words == nullptr || words[0] == '\0') {
        return;
    }
    OoT_Notification_EmitDefault(RSBS_REFUSAL_TOAST_PREFIX, words, /*mute=*/1);
}

int RsbsSave_GetSlotCommitSkew(int slot) {
    return rsbs::SaveManager::Instance().GetSlotCommitSkew(slot);
}

int RsbsSave_CompareCommitGenerations(uint32_t redsaveGeneration, uint32_t ootSavGeneration) {
    return rsbs::SaveManager::CompareCommitGenerations(redsaveGeneration, ootSavGeneration);
}

int RsbsSave_TakeOoTHalfAuthority(void) {
    return rsbs::SaveManager::Instance().TakeOoTHalfAuthority() ? 1 : 0;
}

int RsbsSave_OoTHalfIsAuthoritative(void) {
    return rsbs::SaveManager::Instance().OoTHalfIsAuthoritative() ? 1 : 0;
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

void RsbsSave_ForceLoadRestoreVerifyFailForTest(int on) {
    rsbs::gForceLoadRestoreVerifyFail = on != 0;
}

const char* RsbsSave_LoadToastRefusalMessage(int kind) {
    switch (kind) {
        case RSBS_LOAD_TOAST_REFUSED_RULES:
            return Combo_RefusalWords(RSBS_REFUSAL_WORDS_RULES_DIFFER);
        case RSBS_LOAD_TOAST_REFUSED_OTHER_BUILD:
            return Combo_RefusalWords(RSBS_REFUSAL_WORDS_OTHER_BUILD);
        case RSBS_LOAD_TOAST_REFUSED_DAMAGED:
            return Combo_RefusalWords(RSBS_REFUSAL_WORDS_RECORD_DAMAGED);
        default:
            return nullptr;
    }
}

void RsbsSave_NoteSlotRefusalWords(int slot, const char* words) {
    rsbs::SaveManager::Instance().NoteSlotRefusalWords(slot, words);
}

void RsbsSave_EmitLoadToast(int kind, const char* names, int count) {
    // The copy, in one place: the load and the MM arrival post through here, and
    // so do the UI snapshot's toast/load-* pages.
    const char* prefix = nullptr;
    std::string message;
    switch (kind) {
        case RSBS_LOAD_TOAST_RULES_RESTORED:
            prefix = "Restored from file:";
            message = rsbs::FitNames(names != nullptr ? names : "", count, rsbs::LoadToastRoom(prefix), "rule", "rules");
            break;
        case RSBS_LOAD_TOAST_MM_RESTORED:
            prefix = "Restored for Majora's Mask:";
            message = rsbs::FitNames(names != nullptr ? names : "", count, rsbs::LoadToastRoom(prefix), "setting",
                                     "settings");
            break;
        case RSBS_LOAD_TOAST_MM_NOT_RESTORED:
            // The legacy entry's warning; the open path refuses this file at the
            // file select in the same words (#836).
            prefix = RSBS_REFUSAL_TOAST_PREFIX;
            message = Combo_RefusalWords(RSBS_REFUSAL_WORDS_MM_OPTIONS_DIFFER);
            break;
        case RSBS_LOAD_TOAST_REFUSED_RULES:
        case RSBS_LOAD_TOAST_REFUSED_OTHER_BUILD:
        case RSBS_LOAD_TOAST_REFUSED_DAMAGED:
            prefix = RSBS_REFUSAL_TOAST_PREFIX;
            message = RsbsSave_LoadToastRefusalMessage(kind);
            break;
        default:
            return;
    }
    if (message.empty()) {
        return;
    }
    OoT_Notification_EmitDefault(prefix, message.c_str(), /*mute=*/1);
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

void RsbsSave_DecodeN64FilenameName(const uint8_t name[8], char outName[9]) {
    int len = 0;
    for (int i = 0; i < 8; i++) {
        const uint8_t c = name[i];
        char out = '?';
        if (c <= 0x09) {
            out = static_cast<char>('0' + c);
        } else if (c <= 0x23) {
            out = static_cast<char>('A' + (c - 0x0A));
        } else if (c <= 0x3D) {
            out = static_cast<char>('a' + (c - 0x24));
        } else if (c == 0x3E) {
            out = ' ';
        } else if (c == 0x3F) {
            out = '-';
        } else if (c == 0x40) {
            out = '.';
        }
        outName[i] = out;
        if (out != ' ') {
            len = i + 1;
        }
    }
    outName[len] = '\0';
}

}  // extern "C"
