#include "global.h"
#include <string.h>
#include "soh/OTRGlobals.h"

void func_800C3C80(AudioMgr* audioMgr) {
    AudioTask* task;

    task = audioMgr->rspTask;
    if (audioMgr->rspTask->taskQueue != NULL) {
        osSendMesgPtr(task->taskQueue, NULL, OS_MESG_BLOCK);
    }
}

void OoT_AudioMgr_HandleRetrace(AudioMgr* audioMgr) {
    AudioTask* rspTask;

    if (SREG(20) > 0) {
        audioMgr->rspTask = NULL;
    }
    if (audioMgr->rspTask != NULL) {
        audioMgr->audioTask.next = NULL;
        audioMgr->audioTask.flags = 2;
        audioMgr->audioTask.framebuffer = NULL;

        audioMgr->audioTask.list = audioMgr->rspTask->task;
        audioMgr->audioTask.msgQ = &audioMgr->unk_AC;

        audioMgr->audioTask.msg.ptr = NULL;
        osSendMesgPtr(&audioMgr->sched->cmdQ, &audioMgr->audioTask, OS_MESG_BLOCK);
        OoT_Sched_SendEntryMsg(audioMgr->sched);
    }

    D_8016A550 = OoT_osGetTime();
    if (SREG(20) >= 2) {
        rspTask = NULL;
    } else {
        rspTask = func_800E4FE0();
    }
    D_8016A558 += OoT_osGetTime() - D_8016A550;
    D_8016A550 = 0;
    if (audioMgr->rspTask != NULL) {
        OoT_osRecvMesg(&audioMgr->unk_AC, NULL, OS_MESG_BLOCK);
        func_800C3C80(audioMgr);
    }
    audioMgr->rspTask = rspTask;
}

void AudioMgr_HandlePRENMI(AudioMgr* audioMgr) {
    // "Audio manager received OS_SC_PRE_NMI_MSG"
    osSyncPrintf("オーディオマネージャが OS_SC_PRE_NMI_MSG を受け取りました\n");
    OoT_Audio_PreNMI();
}

void OoT_AudioMgr_ThreadEntry(void* arg0) {
    AudioMgr* audioMgr = (AudioMgr*)arg0;
    s16* msg = NULL;

    // while (true) {
    OoT_osRecvMesg(&audioMgr->unk_74, (OSMesg*)&msg, OS_MESG_BLOCK);
    OoT_AudioMgr_HandleRetrace(audioMgr);
    /*switch (*msg) {
        case OS_SC_RETRACE_MSG:
            OoT_AudioMgr_HandleRetrace(audioMgr);
            while (audioMgr->unk_74.validCount != 0) {
                OoT_osRecvMesg(&audioMgr->unk_74, (OSMesg*)&msg, OS_MESG_BLOCK);
                switch (*msg) {
                    case OS_SC_RETRACE_MSG:
                        break;
                    case OS_SC_PRE_NMI_MSG:
                        AudioMgr_HandlePRENMI(audioMgr);
                        break;
                }
            }
            break;
        case OS_SC_PRE_NMI_MSG:
            AudioMgr_HandlePRENMI(audioMgr);
            break;
    }*/
    //}
}

void OoT_AudioMgr_Unlock(AudioMgr* audioMgr) {
    OoT_osRecvMesg(&audioMgr->unk_C8, NULL, OS_MESG_BLOCK);
}

void OoT_AudioMgr_Init(AudioMgr* audioMgr, void* stack, OSPri pri, OSId id, SchedContext* sched, IrqMgr* irqMgr) {
    // AudioPlayer_Init();

    memset(audioMgr, 0, sizeof(AudioMgr));

    audioMgr->sched = sched;
    audioMgr->irqMgr = irqMgr;
    audioMgr->rspTask = NULL;

    OoT_osCreateMesgQueue(&audioMgr->unk_AC, &audioMgr->unk_C4, 1);
    OoT_osCreateMesgQueue(&audioMgr->unk_74, &audioMgr->unk_8C, 8);
    OoT_osCreateMesgQueue(&audioMgr->unk_C8, &audioMgr->unk_E0, 1);

    osSendMesgPtr(&audioMgr->unk_AC, NULL, OS_MESG_BLOCK);

    static bool hasInitialized = false;

    if (!hasInitialized) {
        IrqMgrClient irqClient;

        osSyncPrintf("オーディオマネージャスレッド実行開始\n"); // "Start running audio manager thread"
        OoT_Audio_Init();
        OoT_AudioLoad_SetDmaHandler(OoT_DmaMgr_DmaHandler);
        OoT_Audio_InitSound();
        osSendMesgPtr(&audioMgr->unk_C8, NULL, OS_MESG_BLOCK);

        Audio_SetGameVolume(SEQ_PLAYER_BGM_MAIN,
                            ((float)CVarGetInteger(CVAR_SETTING("Volume.MainMusic"), 100) / 100.0f));
        Audio_SetGameVolume(SEQ_PLAYER_BGM_SUB, ((float)CVarGetInteger(CVAR_SETTING("Volume.SubMusic"), 100) / 100.0f));
        Audio_SetGameVolume(SEQ_PLAYER_FANFARE, ((float)CVarGetInteger(CVAR_SETTING("Volume.Fanfare"), 100) / 100.0f));
        Audio_SetGameVolume(SEQ_PLAYER_SFX, ((float)CVarGetInteger(CVAR_SETTING("Volume.SFX"), 100) / 100.0f));

        // Removed due to crash
        // OoT_IrqMgr_AddClient(audioMgr->irqMgr, &irqClient, &audioMgr->unk_74);
        hasInitialized = true;
    }

    // osCreateThread(&audioMgr->unk_E8, id, OoT_AudioMgr_ThreadEntry, audioMgr, stack, pri);
    // OoT_osStartThread(&audioMgr->unk_E8);
}
