/**
 * @file test_array_reader_agreement.c
 * @brief #604: both games' 'OARR' Array readers turn the same bytes into the same vertices.
 *
 * Every extracted object's vertex data is an 'OARR' Array resource, and both
 * ports ship a reader for it (SOH:: and S2H::ResourceFactoryBinaryArrayV0).
 * Which reader parses a given array depends on the loader slot's owner and the
 * archive that served it, so a cross-game model drawn by OoT parses MM's
 * vertices with OoT's reader. That only works while the two readers agree, and
 * a divergence produces garbled geometry, not an error. Two rows:
 *
 * array-reader-agreement (ROM-free, never skips): the production boot check
 * Combo_ArrayReaders_BootCheck, then the same comparison spelled out over the
 * synthetic payload (every field distinct), then SENSITIVITY CONTROLS: a copy of
 * the vendored reader's Vertex path with ONE deliberate mutation must be seen
 * to disagree with OoT's reader. Without those controls a comparator that
 * always said "equal" would pass.
 *
 * array-reader-agreement-mm (SKIPs unless mm.o2r is staged): the same over
 * REAL MM vertex arrays (the shipped cross-game model object_mask_truth, and
 * the Zora barrier's object_link_zora_Vtx_011210) read out of mm.o2r through a
 * private ArchiveManager. Then the production loader slot: with MM's factories
 * registered over OoT's (as MM_Game_Init does) and mm.o2r recorded as MM's,
 * the 'Array' slot must hand MM's X8 array object_link_zora_U8_011710 to MM's
 * reader. Checked through MM's production accessor
 * ResourceMgr_LoadArrayByNameAsU8, the call Player_DrawZoraShield makes, whose
 * 80 alpha bytes must equal the bytes in the file. OoT's reader reads zero
 * bytes per X8 element and pushes uninitialized values.
 *
 * Observing the red half without touching vendored code: set
 * RSBS_ARRAY_AGREEMENT_MUTANT to 1 (tc[] read order swapped) or 2 (flag read
 * as one byte) and the mutated copy stands in for MM's reader in the agreement
 * comparisons, which must then FAIL. The dispatch leg's red half is main
 * itself: without the per-archive 'Array' registration, OoT's reader serves
 * the slot.
 *
 * FILE SCOPE, compiled as C++ (included by test_runner.cpp). Game-header-free:
 * each game's reader arrives through its creator, and the accessor is a C
 * symbol. Must be included AFTER test_curated_archive_order.c (CaoResolveArchive,
 * CaoMakeManager, CaoLoadThroughWinner).
 */

#include "array_reader_agreement.h"
#include "f3dvtx_wire_layout.h"

#include <ship/resource/ResourceFactoryBinary.h>
#include <ship/utils/binarytools/BinaryReader.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// Defined in each game's own TU (games/oot/soh/OTRGlobals.cpp,
// games/mm/2s2h/GameExports_SingleExe.cpp).
std::shared_ptr<Ship::ResourceFactory> OoT_CreateArrayFactory();
std::shared_ptr<Ship::ResourceFactory> MM_CreateArrayFactory();
// MM's production scalar accessor (games/mm/2s2h/mm_resources.cpp), the one
// Player_DrawZoraShield calls.
extern "C" uint8_t* ResourceMgr_LoadArrayByNameAsU8(const char* path, uint8_t* buffer);
extern "C" int OoT_RegisterModelResourceFactoriesHeadless(void);
extern "C" int MM_RegisterResourceFactoriesHeadless(void);
extern "C" int MM_MountArchiveHeadless(const char* path);

namespace {

// ---- The mutated copy -------------------------------------------------------

enum class AraMutation { TcOrderSwapped = 1, FlagReadAsByte = 2 };

const char* AraMutationName(AraMutation m) {
    return m == AraMutation::TcOrderSwapped ? "tc[] read order swapped" : "flag read as one byte";
}

class AraVertexArray final : public Ship::Resource<void> {
  public:
    using Resource::Resource;
    void* GetPointer() override {
        return Vertices.data();
    }
    size_t GetPointerSize() override {
        return Vertices.size() * sizeof(Fast::F3DVtx);
    }
    std::vector<Fast::F3DVtx> Vertices;
};

// The Vertex path of games/{oot/soh,mm/2s2h}/resource/importer/ArrayFactory.cpp,
// copied, with exactly one line changed per mutation. Non-Vertex arrays yield
// no vertices (which the comparator refuses as vacuous).
class AraMutantArrayReader final : public Ship::ResourceFactoryBinary {
  public:
    explicit AraMutantArrayReader(AraMutation mutation) : mMutation(mutation) {
    }

    std::shared_ptr<Ship::IResource> ReadResource(std::shared_ptr<Ship::File> file,
                                                  std::shared_ptr<Ship::ResourceInitData> initData) override {
        if (!FileHasValidFormatAndReader(file, initData)) {
            return nullptr;
        }
        auto array = std::make_shared<AraVertexArray>(initData);
        auto reader = std::get<std::shared_ptr<Ship::BinaryReader>>(file->Reader);
        const uint32_t arrayType = reader->ReadUInt32();
        const uint32_t arrayCount = reader->ReadUInt32();
        if (arrayType != rsbs::array_readers::kArrayTypeVertex) {
            return array;
        }
        for (uint32_t i = 0; i < arrayCount; i++) {
            Fast::F3DVtx data;
            data.v.ob[0] = reader->ReadInt16();
            data.v.ob[1] = reader->ReadInt16();
            data.v.ob[2] = reader->ReadInt16();
            if (mMutation == AraMutation::FlagReadAsByte) {
                data.v.flag = reader->ReadUByte(); // MUTATION (vendored: ReadUInt16)
            } else {
                data.v.flag = reader->ReadUInt16();
            }
            if (mMutation == AraMutation::TcOrderSwapped) {
                data.v.tc[1] = reader->ReadInt16(); // MUTATION (vendored: tc[0] first)
                data.v.tc[0] = reader->ReadInt16();
            } else {
                data.v.tc[0] = reader->ReadInt16();
                data.v.tc[1] = reader->ReadInt16();
            }
            data.v.cn[0] = reader->ReadUByte();
            data.v.cn[1] = reader->ReadUByte();
            data.v.cn[2] = reader->ReadUByte();
            data.v.cn[3] = reader->ReadUByte();
            array->Vertices.push_back(data);
        }
        return array;
    }

  private:
    AraMutation mMutation;
};

constexpr AraMutation kAraMutations[] = { AraMutation::TcOrderSwapped, AraMutation::FlagReadAsByte };

// RSBS_ARRAY_AGREEMENT_MUTANT=1|2 swaps a mutant in for MM's reader in the
// agreement comparisons: the observable red half. Unset: MM's real reader.
std::shared_ptr<Ship::ResourceFactory> AraSecondReader(const char** name) {
    const char* env = std::getenv("RSBS_ARRAY_AGREEMENT_MUTANT");
    const int m = (env != nullptr) ? std::atoi(env) : 0;
    if (m == 1 || m == 2) {
        const auto mutation = static_cast<AraMutation>(m);
        printf("[array-reader-agreement] RSBS_ARRAY_AGREEMENT_MUTANT=%d: MM's reader replaced by the mutated "
               "copy (%s) — this run MUST fail\n",
               m, AraMutationName(mutation));
        *name = "MM(mutant)";
        return std::make_shared<AraMutantArrayReader>(mutation);
    }
    *name = "MM";
    return MM_CreateArrayFactory();
}

// One payload: the pair must agree, and every mutant must be SEEN to disagree
// with OoT's reader (on real data a mutant may be undetectable on a given
// array, e.g. tc[0] == tc[1] everywhere, so the caller decides whether an
// undetected mutant is a failure).
struct AraPayloadResult {
    bool agree = false;
    int mutantsDetected = 0;
    bool mutantDetected[3] = { false, false, false };
};

AraPayloadResult AraCheckPayload(const std::vector<char>& payload, Ship::Endianness order, const std::string& path) {
    using namespace rsbs::array_readers;
    AraPayloadResult result;
    const char* secondName = "MM";
    auto second = AraSecondReader(&secondName);
    std::string diag;
    result.agree = ReadersAgree(OoT_CreateArrayFactory(), "OoT", second, secondName, payload, order, path, &diag);
    printf("[array-reader-agreement] %s %s vs %s: %s\n", result.agree ? "AGREE" : "DISAGREE", "OoT", secondName,
           diag.c_str());
    for (AraMutation m : kAraMutations) {
        std::string mdiag;
        const bool mutantAgrees =
            ReadersAgree(OoT_CreateArrayFactory(), "OoT", std::make_shared<AraMutantArrayReader>(m), "mutant", payload,
                         order, path, &mdiag);
        if (!mutantAgrees) {
            result.mutantsDetected++;
            result.mutantDetected[static_cast<int>(m)] = true;
        }
        printf("[array-reader-agreement]   control (%s): %s — %s\n", AraMutationName(m),
               mutantAgrees ? "NOT detected" : "detected", mdiag.c_str());
    }
    return result;
}

// ---- Real-archive helpers ---------------------------------------------------

struct AraOtrFile {
    bool ok = false;
    Ship::Endianness order = Ship::Endianness::Little;
    uint32_t type = 0;
    std::vector<char> payload; // bytes after the 64-byte OTR header
};

uint32_t AraU32(const char* p, Ship::Endianness order) {
    const auto* b = reinterpret_cast<const uint8_t*>(p);
    if (order == Ship::Endianness::Big) {
        return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | uint32_t(b[3]);
    }
    return uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24);
}

// Split one archive entry the way ResourceLoader::ReadResourceInitDataLegacy
// does: byte order at 0, resource type at 4, payload from OTR_HEADER_SIZE.
AraOtrFile AraReadOtr(Ship::ArchiveManager& mgr, const std::string& path) {
    AraOtrFile out;
    auto file = CaoLoadThroughWinner(mgr, path);
    if (file == nullptr || file->Buffer == nullptr || file->Buffer->size() < OTR_HEADER_SIZE + 8) {
        return out;
    }
    const std::vector<char>& buf = *file->Buffer;
    out.order = buf[0] == 1 ? Ship::Endianness::Big : Ship::Endianness::Little;
    out.type = AraU32(&buf[4], out.order);
    out.payload.assign(buf.begin() + OTR_HEADER_SIZE, buf.end());
    out.ok = true;
    return out;
}

constexpr uint32_t kAraTypeOARR = 0x4F415252; // 'OARR'
constexpr const char* kAraZoraShieldVtx = "objects/object_link_zora/object_link_zora_Vtx_011210";
constexpr const char* kAraZoraShieldAlpha = "objects/object_link_zora/object_link_zora_U8_011710";
constexpr uint32_t kAraScalarX8 = 3; // ScalarType::ZSCALAR_X8 in both ports' Array.h
constexpr uint32_t kAraArrayTypeVector = 24;

} // namespace

extern "C" int ArrayReaderAgreement_RunSynthetic(void) {
    using namespace rsbs::array_readers;
    int failures = 0;

    // 1. The production boot check, exactly as rsbs/src/main.cpp calls it.
    const int boot = Combo_ArrayReaders_BootCheck();
    printf("[array-reader-agreement] Combo_ArrayReaders_BootCheck() = %d\n", boot);
    if (boot != 0) {
        fprintf(stderr, "[array-reader-agreement] FAIL: the boot check refuses this build\n");
        failures++;
    }

    // 2. The same comparison spelled out, with the vertex count checked, the
    //    mutant knob honored, and the sensitivity controls run.
    const std::vector<char> payload = SyntheticVertexPayload();
    const ParsedArray oot =
        ParseArrayPayload(OoT_CreateArrayFactory(), payload, Ship::Endianness::Little, "rsbs/synthetic-vertex-array");
    if (!oot.ok || oot.raw.size() != SyntheticVertexCount() * sizeof(Fast::F3DVtx)) {
        fprintf(stderr,
                "[array-reader-agreement] FAIL: OoT's reader turned the %zu-vertex synthetic payload into %zu bytes "
                "(%s) — the payload no longer exercises the Vertex path\n",
                SyntheticVertexCount(), oot.raw.size(), oot.error.c_str());
        failures++;
    }
    const AraPayloadResult r = AraCheckPayload(payload, Ship::Endianness::Little, "rsbs/synthetic-vertex-array");
    if (!r.agree) {
        fprintf(stderr, "[array-reader-agreement] FAIL: the readers disagree on the synthetic payload\n");
        failures++;
    }
    const int mutationCount = static_cast<int>(sizeof(kAraMutations) / sizeof(kAraMutations[0]));
    if (r.mutantsDetected != mutationCount) {
        fprintf(stderr,
                "[array-reader-agreement] FAIL: only %d of %d one-line mutations of the reader were detected on "
                "the synthetic payload — the comparison is not sensitive enough to trust\n",
                r.mutantsDetected, mutationCount);
        failures++;
    }
    return failures == 0 ? 0 : 1;
}

extern "C" int ArrayReaderAgreement_RunMM(const char* mmArchive) {
    using namespace rsbs::array_readers;
    int failures = 0;

    // ---- 1. Real MM vertex arrays, both readers, private manager -----------
    auto mgr = CaoMakeManager({ mmArchive });
    if (mgr == nullptr) {
        fprintf(stderr, "[array-reader-agreement] FAIL: could not open %s\n", mmArchive);
        return 1;
    }
    std::vector<std::string> paths;
    auto maskFiles = mgr->ListFiles("objects/object_mask_truth/*");
    for (const auto& p : *maskFiles) {
        paths.push_back(p);
    }
    paths.push_back(kAraZoraShieldVtx);

    int vertexArrays = 0;
    bool detectedAnywhere[3] = { false, false, false };
    for (const auto& path : paths) {
        const AraOtrFile f = AraReadOtr(*mgr, path);
        if (!f.ok || f.type != kAraTypeOARR || f.payload.size() < 8 ||
            AraU32(f.payload.data(), f.order) != kArrayTypeVertex) {
            if (path == kAraZoraShieldVtx) {
                fprintf(stderr, "[array-reader-agreement] FAIL: %s is not an 'OARR' Vertex array in %s\n", path.c_str(),
                        mmArchive);
                failures++;
            }
            continue;
        }
        vertexArrays++;
        const AraPayloadResult r = AraCheckPayload(f.payload, f.order, path);
        if (!r.agree) {
            fprintf(stderr, "[array-reader-agreement] FAIL: the readers disagree on %s\n", path.c_str());
            failures++;
        }
        for (AraMutation m : kAraMutations) {
            detectedAnywhere[static_cast<int>(m)] |= r.mutantDetected[static_cast<int>(m)];
        }
    }
    printf("[array-reader-agreement] %d real MM vertex array(s) compared\n", vertexArrays);
    if (vertexArrays < 2) {
        fprintf(stderr, "[array-reader-agreement] FAIL: fewer than 2 real vertex arrays found — the leg is vacuous\n");
        failures++;
    }
    for (AraMutation m : kAraMutations) {
        if (!detectedAnywhere[static_cast<int>(m)]) {
            fprintf(stderr, "[array-reader-agreement] FAIL: the '%s' mutation was not detected on any real array\n",
                    AraMutationName(m));
            failures++;
        }
    }

    // ---- 2. The production 'Array' slot routes MM's archive to MM's reader --
    // The alpha bytes as they are in the file: per element a u32 scalar type,
    // then (X8) one byte. Also the premise check: every element really is X8.
    const AraOtrFile alpha = AraReadOtr(*mgr, kAraZoraShieldAlpha);
    std::vector<uint8_t> expected;
    bool premiseOk = alpha.ok && alpha.type == kAraTypeOARR && alpha.payload.size() >= 8;
    if (premiseOk) {
        const char* p = alpha.payload.data();
        const char* end = p + alpha.payload.size();
        const uint32_t arrayType = AraU32(p, alpha.order);
        const uint32_t count = AraU32(p + 4, alpha.order);
        p += 8;
        premiseOk = arrayType != kArrayTypeVertex && arrayType != kAraArrayTypeVector;
        for (uint32_t i = 0; i < count && premiseOk; i++) {
            if (end - p < 4 || AraU32(p, alpha.order) != kAraScalarX8) {
                premiseOk = false;
                break;
            }
            p += 4;
            if (end - p < 1) {
                premiseOk = false;
                break;
            }
            expected.push_back(static_cast<uint8_t>(*p));
            p += 1;
        }
    }
    if (!premiseOk || expected.size() != 80) {
        fprintf(stderr,
                "[array-reader-agreement] FAIL: %s is not the 80-element ZSCALAR_X8 array Player_DrawZoraShield "
                "reads (decoded %zu elements)\n",
                kAraZoraShieldAlpha, expected.size());
        return 1;
    }
    bool varied = false;
    for (uint8_t v : expected) {
        varied |= (v != expected[0]);
    }
    if (!varied) {
        fprintf(stderr, "[array-reader-agreement] FAIL: the file's 80 alpha bytes are all equal — a constant "
                        "cannot tell a parsed array from a filled buffer\n");
        failures++;
    }

    // Production order: OoT's factories at boot, then MM's over them
    // (MM_Game_Init), with mm.o2r recorded as MM's archive (LoadMMArchives).
    if (OoT_RegisterModelResourceFactoriesHeadless() != 0 || MM_MountArchiveHeadless(mmArchive) != 0 ||
        MM_RegisterResourceFactoriesHeadless() != 0) {
        fprintf(stderr, "[array-reader-agreement] FAIL: headless factory registration / MM mount failed\n");
        return 1;
    }
    auto rm = Ship::Context::GetInstance()->GetResourceManager();
    // A previous row in `--test all` could have parsed these already.
    rm->UnloadResource(kAraZoraShieldAlpha);
    rm->UnloadResource(kAraZoraShieldVtx);

    uint8_t got[80];
    std::memset(got, 0xA5, sizeof(got));
    uint8_t* ret = ResourceMgr_LoadArrayByNameAsU8(kAraZoraShieldAlpha, got);
    int mismatches = 0;
    int firstMismatch = -1;
    for (int i = 0; i < 80; i++) {
        if (got[i] != expected[i]) {
            mismatches++;
            if (firstMismatch < 0) {
                firstMismatch = i;
            }
        }
    }
    printf("[array-reader-agreement] ResourceMgr_LoadArrayByNameAsU8(%s): %d of 80 alpha bytes match the file "
           "(file[0..3] = %u %u %u %u, got[0..3] = %u %u %u %u)\n",
           kAraZoraShieldAlpha, 80 - mismatches, expected[0], expected[1], expected[2], expected[3], got[0], got[1],
           got[2], got[3]);
    if (ret != got || mismatches != 0) {
        fprintf(stderr,
                "[array-reader-agreement] FAIL: the production 'Array' slot did not parse MM's X8 array with MM's "
                "reader (%d mismatches, first at %d) — the Zora barrier would draw with garbage alpha\n",
                mismatches, firstMismatch);
        failures++;
    }

    // The Vertex path through the same production slot equals the direct parse.
    auto vtxRes = rm->LoadResourceProcess(kAraZoraShieldVtx, /*loadExact*/ true, nullptr);
    const AraOtrFile vtx = AraReadOtr(*mgr, kAraZoraShieldVtx);
    const ParsedArray direct = ParseArrayPayload(MM_CreateArrayFactory(), vtx.payload, vtx.order, kAraZoraShieldVtx);
    const bool slotMatches = vtxRes != nullptr && direct.ok && vtxRes->GetPointerSize() == direct.raw.size() &&
                             !direct.raw.empty() &&
                             std::memcmp(vtxRes->GetRawPointer(), direct.raw.data(), direct.raw.size()) == 0;
    printf("[array-reader-agreement] production slot vs direct parse of %s: %s (%zu bytes)\n", kAraZoraShieldVtx,
           slotMatches ? "identical" : "DIFFERENT", direct.raw.size());
    if (!slotMatches) {
        fprintf(stderr,
                "[array-reader-agreement] FAIL: the production 'Array' slot's vertex memory for %s differs "
                "from the reader's direct parse\n",
                kAraZoraShieldVtx);
        failures++;
    }
    return failures == 0 ? 0 : 1;
}
