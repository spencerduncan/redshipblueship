/**
 * @file MmVolumeSingleExe.cpp
 * @brief MM's half of src/common/mm_volume_bridge.h: reapply the five shared volume keys to MM's sequence players
 *        when SoH's Settings > Audio sliders change (#1235).
 *
 * See the bridge header for the defect and the contract. This TU is single-exe only: upstream 2Ship's own slider
 * callbacks (BenMenu.cpp, excluded here) call AudioSeq_SetPortVolumeScale directly.
 */

#include "global.h"

#include "mm_volume_bridge.h" // src/common

namespace {
// 1 from MM_AudioMgr_Init until MM_Game_Shutdown (GameExports_SingleExe.cpp). Suspend leaves it set: the audio
// context and its sequence players survive a suspend, only the dispatch is parked.
int sMmAudioLive = 0;
} // namespace

extern "C" void MM_Volume_SetAudioLive(int live) {
    sMmAudioLive = live != 0 ? 1 : 0;
}

extern "C" int MM_Volume_AudioLive(void) {
    return sMmAudioLive;
}

extern "C" int MM_Volume_ReapplyPortScales(void) {
    // Today's behaviour, kept by the lock commit: nothing reaches MM's players after MM_AudioMgr_Init.
    return 0;
}
