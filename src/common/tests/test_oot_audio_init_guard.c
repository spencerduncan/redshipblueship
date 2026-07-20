/**
 * OoT audio producer must no-op while its context is uninitialized (#365).
 *
 * WHAT BROKE
 *
 * In single-exe builds the shared audio pump, OTRAudio_Thread
 * (games/oot/soh/OTRGlobals.cpp), dispatches the OoT synth
 * (OoT_AudioMgr_CreateNextAudioBuffer, games/oot/src/code/code_800E4FE0.c)
 * whenever MM's synth is NOT active — i.e. in the `else` of a gate that only
 * checks MM's audio-heap flag. That `else` runs for every MM frame before MM's
 * audio heap comes up, and for the mid-switch window after OoT_Game_Suspend has
 * run OoT_Audio_PreNMI and cleared gAudioContextInitalized. The thread
 * zero-fills its buffer and expects an uninitialized synth to "write nothing",
 * but the OoT producer never honored that: it bumped totalTaskCnt and drove the
 * DMA/load/synth path against a torn-down audio context. PR #362's drain closes
 * the suspend-time use-after-free but leaves this every-MM-frame unguarded
 * consumer.
 *
 * WHAT THIS LOCKS
 *
 * With gAudioContextInitalized == false the producer must be a no-op. The tell
 * is gAudioContext.totalTaskCnt++, the producer's FIRST act — unconditional,
 * ahead of every state-dependent branch (checking the output buffer instead is
 * vacuous: with a zeroed context the synth writes nothing to it anyway). The
 * guard must return before that increment, so a guarded producer leaves the
 * counter untouched and an unguarded one advances it. The counter is read
 * through OoT_AudioMgr_DebugTaskCnt() so this TU needs no AudioContext layout.
 * Robust to the shared audio thread: with the flag false, every producer call
 * (this one and any the thread makes) no-ops, so the counter is stable in the
 * pass case; the unguarded call advances it by at least one in the fail case.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++); the declarations
 * carry C linkage explicitly because the definitions live in OoT C TUs. The
 * producer is declared with int16_t/uint32_t rather than the libultra s16/u32
 * spellings so this TU stays free of the audio umbrella headers — the C-linkage
 * symbol is identical and the parameter types are ABI-identical.
 */

#include <cstdint>
#include <cstdio>

extern "C" {
void OoT_AudioMgr_CreateNextAudioBuffer(int16_t* samples, uint32_t numSamples);
uint32_t OoT_AudioMgr_DebugTaskCnt(void);
extern int32_t gAudioContextInitalized;
}

TestResult Test_OoTAudioInitGuard(void) {
    printf("[TEST] oot-audio-init-guard: OoT synth is a no-op while gAudioContextInitalized == false (#365)\n");

    // The port's default (no game booted) is already false; save/restore anyway
    // so this test cannot perturb a later one that assumes audio is up.
    const int32_t saved = gAudioContextInitalized;
    gAudioContextInitalized = 0; // OoT suspended / not booted: audio context torn down

    int16_t buf[128] = { 0 }; // stereo scratch; a guarded producer never touches it
    const uint32_t before = OoT_AudioMgr_DebugTaskCnt();
    OoT_AudioMgr_CreateNextAudioBuffer(buf, 64);
    const uint32_t after = OoT_AudioMgr_DebugTaskCnt();

    gAudioContextInitalized = saved;

    if (after != before) {
        printf("[TEST] FAIL: producer ran while gAudioContextInitalized == false (totalTaskCnt %u -> %u) — it drove "
               "the synth against a torn-down context instead of no-op silence\n",
               (unsigned)before, (unsigned)after);
        return TEST_FAIL;
    }

    printf("[TEST] PASS: oot-audio-init-guard\n");
    return TEST_PASS;
}
