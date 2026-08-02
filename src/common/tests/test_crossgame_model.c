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
 * none, plus that every hash reference actually resolves and is served by an
 * MM-side archive.
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
 * Non-vacuity: the row fails if the display list turns out to reference zero
 * textures or zero vertices. A model that references nothing cannot garble, so
 * it also cannot prove anything.
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

// Archives whose bytes would mean the reference resolved to the WRONG game.
// soh.o2r is mounted by the OoT bring-up itself, so this is a live check, not a
// theoretical one.
bool CgmIsOoTArchive(const std::string& archivePath) {
    static const char* kOoTArchives[] = { "oot.o2r", "oot-mq.o2r", "soh.o2r" };
    for (const char* name : kOoTArchives) {
        const size_t len = std::string(name).size();
        if (archivePath.size() >= len && archivePath.compare(archivePath.size() - len, len, name) == 0) {
            return true;
        }
    }
    return false;
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

    // ---- 2. Walk the command stream ---------------------------------------
    int textureRefs = 0;
    int vertexRefs = 0;
    int subDlRefs = 0;
    int rawSegmentedTextures = 0;
    int unresolvedRefs = 0;
    int ootServedRefs = 0;
    std::vector<std::string> resolved;

    const auto& instructions = displayList->Instructions;
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
                rawSegmentedTextures++;
                fprintf(stderr, "[crossgame-model] raw segmented texture at instruction %zu: segment 0x%02X\n", i,
                        (unsigned)((operand >> 24) & 0x0F));
            }
            continue;
        }

        if (!CgmIsExpanded(opcode)) {
            continue;
        }

        // Expanded (128-bit) command: the payload is the next Gfx.
        if (i + 1 >= instructions.size()) {
            fprintf(stderr, "[crossgame-model] FAIL: truncated expanded command 0x%02X at the end of the stream\n",
                    (unsigned)opcode);
            return 1;
        }
        const Gfx payload = instructions[++i];
        if (opcode == kCgmMarker) {
            continue; // Debug marker, no resource behind it.
        }

        const uint64_t hash = ((uint64_t)(uint32_t)payload.words.w0 << 32) | (uint32_t)payload.words.w1;
        const std::string* path = am->HashToString(hash);
        if (path == nullptr) {
            unresolvedRefs++;
            fprintf(stderr, "[crossgame-model] unresolved reference: opcode 0x%02X hash 0x%016llX\n",
                    (unsigned)opcode, (unsigned long long)hash);
            continue;
        }

        // This is the interpreter's own resolution: the OTR handlers take the
        // reference out of the command word and go straight to the resource
        // manager (libultraship/src/fast/interpreter.cpp).
        const void* raw = rm->GetResourceRawPointer(path->c_str());
        auto refArchive = am->GetArchiveFromFile(*path);
        if (raw == nullptr || refArchive == nullptr) {
            unresolvedRefs++;
            fprintf(stderr, "[crossgame-model] reference %s did not load (raw=%p archive=%s)\n", path->c_str(), raw,
                    refArchive == nullptr ? "<none>" : refArchive->GetPath().c_str());
            continue;
        }
        if (CgmIsOoTArchive(refArchive->GetPath())) {
            ootServedRefs++;
            fprintf(stderr, "[crossgame-model] reference %s served by OoT archive %s — this is the cross-game "
                            "texture-divergence failure mode\n",
                    path->c_str(), refArchive->GetPath().c_str());
            continue;
        }

        switch (opcode) {
            case kCgmSetTimgOtrHash:
                textureRefs++;
                break;
            case kCgmVtxOtrHash:
                vertexRefs++;
                break;
            case kCgmDlOtrHash:
                subDlRefs++;
                break;
            default:
                break;
        }
        if (std::find(resolved.begin(), resolved.end(), *path) == resolved.end()) {
            resolved.push_back(*path);
        }
    }

    printf("[crossgame-model] %zu commands; refs: %d texture, %d vertex, %d sub-DL; %zu distinct resources\n",
           instructions.size(), textureRefs, vertexRefs, subDlRefs, resolved.size());
    for (const auto& path : resolved) {
        printf("[crossgame-model]   -> %s\n", path.c_str());
    }

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
