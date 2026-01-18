#include "global.h"

OSPfs sMempakPfsHandle;
s32 sMempakFreeBytes;
s32 sMempakFiles[10];

u16 sMempakCompanyCode = 1;
u32 sMempakGameCode = 1;

// "ZELDA DEMO TOOL "
u8 sMempakGameName[0x10] = { 0x33, 0x1E, 0x25, 0x1D, 0x1A, 0x0F, 0x1D, 0x1E,
                             0x26, 0x28, 0x0F, 0x2D, 0x28, 0x28, 0x25, 0x0F };
u8 sMempakExtName[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

s32 Mempak_Init(s32 controllerNb) {
    OSMesgQueue* mq;
    s32 pad;
    s32 ret = false;

    mq = PadMgr_LockSerialMesgQueue(&OoT_gPadMgr);

    if (!OoT_osPfsInitPak(mq, &sMempakPfsHandle, controllerNb)) {
        ret = true;
    }

    OoT_osPfsFreeBlocks(&sMempakPfsHandle, &sMempakFreeBytes);
    PadMgr_UnlockSerialMesgQueue(&OoT_gPadMgr, mq);

    return ret;
}

s32 Mempak_GetFreeBytes(s32 controllerNb) {
    return sMempakFreeBytes;
}

s32 Mempak_FindFile(s32 controllerNb, char start, char end) {
    OSMesgQueue* mq;
    s32 error;
    char idx;
    u32 bit = 1;
    s32 flag = 0;

    mq = PadMgr_LockSerialMesgQueue(&OoT_gPadMgr);

    for (idx = start; idx <= end; idx++) {
        sMempakExtName[0] = idx - 0x27;

        error = OoT_osPfsFindFile(&sMempakPfsHandle, sMempakCompanyCode, sMempakGameCode, sMempakGameName, sMempakExtName,
                              &sMempakFiles[idx - 'A']);
        if (error == 0) {
            flag |= bit;
        } else {
            sMempakFiles[idx - 'A'] = -1;
        }

        bit <<= 1;
        osSyncPrintf("mempak: find '%c' (%d)\n", idx, error);
    }

    PadMgr_UnlockSerialMesgQueue(&OoT_gPadMgr, mq);
    osSyncPrintf("mempak: find '%c' - '%c' %02x\n", start, end, flag);

    return flag;
}

s32 Mempak_Write(s32 controllerNb, char idx, void* buffer, s32 offset, ptrdiff_t size) {
    OSMesgQueue* mq;
    s32 error;
    s32 ret = false;
    s32 pad;

    mq = PadMgr_LockSerialMesgQueue(&OoT_gPadMgr);

    if (size < sMempakFreeBytes) {
        error = OoT_osPfsReadWriteFile(&sMempakPfsHandle, sMempakFiles[idx - 'A'], 1, offset, size, buffer);
        if (error == 0) {
            ret = true;
        }
        osSyncPrintf("mempak: write %d byte '%c' (%d)->%d\n", size, idx, sMempakFiles[idx - 'A'], error);
    }
    PadMgr_UnlockSerialMesgQueue(&OoT_gPadMgr, mq);

    return ret;
}

s32 Mempak_Read(s32 controllerNb, char idx, void* buffer, s32 offset, ptrdiff_t size) {
    OSMesgQueue* mq;
    s32 error;
    s32 ret = false;
    s32 pad;

    mq = PadMgr_LockSerialMesgQueue(&OoT_gPadMgr);

    if (size < sMempakFreeBytes) {
        error = OoT_osPfsReadWriteFile(&sMempakPfsHandle, sMempakFiles[idx - 'A'], 0, offset, size, buffer);
        if (error == 0) {
            ret = true;
        }
        osSyncPrintf("mempak: read %d byte '%c' (%d)<-%d\n", size, idx, sMempakFiles[idx - 'A'], error);
    }
    PadMgr_UnlockSerialMesgQueue(&OoT_gPadMgr, mq);
    return ret;
}

s32 Mempak_Alloc(s32 controllerNb, char* idx, ptrdiff_t size) {
    OSMesgQueue* mq;
    s32 error;
    s32 ret = 0;
    s32 i;
    s32 pad;

    mq = PadMgr_LockSerialMesgQueue(&OoT_gPadMgr);

    if (*idx >= 'A' && *idx < 'L') {
        sMempakExtName[0] = *idx - 0x27;
        if (-1 == sMempakFiles[*idx - 'A']) {
            error = OoT_osPfsAllocateFile(&sMempakPfsHandle, sMempakCompanyCode, sMempakGameCode, sMempakGameName,
                                      sMempakExtName, size, &sMempakFiles[*idx - 'A']);
            if (error == 0) {
                ret = 1;
            }
            osSyncPrintf("mempak: alloc %d byte '%c' (%d)\n", size, *idx, error);
        } else {
            sMempakExtName[0] = *idx - 0x27;
            if (OoT_osPfsDeleteFile(&sMempakPfsHandle, sMempakCompanyCode, sMempakGameCode, sMempakGameName,
                                sMempakExtName) == 0) {
                ret = 1;
            }
            error = OoT_osPfsAllocateFile(&sMempakPfsHandle, sMempakCompanyCode, sMempakGameCode, sMempakGameName,
                                      sMempakExtName, size, &sMempakFiles[*idx - 'A']);
            if (error == 0) {
                ret |= 1;
            }
            osSyncPrintf("mempak: resize %d byte '%c' (%d)\n", size, *idx, error);
        }
    } else {
        for (i = 0; i < ARRAY_COUNT(sMempakFiles); i++) {
            if (sMempakFiles[i] == -1) {
                break;
            }
        }

        *idx = i + 'A';
        sMempakExtName[0] = *idx - 0x27;
        error = OoT_osPfsAllocateFile(&sMempakPfsHandle, sMempakCompanyCode, sMempakGameCode, sMempakGameName,
                                  sMempakExtName, size, &sMempakFiles[i]);
        osSyncPrintf("mempak: alloc %d byte '%c' (%d) with search\n", size, *idx, error);
        if (error == 0) {
            ret = 1;
        }
    }
    PadMgr_UnlockSerialMesgQueue(&OoT_gPadMgr, mq);

    return ret;
}

s32 Mempak_DeleteFile(s32 controllerNb, char idx) {
    OSMesgQueue* mq;
    s32 error;
    s32 ret = false;

    mq = PadMgr_LockSerialMesgQueue(&OoT_gPadMgr);

    sMempakExtName[0] = idx - 0x27;
    error = OoT_osPfsDeleteFile(&sMempakPfsHandle, sMempakCompanyCode, sMempakGameCode, sMempakGameName, sMempakExtName);
    if (error == 0) {
        ret = true;
    }
    osSyncPrintf("mempak: delete '%c' (%d)\n", idx, error);
    PadMgr_UnlockSerialMesgQueue(&OoT_gPadMgr, mq);

    return ret;
}

s32 Mempak_GetFileSize(s32 controllerNb, char idx) {
    OSMesgQueue* mq = PadMgr_LockSerialMesgQueue(&OoT_gPadMgr);
    OSPfsState state;
    s32 error = OoT_osPfsFileState(&sMempakPfsHandle, sMempakFiles[idx - 'A'], &state);
    s32 pad;

    PadMgr_UnlockSerialMesgQueue(&OoT_gPadMgr, mq);

    if (error != 0) {
        return 0;
    }
    return state.file_size;
}
