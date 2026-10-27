/**
 * @file mm_volume_live_apply_test.cpp
 * @brief ROM-free lock for #1235's live volume apply: a change to SoH's Settings > Audio sliders reaches Majora's
 *        Mask's sequence players while MM's audio is up, and is a no-op while it is not.
 *
 * CTest row MMVolumeLiveApply in CMake/SingleExecutable.cmake, dispatch "mm-volume-live-apply" in
 * src/common/test_runner.cpp.
 *
 * THE DEFECT. MM reads the five volume keys only in MM_AudioMgr_Init, once per MM bring-up, and SoH's slider
 * callbacks call only OoT's Audio_SetGameVolume, so a slider moved after MM's first init changed nothing in MM until
 * a restart. The fix is src/common/mm_volume_bridge.h's MM_Volume_ReapplyPortScales, which the five slider callbacks
 * call.
 *
 * WHY IT LOCKS ROM-FREE. The port volume scales are plain floats in MM's static audio context (gAudioCtx.seqPlayers),
 * written by AudioSeq_SetPortVolumeScale and read back by AudioSeq_GetPortVolumeScale; neither needs an archive, a
 * heap or the audio thread. "MM's audio is up" is the bridge's own flag, which the row drives both ways.
 *
 * WHAT IT ASSERTS:
 *   1. With MM's audio down, the reapply reports a no-op and leaves every scale as it was (a sentinel the row
 *      wrote), whatever the keys hold.
 *   2. With MM's audio up, it reports the write and leaves each of the five players at its key's percent / 100,
 *      with five distinct values so a crossed pair of players is caught.
 * RED without the fix: leg 2 sees the sentinels (the reapply wrote nothing).
 *
 * Restores the flag, the five scales and the five keys (cleared when they were unset) on every exit.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
extern "C" int MM_VolumeLiveApply_RunHeadless(void) {
    printf("[TEST] mm-volume-live-apply: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <climits>
#include <cmath>

#include <libultraship/bridge/consolevariablebridge.h>

#include "cvar_shared_keys.h" // src/common
#include "mm_volume_bridge.h" // src/common

namespace {

struct VolumeChannel {
    const char* key;
    u8 player;
    int32_t percent; // the value the row writes; distinct per channel
};

const VolumeChannel kChannels[] = {
    { RSBS_CVAR_VOLUME_MAIN_MUSIC, SEQ_PLAYER_BGM_MAIN, 37 },
    { RSBS_CVAR_VOLUME_SUB_MUSIC, SEQ_PLAYER_BGM_SUB, 41 },
    { RSBS_CVAR_VOLUME_SFX, SEQ_PLAYER_SFX, 43 },
    { RSBS_CVAR_VOLUME_FANFARE, SEQ_PLAYER_FANFARE, 47 },
    { RSBS_CVAR_VOLUME_AMBIENCE, SEQ_PLAYER_AMBIENCE, 53 },
};
constexpr int kChannelCount = (int)(sizeof(kChannels) / sizeof(kChannels[0]));
constexpr float kSentinel = 0.25f;

bool Near(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

struct Saved {
    int live;
    float scale[kChannelCount];
    int32_t key[kChannelCount]; // INT32_MIN = unset
};

void Restore(const Saved& s) {
    for (int i = 0; i < kChannelCount; i++) {
        AudioSeq_SetPortVolumeScale(kChannels[i].player, s.scale[i]);
        if (s.key[i] == INT32_MIN) {
            CVarClear(kChannels[i].key);
        } else {
            CVarSetInteger(kChannels[i].key, s.key[i]);
        }
    }
    MM_Volume_SetAudioLive(s.live);
}

} // namespace

extern "C" int MM_VolumeLiveApply_RunHeadless(void) {
    printf("[TEST] mm-volume-live-apply: SoH's volume sliders reach MM's sequence players while MM's audio is up, "
           "and nothing while it is down (#1235)\n");

    Saved saved;
    saved.live = MM_Volume_AudioLive();
    for (int i = 0; i < kChannelCount; i++) {
        saved.scale[i] = AudioSeq_GetPortVolumeScale(kChannels[i].player);
        saved.key[i] = CVarGetInteger(kChannels[i].key, INT32_MIN);
    }

    int failures = 0;
    for (int i = 0; i < kChannelCount; i++) {
        CVarSetInteger(kChannels[i].key, kChannels[i].percent);
        AudioSeq_SetPortVolumeScale(kChannels[i].player, kSentinel);
    }

    // Leg 1: MM's audio down. A no-op, reported as one, every sentinel intact.
    MM_Volume_SetAudioLive(0);
    const int downRc = MM_Volume_ReapplyPortScales();
    if (downRc != 0) {
        printf("[TEST] FAIL: with MM's audio down the reapply reported a write (%d); it must be a no-op, because "
               "MM_AudioMgr_Init reads the keys itself when it runs\n",
               downRc);
        failures++;
    }
    for (int i = 0; i < kChannelCount; i++) {
        const float now = AudioSeq_GetPortVolumeScale(kChannels[i].player);
        if (!Near(now, kSentinel)) {
            printf("[TEST] FAIL: with MM's audio down the reapply moved player %u (%s) from %.3f to %.3f\n",
                   (unsigned)kChannels[i].player, kChannels[i].key, kSentinel, now);
            failures++;
        }
    }
    printf("[TEST] leg 1: with MM's audio down the reapply is a no-op (rc %d)\n", downRc);

    // Leg 2: MM's audio up. Each player at its own key's percent / 100.
    MM_Volume_SetAudioLive(1);
    const int upRc = MM_Volume_ReapplyPortScales();
    if (upRc != 1) {
        printf("[TEST] FAIL: with MM's audio up the reapply reported %d, expected 1 (it wrote the five players)\n",
               upRc);
        failures++;
    }
    for (int i = 0; i < kChannelCount; i++) {
        const float want = (float)kChannels[i].percent / 100.0f;
        const float now = AudioSeq_GetPortVolumeScale(kChannels[i].player);
        if (!Near(now, want)) {
            printf("[TEST] FAIL: with MM's audio up player %u holds %.3f, expected %.3f from %s = %d: a slider moved "
                   "after MM's first init does not reach Majora's Mask until a restart\n",
                   (unsigned)kChannels[i].player, now, want, kChannels[i].key, kChannels[i].percent);
            failures++;
        }
    }
    printf("[TEST] leg 2: with MM's audio up the reapply writes the five players from their keys (rc %d)\n", upRc);

    Restore(saved);
    if (failures == 0) {
        printf("[TEST] mm-volume-live-apply: PASS\n");
        return 0;
    }
    printf("[TEST] mm-volume-live-apply: %d failure(s)\n", failures);
    return 1;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
