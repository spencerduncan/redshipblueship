/**
 * @file sequence.c
 *
 * This file implements a set of high-level audio sequence commands that allow sequences to be modified in real-time.
 * These commands are intended to interface external to the audio library.
 *
 * These commands are generated using `AudioSeq_QueueSeqCmd`, and a user-friendly interface for this function
 * can be found in `seqcmd.h`
 *
 * These commands change sequences by generating internal audio commands `AudioThread_QueueCmd` which allows these
 * sequence requests to be passed onto the audio thread. It is worth noting all functions in this file are
 * called from the graph thread.
 *
 * These commands are not to be confused with the sequence instructions used by the sequences themselves
 * which are a midi-based scripting language.
 *
 * Nor are these commands to be confused with the internal audio commands used to transfer requests from
 * the graph thread to the audio thread.
 */
#include "global.h"
#include "2s2h/Enhancements/Audio/AudioEditor.h"

// Direct audio command (skips the queueing system)
#define SEQCMD_SET_SEQPLAYER_VOLUME_NOW(seqPlayerIndex, duration, volume)                          \
    AudioSeq_ProcessSeqCmd((SEQCMD_OP_SET_SEQPLAYER_VOLUME << 28) | ((u8)(seqPlayerIndex) << 24) | \
                           ((u8)(duration) << 16) | ((u8)((volume)*127.0f)));

u8 sSeqCmdWritePos = 0;
u8 sSeqCmdReadPos = 0;
u8 sStartSeqDisabled = 0;
u8 sSoundModeList[] = {
    SOUNDMODE_STEREO, SOUNDMODE_HEADSET, SOUNDMODE_SURROUND_EXTERNAL, SOUNDMODE_MONO, SOUNDMODE_SURROUND,
};
u8 MM_gAudioSpecId = 0;
u8 gAudioHeapResetState = AUDIO_HEAP_RESET_STATE_NONE;
u32 sResetAudioHeapSeqCmd = 0;
// 2S2H [Custom Audio] seqId and seqArgs updated to use u16 instead of u8
void AudioSeq_StartSequence(u8 seqPlayerIndex, u16 seqId, u16 seqArgs, u16 fadeInDuration) {
    u8 channelIndex;
    u16 skipTicks;
    s32 pad;

    if (!sStartSeqDisabled || (seqPlayerIndex == SEQ_PLAYER_SFX)) {
        seqArgs &= 0x7F;
        if (seqArgs == 0x7F) {
            // `fadeInDuration` interpreted as seconds, 60 is refresh rate and does not account for PAL
            skipTicks = (fadeInDuration >> 3) * 60 * gAudioCtx.audioBufferParameters.updatesPerFrame;
            AUDIOCMD_GLOBAL_INIT_SEQPLAYER_SKIP_TICKS(seqPlayerIndex, seqId, skipTicks);
        } else {
            // `fadeInDuration` interpreted as 1/30th of a second, does not account for change in refresh rate for PAL
            AUDIOCMD_GLOBAL_INIT_SEQPLAYER(seqPlayerIndex, seqId,
                                           (fadeInDuration * (u16)gAudioCtx.audioBufferParameters.updatesPerFrame) / 4);
        }

        MM_gActiveSeqs[seqPlayerIndex].seqId = seqId | (seqArgs << 8);
        MM_gActiveSeqs[seqPlayerIndex].prevSeqId = seqId | (seqArgs << 8);
        MM_gActiveSeqs[seqPlayerIndex].isSeqPlayerInit = true;

        if (MM_gActiveSeqs[seqPlayerIndex].volCur != 1.0f) {
            AUDIOCMD_SEQPLAYER_FADE_VOLUME_SCALE(seqPlayerIndex, MM_gActiveSeqs[seqPlayerIndex].volCur);
        }

        MM_gActiveSeqs[seqPlayerIndex].tempoTimer = 0;
        MM_gActiveSeqs[seqPlayerIndex].tempoOriginal = 0;
        MM_gActiveSeqs[seqPlayerIndex].tempoCmd = 0;

        for (channelIndex = 0; channelIndex < SEQ_NUM_CHANNELS; channelIndex++) {
            MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volCur = 1.0f;
            MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volTimer = 0;
            MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].freqScaleCur = 1.0f;
            MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].freqScaleTimer = 0;
        }

        MM_gActiveSeqs[seqPlayerIndex].freqScaleChannelFlags = 0;
        MM_gActiveSeqs[seqPlayerIndex].volChannelFlags = 0;
    }
}

void AudioSeq_StopSequence(u8 seqPlayerIndex, u16 fadeOutDuration) {
    AUDIOCMD_GLOBAL_DISABLE_SEQPLAYER(seqPlayerIndex,
                                      (fadeOutDuration * (u16)gAudioCtx.audioBufferParameters.updatesPerFrame) / 4);
    MM_gActiveSeqs[seqPlayerIndex].seqId = NA_BGM_DISABLED;
}

// 2S2H [Custom Audio] Updated seqId to be 16 bit.
void AudioSeq_ProcessSeqCmd(u32 cmd) {
    s32 priority;
    s32 channelMaskEnable;
    u16 channelMaskDisable;
    u16 fadeTimer;
    u16 val;
    u8 oldSpecId;
    u8 specId;
    u8 fadeReverb;
    u8 op;
    u8 subOp;
    u8 seqPlayerIndex;
    u16 seqId;
    u8 seqArgs;
    u8 found;
    u8 ioPort;
    u8 duration;
    u8 channelIndex;
    u8 i;
    f32 freqScaleTarget;
    u32 outNumFonts;

    op = cmd >> 28;
    seqPlayerIndex = (cmd & SEQCMD_SEQPLAYER_MASK) >> 24;

    switch (op) {
        case SEQCMD_OP_PLAY_SEQUENCE:
            // Play a new sequence
            seqId = cmd & SEQCMD_SEQID_MASK_16;
            seqArgs = (cmd & 0xFF00) >> 8;
            // `fadeTimer` is only shifted 13 bits instead of 16 bits.
            // `fadeTimer` continues to be scaled in `AudioSeq_StartSequence`
            fadeTimer = (cmd & 0xFF0000) >> 13;
            if (!MM_gActiveSeqs[seqPlayerIndex].isWaitingForFonts && !sStartSeqDisabled) {
                // BEN: The code block to later load a sequence doesn't exist in OOT.
                // BEN: The thing preventing this code block from running is actually the fact that we killed the audio
                // tables. BEN: This delayed loading method actually utilizes some functions that OOT is no longer
                // utilizing. BEN: The short of it ends up being the short circuit here is fine since it works and the
                // stuff is loaded anyway.
                if (seqArgs < 0x80 || 1) {
                    AudioSeq_StartSequence(seqPlayerIndex, seqId, seqArgs, fadeTimer);
                } else {
                    // Store the cmd to be called again once the fonts are loaded
                    // but changes the command so that next time, the (seqArgs < 0x80) case is taken
                    MM_gActiveSeqs[seqPlayerIndex].startAsyncSeqCmd =
                        (cmd & ~(SEQ_FLAG_ASYNC | SEQCMD_ASYNC_ACTIVE)) + SEQCMD_ASYNC_ACTIVE;
                    MM_gActiveSeqs[seqPlayerIndex].isWaitingForFonts = true;
                    u8* fontBuff[16];
                    u8* prevFontBuff[16];
                    u8* font = AudioThread_GetFontsForSequence(seqId, &outNumFonts, fontBuff);
                    MM_gActiveSeqs[seqPlayerIndex].fontId = *font;
                    AudioSeq_StopSequence(seqPlayerIndex, 1);

                    if (MM_gActiveSeqs[seqPlayerIndex].prevSeqId != NA_BGM_DISABLED) {
                        if (*AudioThread_GetFontsForSequence(seqId, &outNumFonts, fontBuff) !=
                            *AudioThread_GetFontsForSequence(MM_gActiveSeqs[seqPlayerIndex].prevSeqId & 0xFF, &outNumFonts,
                                                             prevFontBuff)) {
                            // Discard Seq Fonts
                            AUDIOCMD_GLOBAL_DISCARD_SEQ_FONTS((s32)seqId);
                        }
                    }

                    AUDIOCMD_GLOBAL_ASYNC_LOAD_FONT(*AudioThread_GetFontsForSequence(seqId, &outNumFonts, fontBuff),
                                                    (u8)((seqPlayerIndex + 1) & 0xFF));
                }
            }
            break;

        case SEQCMD_OP_STOP_SEQUENCE:
            // Stop a sequence and disable the sequence player
            fadeTimer = (cmd & 0xFF0000) >> 13;
            AudioSeq_StopSequence(seqPlayerIndex, fadeTimer);
            break;

        case SEQCMD_OP_QUEUE_SEQUENCE:
            // Queue a sequence into `sSeqRequests`
            seqId = cmd & SEQCMD_SEQID_MASK;
            seqArgs = (cmd & 0xFF00) >> 8;
            fadeTimer = (cmd & 0xFF0000) >> 13;
            priority = seqArgs;

            // Checks if the requested sequence is first in the list of requests
            // If it is already queued and first in the list, then play the sequence immediately
            for (i = 0; i < MM_sNumSeqRequests[seqPlayerIndex]; i++) {
                if (sSeqRequests[seqPlayerIndex][i].seqId == seqId) {
                    if (i == 0) {
                        AudioSeq_StartSequence(seqPlayerIndex, seqId, seqArgs, fadeTimer);
                    }
                    return;
                }
            }

            // Searches the sequence requests for the first request that does not have a higher priority
            // than the current incoming request
            found = MM_sNumSeqRequests[seqPlayerIndex];
            for (i = 0; i < MM_sNumSeqRequests[seqPlayerIndex]; i++) {
                if (priority >= sSeqRequests[seqPlayerIndex][i].priority) {
                    found = i;
                    i = MM_sNumSeqRequests[seqPlayerIndex]; // "break;"
                }
            }

            // Check if the queue is full
            if (MM_sNumSeqRequests[seqPlayerIndex] < ARRAY_COUNT(sSeqRequests[seqPlayerIndex])) {
                MM_sNumSeqRequests[seqPlayerIndex]++;
            }

            for (i = MM_sNumSeqRequests[seqPlayerIndex] - 1; i != found; i--) {
                // Move all requests of lower priority backwards 1 place in the queue
                // If the queue is full, overwrite the entry with the lowest priority
                sSeqRequests[seqPlayerIndex][i].priority = sSeqRequests[seqPlayerIndex][i - 1].priority;
                sSeqRequests[seqPlayerIndex][i].seqId = sSeqRequests[seqPlayerIndex][i - 1].seqId;
            }

            // Fill the newly freed space in the queue with the new request
            sSeqRequests[seqPlayerIndex][found].priority = seqArgs;
            sSeqRequests[seqPlayerIndex][found].seqId = seqId;

            // The sequence is first in queue, so start playing.
            if (found == 0) {
                AudioSeq_StartSequence(seqPlayerIndex, seqId, seqArgs, fadeTimer);
            }
            break;

        case SEQCMD_OP_UNQUEUE_SEQUENCE:
            // Unqueue sequence
            fadeTimer = (cmd & 0xFF0000) >> 13;

            found = MM_sNumSeqRequests[seqPlayerIndex];
            for (i = 0; i < MM_sNumSeqRequests[seqPlayerIndex]; i++) {
                seqId = cmd & SEQCMD_SEQID_MASK;
                if (sSeqRequests[seqPlayerIndex][i].seqId == seqId) {
                    found = i;
                    i = MM_sNumSeqRequests[seqPlayerIndex]; // "break;"
                }
            }

            if (found != MM_sNumSeqRequests[seqPlayerIndex]) {
                // Move all requests of lower priority forward 1 place in the queue
                for (i = found; i < MM_sNumSeqRequests[seqPlayerIndex] - 1; i++) {
                    sSeqRequests[seqPlayerIndex][i].priority = sSeqRequests[seqPlayerIndex][i + 1].priority;
                    sSeqRequests[seqPlayerIndex][i].seqId = sSeqRequests[seqPlayerIndex][i + 1].seqId;
                }
                MM_sNumSeqRequests[seqPlayerIndex]--;
            }

            // If the sequence was first in queue (it is currently playing),
            // Then stop the sequence and play the next sequence in the queue.
            if (found == 0) {
                AudioSeq_StopSequence(seqPlayerIndex, fadeTimer);
                if (MM_sNumSeqRequests[seqPlayerIndex] != 0) {
                    AudioSeq_StartSequence(seqPlayerIndex, sSeqRequests[seqPlayerIndex][0].seqId,
                                           sSeqRequests[seqPlayerIndex][0].priority, fadeTimer);
                }
            }
            break;

        case SEQCMD_OP_SET_SEQPLAYER_VOLUME:
            // Transition volume to a target volume for an entire player
            duration = (cmd & 0xFF0000) >> 15;
            val = cmd & 0xFF;
            if (duration == 0) {
                duration++;
            }
            // Volume is scaled relative to 127
            MM_gActiveSeqs[seqPlayerIndex].volTarget = (f32)val / 127.0f;
            if (MM_gActiveSeqs[seqPlayerIndex].volCur != MM_gActiveSeqs[seqPlayerIndex].volTarget) {
                MM_gActiveSeqs[seqPlayerIndex].volStep =
                    (MM_gActiveSeqs[seqPlayerIndex].volCur - MM_gActiveSeqs[seqPlayerIndex].volTarget) / (f32)duration;
                MM_gActiveSeqs[seqPlayerIndex].volTimer = duration;
            }
            break;

        case SEQCMD_OP_SET_SEQPLAYER_FREQ:
            // Transition freq scale to a target freq for all channels
            duration = (cmd & 0xFF0000) >> 15;
            val = cmd & 0xFFFF;
            if (duration == 0) {
                duration++;
            }
            // Frequency is scaled relative to 1000
            freqScaleTarget = (f32)val / 1000.0f;
            for (i = 0; i < SEQ_NUM_CHANNELS; i++) {
                MM_gActiveSeqs[seqPlayerIndex].channelData[i].freqScaleTarget = freqScaleTarget;
                MM_gActiveSeqs[seqPlayerIndex].channelData[i].freqScaleTimer = duration;
                MM_gActiveSeqs[seqPlayerIndex].channelData[i].freqScaleStep =
                    (MM_gActiveSeqs[seqPlayerIndex].channelData[i].freqScaleCur - freqScaleTarget) / (f32)duration;
            }
            MM_gActiveSeqs[seqPlayerIndex].freqScaleChannelFlags = 0xFFFF;
            break;

        case SEQCMD_OP_SET_CHANNEL_FREQ:
            // Transition freq scale to a target for a specific channel
            duration = (cmd & 0xFF0000) >> 15;
            channelIndex = (cmd & 0xF000) >> 12;
            val = cmd & 0xFFF;
            if (duration == 0) {
                duration++;
            }
            // Frequency is scaled relative to 1000
            freqScaleTarget = (f32)val / 1000.0f;
            MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].freqScaleTarget = freqScaleTarget;
            MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].freqScaleStep =
                (MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].freqScaleCur - freqScaleTarget) / (f32)duration;
            MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].freqScaleTimer = duration;
            MM_gActiveSeqs[seqPlayerIndex].freqScaleChannelFlags |= 1 << channelIndex;
            break;

        case SEQCMD_OP_SET_CHANNEL_VOLUME:
            // Transition volume to a target volume for a specific channel
            duration = (cmd & 0xFF0000) >> 15;
            channelIndex = (cmd & 0xF00) >> 8;
            val = cmd & 0xFF;
            if (duration == 0) {
                duration++;
            }
            // Volume is scaled relative to 127
            MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volTarget = (f32)val / 127.0f;
            if (MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volCur !=
                MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volTarget) {
                MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volStep =
                    (MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volCur -
                     MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volTarget) /
                    (f32)duration;
                MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volTimer = duration;
                MM_gActiveSeqs[seqPlayerIndex].volChannelFlags |= 1 << channelIndex;
            }
            break;

        case SEQCMD_OP_SET_SEQPLAYER_IO:
            // Set global io port
            ioPort = (cmd & 0xFF0000) >> 16;
            val = cmd & 0xFF;
            AUDIOCMD_SEQPLAYER_SET_IO(seqPlayerIndex, ioPort, val);
            break;

        case SEQCMD_OP_SET_CHANNEL_IO:
            // Set io port if channel masked
            channelIndex = (cmd & 0xF00) >> 8;
            ioPort = (cmd & 0xFF0000) >> 16;
            val = cmd & 0xFF;
            if (!(MM_gActiveSeqs[seqPlayerIndex].channelPortMask & (1 << channelIndex))) {
                AUDIOCMD_CHANNEL_SET_IO(seqPlayerIndex, channelIndex, ioPort, val);
            }
            break;

        case SEQCMD_OP_SET_CHANNEL_IO_DISABLE_MASK:
            // Disable channel io specifically for `SEQCMD_OP_SET_CHANNEL_IO`.
            // This can be bypassed by setting channel io through using `AUDIOCMD_CHANNEL_SET_IO` directly.
            // This is accomplished by setting a channel mask.
            MM_gActiveSeqs[seqPlayerIndex].channelPortMask = cmd & 0xFFFF;
            break;

        case SEQCMD_OP_SET_CHANNEL_DISABLE_MASK:
            // Disable or Reenable channels

            // Disable channels
            channelMaskDisable = cmd & 0xFFFF;
            if (channelMaskDisable != 0) {
                // Apply channel mask `channelMaskDisable`
                AUDIOCMD_GLOBAL_SET_CHANNEL_MASK(seqPlayerIndex, channelMaskDisable);
                // Disable channels
                AUDIOCMD_CHANNEL_SET_MUTE(seqPlayerIndex, AUDIOCMD_ALL_CHANNELS, true);
            }

            // Reenable channels
            channelMaskEnable = channelMaskDisable ^ 0xFFFF;
            if (channelMaskEnable != 0) {
                // Apply channel mask `channelMaskEnable`
                AUDIOCMD_GLOBAL_SET_CHANNEL_MASK(seqPlayerIndex, channelMaskEnable);
                // Enable channels
                AUDIOCMD_CHANNEL_SET_MUTE(seqPlayerIndex, AUDIOCMD_ALL_CHANNELS, false);
            }
            break;

        case SEQCMD_OP_TEMPO_CMD:
            // Update a tempo using a sub-command system.
            // Stores the cmd for processing elsewhere.
            MM_gActiveSeqs[seqPlayerIndex].tempoCmd = cmd;
            break;

        case SEQCMD_OP_SETUP_CMD:
            // Queue a sub-command to execute once the sequence is finished playing
            subOp = (cmd & 0xF00000) >> 20;
            if (subOp != SEQCMD_SUB_OP_SETUP_RESET_SETUP_CMDS) {
                // Ensure the maximum number of setup commands is not exceeded
                if (MM_gActiveSeqs[seqPlayerIndex].setupCmdNum < (ARRAY_COUNT(MM_gActiveSeqs[seqPlayerIndex].setupCmd) - 1)) {
                    found = MM_gActiveSeqs[seqPlayerIndex].setupCmdNum++;
                    if (found < ARRAY_COUNT(MM_gActiveSeqs[seqPlayerIndex].setupCmd)) {
                        MM_gActiveSeqs[seqPlayerIndex].setupCmd[found] = cmd;
                        // Adds a delay of 2 frames before executing any setup commands.
                        // This allows setup commands to be requested along with a new sequence on a seqPlayerIndex.
                        // This 2 frame delay ensures the player is enabled before its state is checked for
                        // the purpose of deciding if the setup commands should be run.
                        // Otherwise, the setup commands will be executed before the sequence starts,
                        // when the player is still disabled, instead of when the newly played sequence ends.
                        MM_gActiveSeqs[seqPlayerIndex].setupCmdTimer = 2;
                    }
                }
            } else {
                // `SEQCMD_SUB_OP_SETUP_RESET_SETUP_CMDS`
                // Discard all setup command requests on `seqPlayerIndex`
                MM_gActiveSeqs[seqPlayerIndex].setupCmdNum = 0;
            }
            break;

        case SEQCMD_OP_GLOBAL_CMD:
            // Apply a command that applies to all sequence players
            subOp = (cmd & 0xF00) >> 8;
            val = cmd & 0xFF;
            switch (subOp) {
                case SEQCMD_SUB_OP_GLOBAL_SET_SOUND_MODE:
                    // Set sound mode
                    AUDIOCMD_GLOBAL_SET_SOUND_MODE(sSoundModeList[val]);
                    break;

                case SEQCMD_SUB_OP_GLOBAL_DISABLE_NEW_SEQUENCES:
                    // Set sequence disabled in (sStartSeqDisabled & 1) bit
                    sStartSeqDisabled = (sStartSeqDisabled & (u8)~1) | (u8)(val & 1);
                    break;

                case SEQCMD_SUB_OP_GLOBAL_DISABLE_NEW_SEQUENCES_2:
                    // Set sequence disabled in (sStartSeqDisabled & 2) bit
                    sStartSeqDisabled = (sStartSeqDisabled & (u8)~2) | (u8)((val & 1) << 1);
                    break;
            }
            break;

        case SEQCMD_OP_RESET_AUDIO_HEAP:
            // Resets the audio heap based on the audio specifications, audio mode, and sfx channel layout
            specId = cmd & 0xFF;
            fadeReverb = (cmd & 0xFF0000) >> 16;
            if (fadeReverb == 0) {
                MM_gSfxChannelLayout = (cmd & 0xFF00) >> 8;
                oldSpecId = MM_gAudioSpecId;
                MM_gAudioSpecId = specId;
                AudioThread_ResetAudioHeap(specId);
                Audio_ResetForAudioHeapStep1(oldSpecId);
                AUDIOCMD_GLOBAL_STOP_AUDIOCMDS();
            } else {
                sResetAudioHeapSeqCmd = cmd;
                sResetAudioHeapFadeReverbVolume = 0x7FFF;
                sResetAudioHeapTimer = 20;
                sResetAudioHeapFadeReverbVolumeStep = 0x666;
            }
            break;
    }
}

/**
 * Add the sequence cmd to the `MM_sAudioSeqCmds` queue
 */
static const uint8_t sClockTownDaySeqIds[4] = {
    NA_BGM_CLOCK_TOWN_DAY_1,
    NA_BGM_CLOCK_TOWN_DAY_2,
    NA_BGM_CLOCK_TOWN_DAY_3,
    // Through glitches and the save editor it is possible to reach the 4th day.
    NA_BGM_CLOCK_TOWN_DAY_1,
};
void AudioSeq_QueueSeqCmd(u32 cmd) {
    // 2S2H [Port] Allow loading custom sequences and use 16 bit seqId
    u8 op = cmd >> 28;
    // Ship had a check for op 12 but it doesn't seem like the seqId is set there
    if (op == 0 || op == 2) {
        u16 seqId = cmd & SEQCMD_SEQID_MASK_16;
        if (seqId == NA_BGM_CLOCK_TOWN_MAIN_SEQUENCE) {
            // Clock town uses one sequence id for all 3 songs. We need to manually figure out which day it is
            seqId = sClockTownDaySeqIds[CURRENT_DAY - 1];
            // Don't update the command as that will break the morning sequence.
        }
        u8 playerIdx = (cmd & 0xF000000) >> 24;
        u16 newSeqId = AudioEditor_GetReplacementSeq(seqId);
        gAudioCtx.seqReplaced[playerIdx] = (seqId != newSeqId);
        gAudioCtx.seqToPlay[playerIdx] = newSeqId;
        // Don't overwrite the seqId we just set for Clock Town
        if (seqId != sClockTownDaySeqIds[CURRENT_DAY - 1]) {
            cmd |= (seqId & SEQCMD_SEQID_MASK_16);
        }
    }
    MM_sAudioSeqCmds[sSeqCmdWritePos++] = cmd;
}

void AudioSeq_ProcessSeqCmds(void) {
    while (sSeqCmdWritePos != sSeqCmdReadPos) {
        AudioSeq_ProcessSeqCmd(MM_sAudioSeqCmds[sSeqCmdReadPos++]);
    }
}

u16 AudioSeq_GetActiveSeqId(u8 seqPlayerIndex) {
    if (MM_gActiveSeqs[seqPlayerIndex].isWaitingForFonts == true) {
        return MM_gActiveSeqs[seqPlayerIndex].startAsyncSeqCmd & SEQCMD_SEQID_MASK;
    }

    if (MM_gActiveSeqs[seqPlayerIndex].seqId != NA_BGM_DISABLED) {
        return MM_gActiveSeqs[seqPlayerIndex].seqId;
    }

    return NA_BGM_DISABLED;
}

s32 AudioSeq_IsSeqCmdNotQueued(u32 cmdVal, u32 cmdMask) {
    u8 i;

    for (i = sSeqCmdReadPos; i != sSeqCmdWritePos; i++) {
        if ((MM_sAudioSeqCmds[i] & cmdMask) == cmdVal) {
            return false;
        }
    }

    return true;
}

// Unused
void AudioSeq_ResetSequenceRequests(u8 seqPlayerIndex) {
    MM_sNumSeqRequests[seqPlayerIndex] = 0;
}

/**
 * Check if the setup command is queued. If it is, then replace the command
 * with `SEQCMD_SUB_OP_SETUP_RESTORE_SEQPLAYER_VOLUME`.
 * Unused
 */
void AudioSeq_ReplaceSeqCmdSetupOpVolRestore(u8 seqPlayerIndex, u8 setupOpDisabled) {
    u8 i;

    for (i = 0; i < MM_gActiveSeqs[seqPlayerIndex].setupCmdNum; i++) {
        u8 setupOp = (MM_gActiveSeqs[seqPlayerIndex].setupCmd[i] & 0xF00000) >> 20;

        if (setupOp == setupOpDisabled) {
            MM_gActiveSeqs[seqPlayerIndex].setupCmd[i] = 0xFF000000;
        }
    }
}

void AudioSeq_SetVolumeScale(u8 seqPlayerIndex, u8 scaleIndex, u8 targetVol, u8 volFadeTimer) {
    f32 volScale;
    u8 i;

    MM_gActiveSeqs[seqPlayerIndex].volScales[scaleIndex] = targetVol & 0x7F;

    if (volFadeTimer != 0) {
        MM_gActiveSeqs[seqPlayerIndex].fadeVolUpdate = true;
        MM_gActiveSeqs[seqPlayerIndex].volFadeTimer = volFadeTimer;
    } else {
        for (i = 0, volScale = 1.0f; i < VOL_SCALE_INDEX_MAX; i++) {
            volScale *= MM_gActiveSeqs[seqPlayerIndex].volScales[i] / 127.0f;
        }

        SEQCMD_SET_SEQPLAYER_VOLUME_NOW(seqPlayerIndex, volFadeTimer, volScale);
    }
}

/**
 * Update different commands and requests for active sequences
 */
void AudioSeq_UpdateActiveSequences(void) {
    u32 tempoCmd;
    u16 tempoPrev;
    u16 seqId;
    u16 channelMask;
    u16 tempoTarget;
    u8 setupOp;
    u8 targetSeqPlayerIndex;
    u8 setupVal2;
    u8 setupVal1;
    u8 tempoOp;
    s32 pad[2];
    u32 retMsg;
    f32 volume;
    u8 tempoTimer;
    u8 seqPlayerIndex;
    u8 j;
    u8 channelIndex;

    for (seqPlayerIndex = 0; seqPlayerIndex < SEQ_PLAYER_MAX; seqPlayerIndex++) {

        // The seqPlayer has finished initializing and is currently playing the active sequences
        if (MM_gActiveSeqs[seqPlayerIndex].isSeqPlayerInit && gAudioCtx.seqPlayers[seqPlayerIndex].enabled) {
            MM_gActiveSeqs[seqPlayerIndex].isSeqPlayerInit = false;
        }

        // The seqPlayer is no longer playing the active sequences
        if ((AudioSeq_GetActiveSeqId(seqPlayerIndex) != NA_BGM_DISABLED) &&
            !gAudioCtx.seqPlayers[seqPlayerIndex].enabled && (!MM_gActiveSeqs[seqPlayerIndex].isSeqPlayerInit)) {
            MM_gActiveSeqs[seqPlayerIndex].seqId = NA_BGM_DISABLED;
        }

        // Check if the requested sequences is waiting for fonts to load
        if (MM_gActiveSeqs[seqPlayerIndex].isWaitingForFonts) {
            switch ((s32)AudioThread_GetExternalLoadQueueMsg(&retMsg)) {
                case SEQ_PLAYER_BGM_MAIN + 1:
                case SEQ_PLAYER_FANFARE + 1:
                case SEQ_PLAYER_SFX + 1:
                case SEQ_PLAYER_BGM_SUB + 1:
                case SEQ_PLAYER_AMBIENCE + 1:
                    // The fonts have been loaded successfully.
                    MM_gActiveSeqs[seqPlayerIndex].isWaitingForFonts = false;
                    // Queue the same command that was stored previously, but without the 0x8000
                    AudioSeq_ProcessSeqCmd(MM_gActiveSeqs[seqPlayerIndex].startAsyncSeqCmd);
                    break;
                case 0xFF:
                    // There was an error in loading the fonts
                    MM_gActiveSeqs[seqPlayerIndex].isWaitingForFonts = false;
                    break;
            }
        }

        // Update global volume
        if (MM_gActiveSeqs[seqPlayerIndex].fadeVolUpdate) {
            volume = 1.0f;
            for (j = 0; j < VOL_SCALE_INDEX_MAX; j++) {
                volume *= (MM_gActiveSeqs[seqPlayerIndex].volScales[j] / 127.0f);
            }

            SEQCMD_SET_SEQPLAYER_VOLUME((u8)(seqPlayerIndex + (SEQCMD_ASYNC_ACTIVE >> 24)),
                                        MM_gActiveSeqs[seqPlayerIndex].volFadeTimer, (u8)(volume * 127.0f));
            MM_gActiveSeqs[seqPlayerIndex].fadeVolUpdate = false;
        }

        if (MM_gActiveSeqs[seqPlayerIndex].volTimer != 0) {
            MM_gActiveSeqs[seqPlayerIndex].volTimer--;

            if (MM_gActiveSeqs[seqPlayerIndex].volTimer != 0) {
                MM_gActiveSeqs[seqPlayerIndex].volCur -= MM_gActiveSeqs[seqPlayerIndex].volStep;
            } else {
                MM_gActiveSeqs[seqPlayerIndex].volCur = MM_gActiveSeqs[seqPlayerIndex].volTarget;
            }

            AUDIOCMD_SEQPLAYER_FADE_VOLUME_SCALE(seqPlayerIndex, MM_gActiveSeqs[seqPlayerIndex].volCur);
        }

        // Process tempo
        if (MM_gActiveSeqs[seqPlayerIndex].tempoCmd != 0) {
            tempoCmd = MM_gActiveSeqs[seqPlayerIndex].tempoCmd;
            tempoTimer = (tempoCmd & 0xFF0000) >> 15;
            tempoTarget = tempoCmd & 0xFFF;
            if (tempoTimer == 0) {
                tempoTimer++;
            }

            // Process tempo commands
            if (gAudioCtx.seqPlayers[seqPlayerIndex].enabled) {
                tempoPrev = gAudioCtx.seqPlayers[seqPlayerIndex].tempo / TATUMS_PER_BEAT;
                tempoOp = (tempoCmd & 0xF000) >> 12;
                switch (tempoOp) {
                    case SEQCMD_SUB_OP_TEMPO_SPEED_UP:
                        // Speed up tempo by `tempoTarget` amount
                        tempoTarget += tempoPrev;
                        break;

                    case SEQCMD_SUB_OP_TEMPO_SLOW_DOWN:
                        // Slow down tempo by `tempoTarget` amount
                        if (tempoTarget < tempoPrev) {
                            tempoTarget = tempoPrev - tempoTarget;
                        }
                        break;

                    case SEQCMD_SUB_OP_TEMPO_SCALE:
                        // Scale tempo by a multiplicative factor
                        tempoTarget = tempoPrev * (tempoTarget / 100.0f);
                        break;

                    case SEQCMD_SUB_OP_TEMPO_RESET:
                        // Reset tempo to original tempo
                        tempoTarget = (MM_gActiveSeqs[seqPlayerIndex].tempoOriginal != 0)
                                          ? MM_gActiveSeqs[seqPlayerIndex].tempoOriginal
                                          : tempoPrev;
                        break;

                    default: // `SEQCMD_SUB_OP_TEMPO_SET`
                        // `tempoTarget` is the new tempo
                        break;
                }

                if (MM_gActiveSeqs[seqPlayerIndex].tempoOriginal == 0) {
                    MM_gActiveSeqs[seqPlayerIndex].tempoOriginal = tempoPrev;
                }

                MM_gActiveSeqs[seqPlayerIndex].tempoTarget = tempoTarget;
                MM_gActiveSeqs[seqPlayerIndex].tempoCur = gAudioCtx.seqPlayers[seqPlayerIndex].tempo / 0x30;
                MM_gActiveSeqs[seqPlayerIndex].tempoStep =
                    (MM_gActiveSeqs[seqPlayerIndex].tempoCur - MM_gActiveSeqs[seqPlayerIndex].tempoTarget) / tempoTimer;
                MM_gActiveSeqs[seqPlayerIndex].tempoTimer = tempoTimer;
                MM_gActiveSeqs[seqPlayerIndex].tempoCmd = 0;
            }
        }

        // Step tempo to target
        if (MM_gActiveSeqs[seqPlayerIndex].tempoTimer != 0) {
            MM_gActiveSeqs[seqPlayerIndex].tempoTimer--;
            if (MM_gActiveSeqs[seqPlayerIndex].tempoTimer != 0) {
                MM_gActiveSeqs[seqPlayerIndex].tempoCur -= MM_gActiveSeqs[seqPlayerIndex].tempoStep;
            } else {
                MM_gActiveSeqs[seqPlayerIndex].tempoCur = MM_gActiveSeqs[seqPlayerIndex].tempoTarget;
            }

            AUDIOCMD_SEQPLAYER_SET_TEMPO(seqPlayerIndex, MM_gActiveSeqs[seqPlayerIndex].tempoCur);
        }

        // Update channel volumes
        if (MM_gActiveSeqs[seqPlayerIndex].volChannelFlags != 0) {
            for (channelIndex = 0; channelIndex < SEQ_NUM_CHANNELS; channelIndex++) {
                if (MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volTimer != 0) {
                    MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volTimer--;
                    if (MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volTimer != 0) {
                        MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volCur -=
                            MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volStep;
                    } else {
                        MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volCur =
                            MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volTarget;
                        MM_gActiveSeqs[seqPlayerIndex].volChannelFlags ^= (1 << channelIndex);
                    }

                    AUDIOCMD_CHANNEL_SET_VOL_SCALE(seqPlayerIndex, channelIndex,
                                                   MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].volCur);
                }
            }
        }

        // Update frequencies
        if (MM_gActiveSeqs[seqPlayerIndex].freqScaleChannelFlags != 0) {
            for (channelIndex = 0; channelIndex < SEQ_NUM_CHANNELS; channelIndex++) {
                if (MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].freqScaleTimer != 0) {
                    MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].freqScaleTimer--;
                    if (MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].freqScaleTimer != 0) {
                        MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].freqScaleCur -=
                            MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].freqScaleStep;
                    } else {
                        MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].freqScaleCur =
                            MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].freqScaleTarget;
                        MM_gActiveSeqs[seqPlayerIndex].freqScaleChannelFlags ^= (1 << channelIndex);
                    }

                    AUDIOCMD_CHANNEL_SET_FREQ_SCALE(seqPlayerIndex, channelIndex,
                                                    MM_gActiveSeqs[seqPlayerIndex].channelData[channelIndex].freqScaleCur);
                }
            }
        }

        // Process setup commands
        if (MM_gActiveSeqs[seqPlayerIndex].setupCmdNum != 0) {
            // If there is a SeqCmd to reset the audio heap queued, then drop all setup commands
            if (!AudioSeq_IsSeqCmdNotQueued(SEQCMD_OP_RESET_AUDIO_HEAP << 28, SEQCMD_OP_MASK)) {
                MM_gActiveSeqs[seqPlayerIndex].setupCmdNum = 0;
                break;
            }

            // Only process setup commands once the timer reaches MM_zero
            if (MM_gActiveSeqs[seqPlayerIndex].setupCmdTimer != 0) {
                MM_gActiveSeqs[seqPlayerIndex].setupCmdTimer--;
                continue;
            }

            // Only process setup commands if `seqPlayerIndex` if no longer playing
            // i.e. the `seqPlayer` is no longer enabled
            if (gAudioCtx.seqPlayers[seqPlayerIndex].enabled) {
                continue;
            }

            for (j = 0; j < MM_gActiveSeqs[seqPlayerIndex].setupCmdNum; j++) {
                setupOp = (MM_gActiveSeqs[seqPlayerIndex].setupCmd[j] & 0xF00000) >> 20;
                targetSeqPlayerIndex = (MM_gActiveSeqs[seqPlayerIndex].setupCmd[j] & 0xF0000) >> 16;
                setupVal2 = (MM_gActiveSeqs[seqPlayerIndex].setupCmd[j] & 0xFF00) >> 8;
                setupVal1 = MM_gActiveSeqs[seqPlayerIndex].setupCmd[j] & 0xFF;

                switch (setupOp) {
                    case SEQCMD_SUB_OP_SETUP_RESTORE_SEQPLAYER_VOLUME:
                        // Restore `targetSeqPlayerIndex` volume back to normal levels
                        AudioSeq_SetVolumeScale(targetSeqPlayerIndex, VOL_SCALE_INDEX_FANFARE, 0x7F, setupVal1);
                        break;

                    case SEQCMD_SUB_OP_SETUP_RESTORE_SEQPLAYER_VOLUME_IF_QUEUED:
                        // Restore `targetSeqPlayerIndex` volume back to normal levels,
                        // but only if the number of sequence queue requests from `sSeqRequests`
                        // exactly matches the argument to the command
                        if (setupVal1 == MM_sNumSeqRequests[seqPlayerIndex]) {
                            AudioSeq_SetVolumeScale(targetSeqPlayerIndex, VOL_SCALE_INDEX_FANFARE, 0x7F, setupVal2);
                        }
                        break;

                    case SEQCMD_SUB_OP_SETUP_SEQ_UNQUEUE:
                        // Unqueue `seqPlayerIndex` from sSeqRequests
                        //! @bug this command does not work as intended as unqueueing
                        //! the sequence relies on `MM_gActiveSeqs[seqPlayerIndex].seqId`
                        //! However, `MM_gActiveSeqs[seqPlayerIndex].seqId` is reset before the sequence on
                        //! `seqPlayerIndex` is requested to stop, i.e. before the sequence is disabled and setup
                        //! commands (including this command) can run. A simple fix would have been to unqueue based on
                        //! `MM_gActiveSeqs[seqPlayerIndex].prevSeqId` instead
                        SEQCMD_UNQUEUE_SEQUENCE((u8)(seqPlayerIndex + (SEQCMD_ASYNC_ACTIVE >> 24)), 0,
                                                MM_gActiveSeqs[seqPlayerIndex].seqId);
                        break;

                    case SEQCMD_SUB_OP_SETUP_RESTART_SEQ:
                        // Restart the currently active sequence on `targetSeqPlayerIndex` with full volume.
                        // Sequence on `targetSeqPlayerIndex` must still be active to play (can be muted)
                        SEQCMD_PLAY_SEQUENCE((u8)(targetSeqPlayerIndex + (SEQCMD_ASYNC_ACTIVE >> 24)), 1,
                                             MM_gActiveSeqs[targetSeqPlayerIndex].seqId);
                        MM_gActiveSeqs[targetSeqPlayerIndex].fadeVolUpdate = true;
                        MM_gActiveSeqs[targetSeqPlayerIndex].volScales[1] = 0x7F;
                        break;

                    case SEQCMD_SUB_OP_SETUP_TEMPO_SCALE:
                        // Scale tempo by a multiplicative factor
                        SEQCMD_SCALE_TEMPO((u8)(targetSeqPlayerIndex + (SEQCMD_ASYNC_ACTIVE >> 24)), setupVal2,
                                           setupVal1);
                        break;

                    case SEQCMD_SUB_OP_SETUP_TEMPO_RESET:
                        // Reset tempo to previous tempo
                        SEQCMD_RESET_TEMPO((u8)(targetSeqPlayerIndex + (SEQCMD_ASYNC_ACTIVE >> 24)), setupVal1);
                        break;

                    case SEQCMD_SUB_OP_SETUP_PLAY_SEQ:
                        // Play the requested sequence
                        // Uses the fade timer set by `SEQCMD_SUB_OP_SETUP_SET_FADE_TIMER`
                        seqId = MM_gActiveSeqs[seqPlayerIndex].setupCmd[j] & 0xFFFF;
                        SEQCMD_PLAY_SEQUENCE((u8)(targetSeqPlayerIndex + (SEQCMD_ASYNC_ACTIVE >> 24)),
                                             MM_gActiveSeqs[targetSeqPlayerIndex].setupFadeTimer, seqId);
                        AudioSeq_SetVolumeScale(targetSeqPlayerIndex, VOL_SCALE_INDEX_FANFARE, 0x7F, 0);
                        MM_gActiveSeqs[targetSeqPlayerIndex].setupFadeTimer = 0;
                        break;

                    case SEQCMD_SUB_OP_SETUP_SET_FADE_TIMER:
                        // A command specifically to support `SEQCMD_SUB_OP_SETUP_PLAY_SEQ`
                        // Sets the fade timer for the sequence requested in `SEQCMD_SUB_OP_SETUP_PLAY_SEQ`
                        MM_gActiveSeqs[seqPlayerIndex].setupFadeTimer = setupVal2;
                        break;

                    case SEQCMD_SUB_OP_SETUP_RESTORE_SEQPLAYER_VOLUME_WITH_SCALE_INDEX:
                        // Restore the volume back to default levels
                        // Allows a `scaleIndex` to be specified.
                        AudioSeq_SetVolumeScale(targetSeqPlayerIndex, setupVal2, 0x7F, setupVal1);
                        break;

                    case SEQCMD_SUB_OP_SETUP_POP_PERSISTENT_CACHE:
                        // Discard audio data by popping one more audio caches from the audio heap
                        if (setupVal1 & (1 << SEQUENCE_TABLE)) {
                            AUDIOCMD_GLOBAL_POP_PERSISTENT_CACHE(SEQUENCE_TABLE);
                        }
                        if (setupVal1 & (1 << FONT_TABLE)) {
                            AUDIOCMD_GLOBAL_POP_PERSISTENT_CACHE(FONT_TABLE);
                        }
                        if (setupVal1 & (1 << SAMPLE_TABLE)) {
                            AUDIOCMD_GLOBAL_POP_PERSISTENT_CACHE(SAMPLE_TABLE);
                        }
                        break;

                    case SEQCMD_SUB_OP_SETUP_SET_CHANNEL_DISABLE_MASK:
                        // Disable (or reenable) specific channels of `targetSeqPlayerIndex`
                        channelMask = MM_gActiveSeqs[seqPlayerIndex].setupCmd[j] & 0xFFFF;
                        SEQCMD_SET_CHANNEL_DISABLE_MASK((u8)(targetSeqPlayerIndex + (SEQCMD_ASYNC_ACTIVE >> 24)),
                                                        channelMask);
                        break;

                    case SEQCMD_SUB_OP_SETUP_SET_SEQPLAYER_FREQ:
                        // Scale all channels of `targetSeqPlayerIndex`
                        SEQCMD_SET_SEQPLAYER_FREQ((u8)(targetSeqPlayerIndex + (SEQCMD_ASYNC_ACTIVE >> 24)), setupVal2,
                                                  setupVal1 * 10);
                        break;

                    default:
                        break;
                }
            }

            MM_gActiveSeqs[seqPlayerIndex].setupCmdNum = 0;
        }
    }
}

u8 AudioSeq_UpdateAudioHeapReset(void) {
    if (gAudioHeapResetState != AUDIO_HEAP_RESET_STATE_NONE) {
        if (gAudioHeapResetState == AUDIO_HEAP_RESET_STATE_RESETTING) {
            if (func_80193C5C() == 1) {
                gAudioHeapResetState = AUDIO_HEAP_RESET_STATE_NONE;
                AUDIOCMD_SEQPLAYER_SET_IO(SEQ_PLAYER_SFX, 0, MM_gSfxChannelLayout);
                Audio_ResetForAudioHeapStep2();
            }
        } else if (gAudioHeapResetState == AUDIO_HEAP_RESET_STATE_RESETTING_ALT) {
            while (func_80193C5C() != 1) {}
            gAudioHeapResetState = AUDIO_HEAP_RESET_STATE_NONE;
            AUDIOCMD_SEQPLAYER_SET_IO(SEQ_PLAYER_SFX, 0, MM_gSfxChannelLayout);
            Audio_ResetForAudioHeapStep2();
        }
    }

    return gAudioHeapResetState;
}

u8 AudioSeq_ResetReverb(void) {
    u8 isReverbFading = false;
    u8 fadeReverb = ((sResetAudioHeapSeqCmd & 0xFF0000) >> 16);
    u8 reverbIndex = 0;
    u8 specId;

    if (sResetAudioHeapSeqCmd != 0) {
        if (sResetAudioHeapTimer--) {
            while (fadeReverb != 0) {
                if (fadeReverb & 1) {
                    AUDIOCMD_GLOBAL_SET_REVERB_DATA(reverbIndex, REVERB_DATA_TYPE_VOLUME,
                                                    sResetAudioHeapFadeReverbVolume);
                    AudioThread_ScheduleProcessCmds();
                }
                reverbIndex++;
                fadeReverb = fadeReverb >> 1;
            }

            sResetAudioHeapFadeReverbVolume -= sResetAudioHeapFadeReverbVolumeStep;
            isReverbFading = true;
        } else {
            while (fadeReverb != 0) {
                if (fadeReverb & 1) {
                    //! @bug Triggering the ending cutscene during final hours can give an index of 12 here, which is
                    // OOB. This does not normally happen in the game, but Triforce Hunt makes this a possibility.
                    // Follow a condition similar to what is found in Audio_ResetForAudioHeapStep2.
                    if (((u8)(sResetAudioHeapSeqCmd & 0xFF)) < ARRAY_COUNT(gReverbSettingsTable)) {
                        AUDIOCMD_GLOBAL_SET_REVERB_DATA(
                            reverbIndex, REVERB_DATA_TYPE_SETTINGS,
                            (uintptr_t)(gReverbSettingsTable[(u8)(sResetAudioHeapSeqCmd & 0xFF)] + reverbIndex));
                        AudioThread_ScheduleProcessCmds();
                    }
                }
                reverbIndex++;
                fadeReverb = fadeReverb >> 1;
            }

            sResetAudioHeapSeqCmd = 0;
            AUDIOCMD_SEQPLAYER_SET_IO(SEQ_PLAYER_SFX, 0, MM_gSfxChannelLayout);
            Audio_ResetForAudioHeapStep3();
        }
    }

    return isReverbFading;
}

void AudioSeq_ResetActiveSequences(void) {
    u8 seqPlayerIndex;
    u8 scaleIndex;

    for (seqPlayerIndex = 0; seqPlayerIndex < SEQ_PLAYER_MAX; seqPlayerIndex++) {
        MM_sNumSeqRequests[seqPlayerIndex] = 0;

        MM_gActiveSeqs[seqPlayerIndex].seqId = NA_BGM_DISABLED;
        MM_gActiveSeqs[seqPlayerIndex].prevSeqId = NA_BGM_DISABLED;
        MM_gActiveSeqs[seqPlayerIndex].tempoTimer = 0;
        MM_gActiveSeqs[seqPlayerIndex].tempoOriginal = 0;
        MM_gActiveSeqs[seqPlayerIndex].tempoCmd = 0;
        MM_gActiveSeqs[seqPlayerIndex].channelPortMask = 0;
        MM_gActiveSeqs[seqPlayerIndex].setupCmdNum = 0;
        MM_gActiveSeqs[seqPlayerIndex].setupFadeTimer = 0;
        MM_gActiveSeqs[seqPlayerIndex].freqScaleChannelFlags = 0;
        MM_gActiveSeqs[seqPlayerIndex].volChannelFlags = 0;
        MM_gActiveSeqs[seqPlayerIndex].isWaitingForFonts = false;
        MM_gActiveSeqs[seqPlayerIndex].isSeqPlayerInit = false;

        for (scaleIndex = 0; scaleIndex < VOL_SCALE_INDEX_MAX; scaleIndex++) {
            MM_gActiveSeqs[seqPlayerIndex].volScales[scaleIndex] = 0x7F;
        }

        MM_gActiveSeqs[seqPlayerIndex].volFadeTimer = 1;
        MM_gActiveSeqs[seqPlayerIndex].fadeVolUpdate = true;
    }
}

void AudioSeq_ResetActiveSequencesAndVolume(void) {
    u8 seqPlayerIndex;
    u8 scaleIndex;

    for (seqPlayerIndex = 0; seqPlayerIndex < SEQ_PLAYER_MAX; seqPlayerIndex++) {
        MM_gActiveSeqs[seqPlayerIndex].volCur = 1.0f;
        MM_gActiveSeqs[seqPlayerIndex].volTimer = 0;
        MM_gActiveSeqs[seqPlayerIndex].fadeVolUpdate = false;
        for (scaleIndex = 0; scaleIndex < VOL_SCALE_INDEX_MAX; scaleIndex++) {
            MM_gActiveSeqs[seqPlayerIndex].volScales[scaleIndex] = 0x7F;
        }
    }
    AudioSeq_ResetActiveSequences();
}

// #region 2S2H [Port][Audio] Setter/Getter funcs for controlling the port volume sliders for each sequence player
void AudioSeq_SetPortVolumeScale(u8 seqPlayerIndex, f32 volume) {
    if (seqPlayerIndex >= SEQ_PLAYER_MAX) {
        return;
    }

    gAudioCtx.seqPlayers[seqPlayerIndex].portVolumeScale = volume;
    gAudioCtx.seqPlayers[seqPlayerIndex].recalculateVolume = true;
}

float AudioSeq_GetPortVolumeScale(u8 seqPlayerIndex) {
    return gAudioCtx.seqPlayers[seqPlayerIndex].portVolumeScale;
}
// #endregion
