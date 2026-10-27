/**
 * @file oot_xml_skeleton_empty_limb_test.cpp
 * Lock for #1190: an XML skeleton limb whose DisplayList1 is Fast64's empty
 * marker "gEmptyDL" has NO display list.
 *
 * XmlSkeletonEmptyLimb (CTest label "redship", dispatch "xml-skeleton-empty-limb"):
 * display-free and ROM-free, never skips.
 *
 * WHY. Fast64 exports a limb with no mesh as DisplayList1="gEmptyDL". SoH's
 * ResourceFactoryXMLSkeletonLimbV0 recognises that marker and leaves the limb's
 * dLists[0] null; the January symbol-namespacing pass (#53) rewrote the compared
 * literal with the OoT_ prefix, so the marker stopped matching, the limb got
 * dLists[0] = "__OTR__gEmptyDL", and the first draw of a custom skeleton (the
 * title demo's Link with Alt Assets on) loaded a display list that does not exist
 * and faulted in ResourceMgr_LoadGfxByName.
 *
 * LEG 1, the factory alone (no Context): a synthetic limb document of the shape a
 * Fast64 mod ships is handed to ResourceFactoryXMLSkeletonLimbV0::ReadResource;
 * the "gEmptyDL" limb must come back with dLists[0] == nullptr, and a control limb
 * naming a real path must come back with "__OTR__<path>".
 *
 * LEG 2, the production loader: a loose-folder archive in a per-process temp
 * directory holds a Flex skeleton with four XML limbs (the root marked
 * "gEmptyDL", three with display lists) and DisplayListCount 3. It is loaded
 * through the shared ResourceManager with the XML Skeleton and SkeletonLimb
 * factories registered as OTRGlobals::Initialize registers them. The root limb
 * must have no display list, and the number of limbs with one must equal
 * DisplayListCount: SkelAnime_DrawFlexLod allocates DisplayListCount matrices and
 * every limb with a display list consumes one, so a mismatch is a fault or an
 * overrun at draw time.
 */

#include "soh/resource/importer/SkeletonLimbFactory.h"
#include "soh/resource/type/Skeleton.h"
#include "soh/resource/type/SkeletonLimb.h"
#include "soh/resource/type/SohResourceType.h"

#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <tinyxml2.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define XSEL_GETPID _getpid
#else
#include <unistd.h>
#define XSEL_GETPID getpid
#endif

// games/oot/soh/OTRGlobals.cpp: the Skeleton and SkeletonLimb factories, binary
// and XML, registered on the shared loader without the display-bound Initialize.
extern "C" int OoT_RegisterSkeletonResourceFactoriesHeadless(void);

namespace {

constexpr const char* kXselSkelPath = "objects/rsbs_test/gTestSkel";
constexpr int kXselLimbCount = 4;
constexpr int kXselDListCount = 3;

std::string XselLimbPath(int i) {
    char buf[64];
    snprintf(buf, sizeof(buf), "objects/rsbs_test/gTestSkelLimb_%03d", i);
    return buf;
}

// The shape of the mod's gLinkAdultSkelLimb_000 (3ds_hero_malon_(2026).otr):
// synthetic text, no mod data.
std::string XselLimbXml(const char* displayList1, int childIndex, int siblingIndex, int legTransZ) {
    std::string xml = "<SkeletonLimb Version=\"0\" Type=\"Standard\" LegTransX=\"0\" LegTransY=\"0\" LegTransZ=\"" +
                      std::to_string(legTransZ) + "\" ChildIndex=\"" + std::to_string(childIndex) +
                      "\" SiblingIndex=\"" + std::to_string(siblingIndex) + "\"";
    if (displayList1 != nullptr) {
        xml += std::string(" DisplayList1=\"") + displayList1 + "\"";
    }
    xml += "/>";
    return xml;
}

// ReadResource over one in-memory limb document. Returns the parsed limb, or
// nullptr when the factory refused it.
std::shared_ptr<SOH::SkeletonLimb> XselReadLimb(const std::string& xml, const char* path) {
    auto doc = std::make_shared<tinyxml2::XMLDocument>();
    if (doc->Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) {
        printf("[xml-skeleton-empty-limb] could not parse the synthetic limb: %s\n", doc->ErrorStr());
        return nullptr;
    }
    auto file = std::make_shared<Ship::File>();
    file->Buffer = std::make_shared<std::vector<char>>(xml.begin(), xml.end());
    file->Reader = doc;
    file->IsLoaded = true;

    auto initData = std::make_shared<Ship::ResourceInitData>();
    initData->Path = path;
    initData->ByteOrder = Ship::Endianness::Native;
    initData->Type = static_cast<uint32_t>(SOH::ResourceType::SOH_SkeletonLimb);
    initData->ResourceVersion = 0;
    initData->Id = 0;
    initData->IsCustom = true;
    initData->Format = RESOURCE_FORMAT_XML;

    auto res = SOH::ResourceFactoryXMLSkeletonLimbV0().ReadResource(file, initData);
    return std::static_pointer_cast<SOH::SkeletonLimb>(res);
}

const char* XselDescribe(const Gfx* dl) {
    return dl == nullptr ? "(null)" : reinterpret_cast<const char*>(dl);
}

int XselLegFactory() {
    int failures = 0;

    // The marker: no display list.
    auto empty = XselReadLimb(XselLimbXml("gEmptyDL", 1, 255, 3605), "objects/rsbs_test/limb_empty");
    if (empty == nullptr) {
        printf("[xml-skeleton-empty-limb] FAIL: the factory refused the gEmptyDL limb\n");
        failures++;
    } else {
        const Gfx* dl = empty->limbData.lodLimb.dLists[0];
        printf("[xml-skeleton-empty-limb] DisplayList1=\"gEmptyDL\" -> dLists[0] = %s\n", XselDescribe(dl));
        if (dl != nullptr) {
            printf("[xml-skeleton-empty-limb] FAIL: Fast64's gEmptyDL marker was not recognised: the limb names a "
                   "display list that does not exist and faults at the first draw (#1190)\n");
            failures++;
        }
    }

    // Control: a real path keeps its OTR reference.
    const char* realPath = "objects/rsbs_test/gTestDL1";
    const std::string expected = std::string("__OTR__") + realPath;
    auto real = XselReadLimb(XselLimbXml(realPath, 255, 255, 0), "objects/rsbs_test/limb_real");
    if (real == nullptr) {
        printf("[xml-skeleton-empty-limb] FAIL: the factory refused the control limb\n");
        failures++;
    } else {
        const Gfx* dl = real->limbData.lodLimb.dLists[0];
        printf("[xml-skeleton-empty-limb] DisplayList1=\"%s\" -> dLists[0] = %s\n", realPath, XselDescribe(dl));
        if (dl == nullptr || std::strcmp(reinterpret_cast<const char*>(dl), expected.c_str()) != 0) {
            printf("[xml-skeleton-empty-limb] FAIL: the control limb's display list is not \"%s\"\n", expected.c_str());
            failures++;
        }
    }
    return failures;
}

bool XselWriteText(const std::filesystem::path& p, const std::string& text) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    if (ec) {
        return false;
    }
    std::FILE* f = std::fopen(p.string().c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    return std::fclose(f) == 0 && ok;
}

int XselLegLoader() {
    auto ctx = Ship::Context::GetInstance();
    auto rm = ctx != nullptr ? ctx->GetResourceManager() : nullptr;
    auto archiveMgr = rm != nullptr ? rm->GetArchiveManager() : nullptr;
    if (archiveMgr == nullptr) {
        printf("[xml-skeleton-empty-limb] FAIL: no shared ArchiveManager\n");
        return 1;
    }
    if (OoT_RegisterSkeletonResourceFactoriesHeadless() != 0) {
        printf("[xml-skeleton-empty-limb] FAIL: headless skeleton factory registration failed\n");
        return 1;
    }

    std::error_code ec;
    const std::filesystem::path root =
        (std::filesystem::temp_directory_path(ec) / ("rsbs1190_xml_skel_" + std::to_string(XSEL_GETPID())))
            .lexically_normal();
    std::filesystem::remove_all(root, ec);

    // Limb 0 is the mesh-less root (Fast64's marker); limbs 1-3 each draw.
    std::string skel = "<Skeleton Version=\"0\" Type=\"Flex\" LimbCount=\"" + std::to_string(kXselLimbCount) +
                       "\" DisplayListCount=\"" + std::to_string(kXselDListCount) + "\">";
    bool staged = true;
    for (int i = 0; i < kXselLimbCount; i++) {
        skel += "<SkeletonLimb Path=\"" + XselLimbPath(i) + "\"/>";
        std::string dl = "objects/rsbs_test/gTestDL" + std::to_string(i);
        const int child = i + 1 < kXselLimbCount ? i + 1 : 255;
        const std::string limb = XselLimbXml(i == 0 ? "gEmptyDL" : dl.c_str(), child, 255, i == 0 ? 3605 : 100);
        staged = staged && XselWriteText(root / XselLimbPath(i), limb);
    }
    skel += "</Skeleton>";
    staged = staged && XselWriteText(root / kXselSkelPath, skel);
    if (!staged) {
        printf("[xml-skeleton-empty-limb] FAIL: could not stage %s\n", root.generic_string().c_str());
        std::filesystem::remove_all(root, ec);
        return 1;
    }

    const std::string archivePath = root.generic_string();
    auto archive = archiveMgr->AddArchive(archivePath);
    if (archive == nullptr) {
        printf("[xml-skeleton-empty-limb] FAIL: could not mount %s\n", archivePath.c_str());
        std::filesystem::remove_all(root, ec);
        return 1;
    }

    int failures = 0;
    auto res = rm->LoadResourceProcess(kXselSkelPath, /*loadExact*/ true, nullptr);
    auto skeleton = std::dynamic_pointer_cast<SOH::Skeleton>(res);
    if (skeleton == nullptr) {
        printf("[xml-skeleton-empty-limb] FAIL: %s did not load as a Skeleton\n", kXselSkelPath);
        failures++;
    } else {
        const auto& segments = skeleton->skeletonHeaderSegments;
        const int dListCount = skeleton->skeletonData.flexSkeletonHeader.dListCount;
        printf("[xml-skeleton-empty-limb] loaded %s: %zu limb(s), DisplayListCount %d\n", kXselSkelPath,
               segments.size(), dListCount);
        if (static_cast<int>(segments.size()) != kXselLimbCount || dListCount != kXselDListCount) {
            printf("[xml-skeleton-empty-limb] FAIL: expected %d limbs and DisplayListCount %d\n", kXselLimbCount,
                   kXselDListCount);
            failures++;
        } else {
            int withDList = 0;
            bool allLoaded = true;
            for (int i = 0; i < kXselLimbCount; i++) {
                // What SkelAnime_DrawFlexLod reads: the segment table's LodLimb.
                const auto* limb = static_cast<const SOH::LodLimb*>(segments[i]);
                if (limb == nullptr) {
                    printf("[xml-skeleton-empty-limb] FAIL: limb %d did not load\n", i);
                    allLoaded = false;
                    continue;
                }
                printf("[xml-skeleton-empty-limb]   limb %d dLists[0] = %s\n", i, XselDescribe(limb->dLists[0]));
                withDList += limb->dLists[0] != nullptr ? 1 : 0;
            }
            if (!allLoaded) {
                failures++;
            } else {
                const auto* rootLimb = static_cast<const SOH::LodLimb*>(segments[0]);
                if (rootLimb->dLists[0] != nullptr) {
                    printf("[xml-skeleton-empty-limb] FAIL: the gEmptyDL root limb carries a display list (#1190)\n");
                    failures++;
                }
                printf("[xml-skeleton-empty-limb] %d limb(s) with a display list, DisplayListCount %d\n", withDList,
                       dListCount);
                if (withDList != dListCount) {
                    printf("[xml-skeleton-empty-limb] FAIL: DrawFlexLod allocates DisplayListCount matrices and each "
                           "limb with a display list takes one: %d limbs against %d matrices\n",
                           withDList, dListCount);
                    failures++;
                }
            }
        }
    }

    skeleton.reset();
    res.reset();
    rm->UnloadResource(kXselSkelPath);
    for (int i = 0; i < kXselLimbCount; i++) {
        rm->UnloadResource(XselLimbPath(i));
    }
    archiveMgr->RemoveArchive(archive);
    archive.reset();
    std::filesystem::remove_all(root, ec);
    return failures;
}

} // namespace

extern "C" int OoT_XmlSkeletonEmptyLimb_RunHeadless(void) {
    printf("[TEST] xml-skeleton-empty-limb: an XML limb whose DisplayList1 is Fast64's \"gEmptyDL\" has no display "
           "list, through the factory and the loader (#1190)\n");
    int failures = XselLegFactory();
    failures += XselLegLoader();
    printf("[TEST] %s: xml-skeleton-empty-limb (%d failure(s))\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
