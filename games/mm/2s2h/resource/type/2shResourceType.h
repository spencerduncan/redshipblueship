#pragma once

namespace S2H {
enum class ResourceType {
    SOH_Array = 0x4F415252,           // OARR
    SOH_Animation = 0x4F414E4D,       // OANM
    SOH_PlayerAnimation = 0x4F50414D, // OPAM
    SOH_Room = 0x4F524F4D,            // OROM
    SOH_CollisionHeader = 0x4F434F4C, // OCOL
    SOH_Skeleton = 0x4F534B4C,        // OSKL
    SOH_SkeletonLimb = 0x4F534C42,    // OSLB
    SOH_Path = 0x4F505448,            // OPTH
    SOH_Cutscene = 0x4F435654,        // OCUT
    SOH_Text = 0x4F545854,            // OTXT
    SOH_Audio = 0x4F415544,           // OAUD
    SOH_AudioSample = 0x4F534D50,     // OSMP
    SOH_AudioSoundFont = 0x4F534654,  // OSFT
    SOH_AudioSequence = 0x4F534551,   // OSEQ
    SOH_Background = 0x4F424749,      // OBGI
    SOH_SceneCommand = 0x4F52434D,    // ORCM

    TSH_TextMM = 0x4F54584D, // OTXM

    TSH_TexAnim = 0x4F54414E,       // OTAN
    TSH_CKeyFrameAnim = 0x4F4B4641, // OKFA
    TSH_CKeyFrameSkel = 0x4F4B4653  // OKFS
};
} // namespace S2H

// Compatibility alias for the OTRExporter submodule, whose GAME_MM exporters
// still say SOH:: (upstream 2Ship naming). MM's resource layer itself was
// renamed to S2H in redshipblueship (issue #344) so it can be compiled
// alongside OoT's SOH classes in single-exe builds. GAME_MM is defined only
// for the OTRExporter_MM target (OTRExporter/OTRExporter/CMakeLists.txt), and
// every OTRExporter TU is strictly GAME_MM- or GAME_OOT-scoped, so no
// translation unit ever sees both this alias and OoT's real namespace SOH.
// Game and common TUs (including src/common/test_runner.cpp, which reaches
// this header via tests/test_mm_scene_parse.c) never define GAME_MM and never
// import the alias.
#ifdef GAME_MM
namespace SOH = S2H;
#endif
