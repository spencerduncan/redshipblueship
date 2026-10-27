/**
 * @file MmVolumeSingleExe.cpp
 * @brief MM's half of src/common/mm_volume_bridge.h: reapply the five shared volume keys to MM's sequence players
 *        when SoH's Settings > Audio sliders change (#1235).
 *
 * See the bridge header for the defect and the contract. This TU is single-exe only: upstream 2Ship's own slider
 * callbacks (BenMenu.cpp, excluded here) call AudioSeq_SetPortVolumeScale directly.
 */

#include "global.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include "cvar_shared_keys.h" // src/common: RSBS_CVAR_VOLUME_*
#include "mm_volume_bridge.h" // src/common

namespace {
// 1 from MM_AudioMgr_Init until MM_Game_Shutdown (GameExports_SingleExe.cpp). Suspend leaves it set: the audio
// context and its sequence players survive a suspend, only the dispatch is parked.
int sMmAudioLive = 0;

// MM_AudioMgr_Init's five reads (audio_thread_manager.c), the same keys, players and conversion: integer percent,
// default 100, scaled to 0..1.
struct VolumeChannel {
    u8 player;
    const char* key;
};
constexpr VolumeChannel kChannels[] = {
    { SEQ_PLAYER_BGM_MAIN, RSBS_CVAR_VOLUME_MAIN_MUSIC },
    { SEQ_PLAYER_BGM_SUB, RSBS_CVAR_VOLUME_SUB_MUSIC },
    { SEQ_PLAYER_SFX, RSBS_CVAR_VOLUME_SFX },
    { SEQ_PLAYER_FANFARE, RSBS_CVAR_VOLUME_FANFARE },
    { SEQ_PLAYER_AMBIENCE, RSBS_CVAR_VOLUME_AMBIENCE },
};
} // namespace

extern "C" void MM_Volume_SetAudioLive(int live) {
    sMmAudioLive = live != 0 ? 1 : 0;
}

extern "C" int MM_Volume_AudioLive(void) {
    return sMmAudioLive;
}

extern "C" int MM_Volume_ReapplyPortScales(void) {
    if (!sMmAudioLive) {
        // MM_AudioMgr_Init reads the keys itself when it runs; before then MM_AudioLoad_Init would wipe a write.
        return 0;
    }
    for (const VolumeChannel& c : kChannels) {
        AudioSeq_SetPortVolumeScale(c.player, (float)CVarGetInteger(c.key, 100) / 100.0f);
    }
    return 1;
}
