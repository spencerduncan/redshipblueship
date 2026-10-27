/**
 * @file mm_volume_bridge.h
 * @brief The one way SoH's Settings > Audio sliders reach Majora's Mask's sequence players (#1235).
 *
 * THE DEFECT. Both games read the five volume keys (src/common/cvar_shared_keys.h, RSBS_CVAR_VOLUME_*), but MM reads
 * them in one place only: MM_AudioMgr_Init (games/mm/src/code/audio_thread_manager.c), which runs once per MM
 * bring-up. SoH's slider callbacks call OoT's Audio_SetGameVolume and nothing else, and 2Ship's own callbacks
 * (BenMenu.cpp) are excluded from the single exe. So a volume changed after MM's first init reached MM only after a
 * restart.
 *
 * THE BRIDGE. MM exports two C functions, defined in games/mm/2s2h/MmVolumeSingleExe.cpp; the OoT-side callers
 * (SoH's slider callbacks and the hosted ambience row) reach them through this header and never include an MM
 * header. MM_Volume_ReapplyPortScales does what MM_AudioMgr_Init does for the five keys, with
 * AudioSeq_SetPortVolumeScale, and only while MM's audio is up: GameExports_SingleExe.cpp marks it live right after
 * MM_AudioMgr_Init and dead again in MM_Game_Shutdown. Suspend leaves it live (the audio context survives a suspend),
 * so a change made while OoT runs is waiting in MM's players when MM resumes.
 *
 * Locked ROM-free by MMVolumeLiveApply (games/mm/2s2h/mm_volume_live_apply_test.cpp).
 */

#ifndef RSBS_COMMON_MM_VOLUME_BRIDGE_H
#define RSBS_COMMON_MM_VOLUME_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Reapply Main Music, Sub Music, Sound Effects, Fanfare and Ambience to MM's sequence players from the shared keys.
 * Returns 1 when it wrote them, 0 when MM's audio is not up (a no-op: MM_AudioMgr_Init reads the keys itself when
 * it runs).
 */
int MM_Volume_ReapplyPortScales(void);

/** Whether MM's audio is up. 1 from MM_AudioMgr_Init until MM_Game_Shutdown; a test drives it to cover both legs. */
void MM_Volume_SetAudioLive(int live);
int MM_Volume_AudioLive(void);

#ifdef __cplusplus
}
#endif

#endif // RSBS_COMMON_MM_VOLUME_BRIDGE_H
