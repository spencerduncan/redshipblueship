#include <ship/resource/ResourceManager.h>
#include "Skeleton.h"
#include "soh/OTRGlobals.h"
#include "libultraship/libultraship.h"
#include <soh_assets.h>
#include <objects/object_link_child/object_link_child.h>
#include <objects/object_link_boy/object_link_boy.h>

extern "C" {
#include "variables.h"
#include "z64.h"
#include "macros.h"
#include "z64player.h"
extern PlayState* OoT_gPlayState;
}

extern "C" SaveContext gSaveContext;
extern "C" u16 OoT_gEquipMasks[4];
extern "C" u8 OoT_gEquipShifts[4];

namespace SOH {
SkeletonData* Skeleton::GetPointer() {
    return &skeletonData;
}

size_t Skeleton::GetPointerSize() {
    switch (type) {
        case SkeletonType::Normal:
            return sizeof(skeletonData.skeletonHeader);
        case SkeletonType::Flex:
            return sizeof(skeletonData.flexSkeletonHeader);
        case SkeletonType::Curve:
            return sizeof(skeletonData.skelCurveLimbList);
        default:
            return 0;
    }
}

std::vector<SkeletonPatchInfo> SkeletonPatcher::skeletons;

bool SkeletonPatcher::IsLinkSkeletonPath(const std::string& path) {
    return (sOtr + path == std::string(gLinkAdultSkel)) || (sOtr + path == std::string(gLinkChildSkel));
}

bool SkeletonPatcher::IsLocalPlayerSkelAnime(SkelAnime* skelAnime) {
    if (OoT_gPlayState == nullptr) {
        return false;
    }

    Player* player = GET_PLAYER(OoT_gPlayState);

    if (player == nullptr) {
        return false;
    }

    PauseContext* pauseCtx = &OoT_gPlayState->pauseCtx;

    return (skelAnime == &player->skelAnime) || (skelAnime == &player->upperSkelAnime) ||
           (skelAnime == &pauseCtx->playerSkelAnime);
}

void SkeletonPatcher::RegisterSkeleton(std::string& path, SkelAnime* skelAnime) {
    SkeletonPatchInfo info;

    info.skelAnime = skelAnime;
    info.isLocalPlayer = false;

    if (path.starts_with(sOtr)) {
        path = path.substr(sOtr.length());
    }

    // Determine if we're using an alternate skeleton
    if (path.starts_with(Ship::IResource::gAltAssetPrefix)) {
        info.vanillaSkeletonPath = path.substr(Ship::IResource::gAltAssetPrefix.length(),
                                               path.size() - Ship::IResource::gAltAssetPrefix.length());
    } else {
        info.vanillaSkeletonPath = path;
    }

    if (IsLinkSkeletonPath(info.vanillaSkeletonPath)) {
        info.isLocalPlayer = IsLocalPlayerSkelAnime(skelAnime);

        // Skip registering skeletons that do not belong to the local player (e.g. Anchor dummy actors)
        if (!info.isLocalPlayer) {
            return;
        }
    }

    skeletons.push_back(info);
}

void SkeletonPatcher::UnregisterSkeleton(SkelAnime* skelAnime) {

    // TODO: Should probably just use a dictionary here...
    for (size_t i = 0; i < skeletons.size(); i++) {
        auto skel = skeletons[i];

        if (skel.skelAnime == skelAnime) {
            skeletons.erase(skeletons.begin() + i);
            break;
        }
    }
}
void SkeletonPatcher::ClearSkeletons() {
    skeletons.clear();
}

void SkeletonPatcher::UpdateSkeletons() {
    auto resourceMgr = Ship::Context::GetInstance()->GetResourceManager();
    bool isAlt = resourceMgr->IsAltAssetsEnabled();
    for (auto skel : skeletons) {
        Skeleton* newSkel =
            (Skeleton*)resourceMgr
                ->LoadResource((isAlt ? Ship::IResource::gAltAssetPrefix : "") + skel.vanillaSkeletonPath, true)
                .get();

        if (newSkel != nullptr) {
            ApplyEntry(skel, newSkel->skeletonData.skeletonHeader.segment, (uintptr_t)newSkel->GetPointer());
        }
    }
}

void SkeletonPatcher::ApplyEntry(SkeletonPatchInfo& skel, void** skeletonSegment, uintptr_t resourcePointer) {
    skel.skelAnime->skeleton = skeletonSegment;
    memcpy(&skel.skelAnime->skeletonHeader, &resourcePointer,
           sizeof(uintptr_t)); // Dumb thing that needs to be done because cast is not cooperating
}

void SkeletonPatcher::UpdateCustomSkeletons() {
    for (auto skel : skeletons) {
        if (!skel.isLocalPlayer) {
            continue;
        }

        UpdateTunicSkeletons(skel);
    }
}

void SkeletonPatcher::UpdateTunicSkeletons(SkeletonPatchInfo& skel) {
    std::string skeletonPath = "";

    // Check if this is one of Link's skeletons
    if (sOtr + skel.vanillaSkeletonPath == std::string(gLinkAdultSkel)) {
        // Check what Link's current tunic is
        switch (TUNIC_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC))) {
            case PLAYER_TUNIC_KOKIRI:
                skeletonPath = std::string(gLinkAdultKokiriTunicSkel).substr(sOtr.length());
                break;
            case PLAYER_TUNIC_GORON:
                skeletonPath = std::string(gLinkAdultGoronTunicSkel).substr(sOtr.length());
                break;
            case PLAYER_TUNIC_ZORA:
                skeletonPath = std::string(gLinkAdultZoraTunicSkel).substr(sOtr.length());
                break;
            default:
                return;
        }

        UpdateCustomSkeletonFromPath(skeletonPath, skel);
    } else if (sOtr + skel.vanillaSkeletonPath == std::string(gLinkChildSkel)) {
        // Check what Link's current tunic is
        switch (TUNIC_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC))) {
            case PLAYER_TUNIC_KOKIRI:
                skeletonPath = std::string(gLinkChildKokiriTunicSkel).substr(sOtr.length());
                break;
            case PLAYER_TUNIC_GORON:
                skeletonPath = std::string(gLinkChildGoronTunicSkel).substr(sOtr.length());
                break;
            case PLAYER_TUNIC_ZORA:
                skeletonPath = std::string(gLinkChildZoraTunicSkel).substr(sOtr.length());
                break;
            default:
                return;
        }

        UpdateCustomSkeletonFromPath(skeletonPath, skel);
    }
}

void SkeletonPatcher::UpdateCustomSkeletonFromPath(const std::string& skeletonPath, SkeletonPatchInfo& skel) {
    Skeleton* newSkel = nullptr;
    Skeleton* altSkel = nullptr;
    auto resourceMgr = Ship::Context::GetInstance()->GetResourceManager();
    bool isAlt = resourceMgr->IsAltAssetsEnabled();

    // If alt assets are on, look for alt tagged skeletons
    if (isAlt) {
        altSkel = (Skeleton*)Ship::Context::GetInstance()
                      ->GetResourceManager()
                      ->LoadResource(Ship::IResource::gAltAssetPrefix + skeletonPath, true)
                      .get();

        // Override non-alt skeleton if necessary
        if (altSkel != nullptr) {
            newSkel = altSkel;
        }
    }

    // Load new skeleton based on the custom model if it exists
    if (altSkel == nullptr) {
        newSkel = (Skeleton*)Ship::Context::GetInstance()->GetResourceManager()->LoadResource(skeletonPath, true).get();
    }

    // Change back to the original skeleton if no skeleton's were found
    if (newSkel == nullptr && skeletonPath != skel.vanillaSkeletonPath) {
        UpdateCustomSkeletonFromPath(skel.vanillaSkeletonPath, skel);
        return;
    }

    if (newSkel != nullptr) {
        ApplyEntry(skel, newSkel->skeletonData.skeletonHeader.segment, (uintptr_t)newSkel->GetPointer());
    }
}
} // namespace SOH

#ifdef RSBS_SINGLE_EXECUTABLE
// RedShipBlueShip (#1176) test seams, C linkage so the ROM-free
// skeleton-patcher-mm-layout row (src/common/tests/test_skeleton_patcher_layout.c)
// can reach them without OoT's headers.

// Applies one patch entry tagged with `game` (a GameId) to the bytes at skelAnime.
extern "C" void SkeletonPatcher_ApplyForTest(void* skelAnime, int game, void** skeletonSegment,
                                             uintptr_t resourcePointer) {
    (void)game; // every entry is written with OoT's layout
    SOH::SkeletonPatchInfo info;
    info.skelAnime = (SkelAnime*)skelAnime;
    info.isLocalPlayer = false;
    SOH::SkeletonPatcher::ApplyEntry(info, skeletonSegment, resourcePointer);
}

// The GameId whose SkelAnime layout the registered entry for skelAnime is
// written with, or -1 when nothing is registered for it.
extern "C" int SkeletonPatcher_RegisteredGameForTest(const void* skelAnime) {
    for (const auto& skel : SOH::SkeletonPatcher::skeletons) {
        if ((const void*)skel.skelAnime == skelAnime) {
            return 1; // GAME_OOT: every entry is written with OoT's layout
        }
    }
    return -1;
}
#endif
