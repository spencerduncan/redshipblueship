/**
 * @file test_crossgame_model.c
 * @brief Cross-game model resolution lock (#577).
 *
 * The claim under test: with only OoT brought up, an MM-EXCLUSIVE model carried
 * by the curated cross-game archive is fully drawable — the display list parses,
 * and every resource its command stream references resolves, out of a non-OoT
 * archive, with no reference left that would be resolved against the HOST
 * game's segment table.
 *
 * That last clause is the whole point of the row. The standing objection to
 * cross-game rendering was never really the 151-name object-namespace overlap
 * (models resolve by resource PATH here, not by object slot — see
 * gfx_dl_otr_filepath_handler_custom in libultraship/src/fast/interpreter.cpp).
 * It was segment-0x04 divergence: a ROM-extracted display list that reaches
 * into the shared "keep" segment by SEGMENT ADDRESS samples whatever the host
 * game has bound there, which is why OoTMM needed ~107 hand-authored
 * kObjectPatches[] entries. In our exported format that hazard has an exact,
 * mechanical signature — OTRExporter emits a texture reference either as
 * G_SETTIMG_OTR_HASH carrying a resource path (when the source segment was
 * registered at export time) or as a RAW gsDPSetTextureImage carrying the
 * segmented address verbatim (when it was not); see the G_SETTIMG branch of
 * OTRExporter/OTRExporter/DisplayListExporter.cpp. A raw one in a foreign
 * model IS the divergence. This walks the parsed stream and asserts there are
 * none, plus that the model itself comes from the CURATED archive and every
 * hash reference it makes actually resolves out of a non-OoT one.
 *
 * The walk follows gsSPDisplayList / gsSPBranchLessZ edges into sub-display-
 * lists, because the commands on the far side of one execute under the same
 * host game: a raw segmented G_SETTIMG one level down is exactly the same
 * hazard, and a top-level-only scan would report a clean model that is not.
 * (object_mask_truth, the shipped default, happens to be a single flat stream —
 * but the row is deliberately re-pointable with RSBS_CROSSGAME_MODEL_PATH, and
 * most objects are not that flat.) Cycles and runaway nesting are bounded; if
 * the depth cap is hit the row FAILS rather than reporting a clean model it
 * did not finish checking.
 *
 * Falsifiability, and the counterfactuals that were actually run:
 *   - Remove redship.o2r from the build root: the wrapper SKIPs rather than
 *     letting an unstaged tree read as a pass, and with the mount removed but
 *     the archive present the model resolves to nothing at all.
 *   - RSBS_CROSSGAME_MODEL_PATH pointed at a curated object whose stream DOES
 *     carry raw segmented texture references: the raw-SETTIMG assertion fires.
 *     No rebuild needed — add the object to assets/crossgame/manifest.txt,
 *     re-run GenerateRedshipOtr, set the variable. That is what keeps the
 *     assertion from being decorative.
 *
 * Non-vacuity, three ways: the row fails if the display list turns out to
 * reference zero textures or zero vertices (a model that references nothing
 * cannot garble, so it also cannot prove anything); it fails if the model was
 * served by anything other than the curated archive the caller just mounted;
 * and the wrapper SKIPs, rather than passing, when no curated archive exists.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++): it drives the
 * C++-linkage Ship::Context / ResourceManager / ArchiveManager and Fast::
 * DisplayList APIs. The caller (Test_CrossGameModel) does the display-free
 * bring-up, the redship.o2r staging check and mount, and the factory
 * registration first.
 */

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <fast/resource/type/DisplayList.h>
#include <libultraship/libultra/gbi.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

// The curated MM-exclusive model. Collision-free against every OoT archive:
// all 24 `object_mask_*` directories are MM-only.
constexpr const char* kCgmDefaultDisplayList = "objects/object_mask_truth/object_mask_truth_DL_0001A0";

// Counterfactual knob. The shipped lock is the default above; pointing this at
// a display list that DOES carry raw segmented texture references is how the
// raw-SETTIMG assertion below is shown to be live rather than decorative, and
// it needs no rebuild — add the object to assets/crossgame/manifest.txt,
// re-run GenerateRedshipOtr, and set the variable.
const char* CgmDisplayListPath() {
    const char* env = std::getenv("RSBS_CROSSGAME_MODEL_PATH");
    return (env != nullptr && env[0] != '\0') ? env : kCgmDefaultDisplayList;
}

bool CgmArchiveIsNamed(const std::string& archivePath, const char* name) {
    const size_t len = std::string(name).size();
    return archivePath.size() >= len && archivePath.compare(archivePath.size() - len, len, name) == 0;
}

// Archives whose bytes would mean the reference resolved to the WRONG game.
// soh.o2r is mounted by the OoT bring-up itself, so this is a live check, not a
// theoretical one.
bool CgmIsOoTArchive(const std::string& archivePath) {
    static const char* kOoTArchives[] = { "oot.o2r", "oot-mq.o2r", "soh.o2r" };
    for (const char* name : kOoTArchives) {
        if (CgmArchiveIsNamed(archivePath, name)) {
            return true;
        }
    }
    return false;
}

// The curated cross-game archive itself. The model must come from HERE, not
// merely from "some archive that is not OoT's".
//
// This matters because the row also executes inside `--test all`, which runs
// the whole dispatch table in ONE process — by the time it runs there, boot-mm
// has already mounted mm.o2r, which carries object_mask_truth too. A
// not-OoT-archive test would then pass on mm.o2r's copy and prove nothing about
// the curated archive. Requiring the curated archive is deterministic in both
// settings: the caller mounts it immediately beforehand and ArchiveManager is
// last-added-wins, so it owns the path either way.
bool CgmIsCuratedArchive(const std::string& archivePath) {
    return CgmArchiveIsNamed(archivePath, "redship.o2r");
}

// LUS custom opcodes (libultraship/include/fast/lus_gbi.h). Spelled out here
// rather than included: lus_gbi.h pulls the F3DEX ucode headers into whatever
// TU includes it, and this row only needs the opcode numbers. The expanded set
// below must stay in step with ResourceFactoryBinaryDisplayListV0::ReadResource
// — those are exactly the commands the factory stores as TWO Gfx entries, and
// mis-listing one desynchronizes this walk from the real stream.
constexpr uint8_t kCgmSetTimgOtrHash = 0x20;
constexpr uint8_t kCgmDlOtrHash = 0x31;
constexpr uint8_t kCgmVtxOtrHash = 0x32;
constexpr uint8_t kCgmMarker = 0x33;
constexpr uint8_t kCgmMtxOtr = 0x36;
constexpr uint8_t kCgmBranchZOtr = 0x35;
constexpr uint8_t kCgmMovememOtr = 0x42;
// Raw RDP/RSP commands whose operand is a segmented address the interpreter
// resolves against the HOST game's segment table.
constexpr uint8_t kCgmRawSetTimg = 0xFD;

bool CgmIsExpanded(uint8_t opcode) {
    return opcode == kCgmSetTimgOtrHash || opcode == kCgmDlOtrHash || opcode == kCgmVtxOtrHash ||
           opcode == kCgmMarker || opcode == kCgmMtxOtr || opcode == kCgmBranchZOtr || opcode == kCgmMovememOtr;
}

// Tallies accumulated across the whole model — the top-level stream AND every
// sub-display-list it branches into.
struct CgmStats {
    int textureRefs = 0;
    int vertexRefs = 0;
    int subDlRefs = 0;
    int rawSegmentedTextures = 0;
    int unresolvedRefs = 0;
    int ootServedRefs = 0;
    int streamsWalked = 0;
    std::vector<std::string> resolved;
};

// A gsSPDisplayList to another resource is a real edge of the model, and the
// commands on the far side are executed under the HOST game exactly like the
// ones on this side — so a raw segmented G_SETTIMG hiding one level down is the
// same hazard, and stopping at the top level would let it through silently.
// object_mask_truth happens to have no sub-DLs, but the row is pointed at other
// objects via RSBS_CROSSGAME_MODEL_PATH and most models are not that flat.
//
// Depth is capped and visited paths are skipped: an OTR-hash edge is just a
// CRC64, so nothing in the format prevents a cycle, and a test must not hang.
constexpr int kCgmMaxDepth = 8;

void CgmWalkStream(Ship::ResourceManager* rm, Ship::ArchiveManager* am, const std::string& streamPath,
                   const Fast::DisplayList& dl, int depth, std::vector<std::string>& visited, CgmStats& stats);

// Load a referenced path as a display list, if it is one. Returns nullptr when
// the resource is not a DisplayList (a texture, vertex array, matrix, ...),
// which is not an error — only DL edges are walked.
std::shared_ptr<Fast::DisplayList> CgmLoadSubDisplayList(Ship::ResourceManager* rm, const std::string& path) {
    auto resource = rm->LoadResourceProcess(path, /*loadExact*/ true, nullptr);
    if (resource == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<Fast::DisplayList>(resource);
}

void CgmWalkStream(Ship::ResourceManager* rm, Ship::ArchiveManager* am, const std::string& streamPath,
                   const Fast::DisplayList& dl, int depth, std::vector<std::string>& visited, CgmStats& stats) {
    stats.streamsWalked++;
    const auto& instructions = dl.Instructions;

    for (size_t i = 0; i < instructions.size(); i++) {
        const uint8_t opcode = (uint8_t)(instructions[i].words.w0 >> 24);

        if (opcode == kCgmRawSetTimg) {
            // The exporter only emits this form when it could not turn the
            // texture into a resource path — i.e. the operand is still a
            // segmented address. Under a foreign host that samples the host's
            // segment table. Segment 0 is a plain physical address and is not
            // the hazard.
            const uint32_t operand = (uint32_t)instructions[i].words.w1;
            if (operand != 0 && ((operand >> 24) & 0x0F) != 0) {
                stats.rawSegmentedTextures++;
                fprintf(stderr, "[crossgame-model] raw segmented texture in %s at instruction %zu: segment 0x%02X\n",
                        streamPath.c_str(), i, (unsigned)((operand >> 24) & 0x0F));
            }
            continue;
        }

        if (!CgmIsExpanded(opcode)) {
            continue;
        }

        // Expanded (128-bit) command: the payload is the next Gfx.
        if (i + 1 >= instructions.size()) {
            stats.unresolvedRefs++;
            fprintf(stderr, "[crossgame-model] truncated expanded command 0x%02X at the end of %s\n", (unsigned)opcode,
                    streamPath.c_str());
            break;
        }
        const Gfx payload = instructions[++i];
        if (opcode == kCgmMarker) {
            continue; // Debug marker, no resource behind it.
        }

        const uint64_t hash = ((uint64_t)(uint32_t)payload.words.w0 << 32) | (uint32_t)payload.words.w1;
        const std::string* path = am->HashToString(hash);
        if (path == nullptr) {
            stats.unresolvedRefs++;
            fprintf(stderr, "[crossgame-model] unresolved reference in %s: opcode 0x%02X hash 0x%016llX\n",
                    streamPath.c_str(), (unsigned)opcode, (unsigned long long)hash);
            continue;
        }

        // This is the interpreter's own resolution: the OTR handlers take the
        // reference out of the command word and go straight to the resource
        // manager (libultraship/src/fast/interpreter.cpp).
        const void* raw = rm->GetResourceRawPointer(path->c_str());
        auto refArchive = am->GetArchiveFromFile(*path);
        if (raw == nullptr || refArchive == nullptr) {
            stats.unresolvedRefs++;
            fprintf(stderr, "[crossgame-model] reference %s did not load (raw=%p archive=%s)\n", path->c_str(), raw,
                    refArchive == nullptr ? "<none>" : refArchive->GetPath().c_str());
            continue;
        }
        if (CgmIsOoTArchive(refArchive->GetPath())) {
            stats.ootServedRefs++;
            fprintf(stderr, "[crossgame-model] reference %s served by OoT archive %s — this is the cross-game "
                            "texture-divergence failure mode\n",
                    path->c_str(), refArchive->GetPath().c_str());
            continue;
        }

        switch (opcode) {
            case kCgmSetTimgOtrHash:
                stats.textureRefs++;
                break;
            case kCgmVtxOtrHash:
                stats.vertexRefs++;
                break;
            case kCgmDlOtrHash:
            case kCgmBranchZOtr:
                stats.subDlRefs++;
                break;
            default:
                break;
        }
        if (std::find(stats.resolved.begin(), stats.resolved.end(), *path) == stats.resolved.end()) {
            stats.resolved.push_back(*path);
        }

        // Follow display-list edges. Both gsSPDisplayList and gsSPBranchLessZ
        // hand the interpreter another command stream that runs under the same
        // host, so a raw segmented G_SETTIMG down there is the same hazard as
        // one up here.
        if (opcode != kCgmDlOtrHash && opcode != kCgmBranchZOtr) {
            continue;
        }
        if (std::find(visited.begin(), visited.end(), *path) != visited.end()) {
            continue;
        }
        if (depth + 1 > kCgmMaxDepth) {
            stats.unresolvedRefs++;
            fprintf(stderr, "[crossgame-model] sub-display-list nesting exceeded %d levels at %s — refusing to walk "
                            "further, so this model is NOT fully checked\n",
                    kCgmMaxDepth, path->c_str());
            continue;
        }
        visited.push_back(*path);
        auto subDl = CgmLoadSubDisplayList(rm, *path);
        if (subDl == nullptr) {
            // Not a DisplayList (or would not parse as one). It already counted
            // as resolved above; there is simply no stream to walk.
            continue;
        }
        CgmWalkStream(rm, am, *path, *subDl, depth + 1, visited, stats);
    }
}

} // namespace

extern "C" int CrossGameModel_RunHeadless(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetResourceManager() == nullptr ||
        ctx->GetResourceManager()->GetArchiveManager() == nullptr) {
        fprintf(stderr, "[crossgame-model] FAIL: resource manager not initialized (caller must run the shared "
                        "bring-up first)\n");
        return 1;
    }
    auto rm = ctx->GetResourceManager();
    const char* dlPath = CgmDisplayListPath();
    auto am = rm->GetArchiveManager();

    // ---- 1. The model itself resolves, from a non-OoT archive --------------
    auto ownerArchive = am->GetArchiveFromFile(dlPath);
    if (ownerArchive == nullptr) {
        fprintf(stderr,
                "[crossgame-model] FAIL: %s is not in any mounted archive. The curated cross-game archive is the "
                "only thing that puts an MM path in reach of an OoT-only session (#577).\n",
                dlPath);
        return 1;
    }
    if (CgmIsOoTArchive(ownerArchive->GetPath())) {
        fprintf(stderr, "[crossgame-model] FAIL: %s resolved to OoT archive %s — it is supposed to be MM-exclusive\n",
                dlPath, ownerArchive->GetPath().c_str());
        return 1;
    }
    if (!CgmIsCuratedArchive(ownerArchive->GetPath())) {
        fprintf(stderr,
                "[crossgame-model] FAIL: %s resolved to %s, not to the curated cross-game archive the caller just "
                "mounted. Passing on some other archive's copy would make this row vacuous (see "
                "CgmIsCuratedArchive).\n",
                dlPath, ownerArchive->GetPath().c_str());
        return 1;
    }
    printf("[crossgame-model] %s served by %s\n", dlPath, ownerArchive->GetPath().c_str());

    auto resource = rm->LoadResourceProcess(dlPath, /*loadExact*/ true, nullptr);
    if (resource == nullptr) {
        fprintf(stderr, "[crossgame-model] FAIL: %s did not parse (no DisplayList factory, or corrupt entry)\n",
                dlPath);
        return 1;
    }
    auto displayList = std::dynamic_pointer_cast<Fast::DisplayList>(resource);
    if (displayList == nullptr) {
        fprintf(stderr, "[crossgame-model] FAIL: %s parsed as something other than a DisplayList\n", dlPath);
        return 1;
    }
    if (displayList->Instructions.empty()) {
        fprintf(stderr, "[crossgame-model] FAIL: %s parsed to an empty command stream\n", dlPath);
        return 1;
    }

    // ---- 2. Walk the whole model: this stream and every sub-DL it reaches ---
    CgmStats stats;
    std::vector<std::string> visited;
    visited.push_back(dlPath);
    CgmWalkStream(rm.get(), am.get(), dlPath, *displayList, 0, visited, stats);

    printf("[crossgame-model] %d stream(s) walked; refs: %d texture, %d vertex, %d sub-DL; %zu distinct resources\n",
           stats.streamsWalked, stats.textureRefs, stats.vertexRefs, stats.subDlRefs, stats.resolved.size());
    for (const auto& path : stats.resolved) {
        printf("[crossgame-model]   -> %s\n", path.c_str());
    }

    const int textureRefs = stats.textureRefs;
    const int vertexRefs = stats.vertexRefs;
    const int rawSegmentedTextures = stats.rawSegmentedTextures;
    const int unresolvedRefs = stats.unresolvedRefs;
    const int ootServedRefs = stats.ootServedRefs;

    int failures = 0;
    if (rawSegmentedTextures > 0) {
        fprintf(stderr,
                "[crossgame-model] FAIL: %d raw segmented texture reference(s). These resolve against the HOST "
                "game's segment table, which is the OoTMM kObjectPatches[] hazard — this model cannot be drawn "
                "cross-game without a patch entry.\n",
                rawSegmentedTextures);
        failures++;
    }
    if (unresolvedRefs > 0) {
        fprintf(stderr, "[crossgame-model] FAIL: %d reference(s) did not resolve\n", unresolvedRefs);
        failures++;
    }
    if (ootServedRefs > 0) {
        fprintf(stderr, "[crossgame-model] FAIL: %d reference(s) served by an OoT archive\n", ootServedRefs);
        failures++;
    }
    // Non-vacuity: a model that references no textures cannot demonstrate that
    // foreign textures resolve, so an empty walk is a failure, not a pass.
    if (textureRefs < 1) {
        fprintf(stderr, "[crossgame-model] FAIL: zero texture references walked — the lock is vacuous\n");
        failures++;
    }
    if (vertexRefs < 1) {
        fprintf(stderr, "[crossgame-model] FAIL: zero vertex references walked — the lock is vacuous\n");
        failures++;
    }

    return failures == 0 ? 0 : 1;
}
