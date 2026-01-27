#include "global.h"
#include "carthandle.h"
#include "fault.h"
#include "stack.h"
#include "stackcheck.h"
#include "z64dma.h"
#include "z64thread.h"

size_t MM_gDmaMgrDmaBuffSize = 0x2000;

// StackEntry sDmaMgrStackInfo;
// u16 sNumDmaEntries;
// OSMesgQueue sDmaMgrMsgQueue;
// OSMesg sDmaMgrMsgs[32];
// OSThread sDmaMgrThread;
// STACK(sDmaMgrStack, 0x500);

s32 MM_DmaMgr_DmaRomToRam(uintptr_t rom, void* ram, size_t size) {
#if 0
    OSIoMesg ioMsg;
    OSMesgQueue queue;
    OSMesg msg[1];
    s32 ret;
    size_t buffSize = MM_gDmaMgrDmaBuffSize;

    osInvalDCache(ram, size);
    MM_osCreateMesgQueue(&queue, msg, ARRAY_COUNT(msg));

    if (buffSize != 0) {
        while (buffSize < size) {
            ioMsg.hdr.pri = 0;
            ioMsg.hdr.retQueue = &queue;
            ioMsg.devAddr = rom;
            ioMsg.dramAddr = ram;
            ioMsg.size = buffSize;
            ret = MM_osEPiStartDma(MM_gCartHandle, &ioMsg, 0);
            if (ret != 0) {
                goto END;
            }

            MM_osRecvMesg(&queue, NULL, OS_MESG_BLOCK);
            size -= buffSize;
            rom += buffSize;
            ram = (u8*)ram + buffSize;
        }
    }
    ioMsg.hdr.pri = 0;
    ioMsg.hdr.retQueue = &queue;
    ioMsg.devAddr = rom;
    ioMsg.dramAddr = ram;
    ioMsg.size = size;
    ret = MM_osEPiStartDma(MM_gCartHandle, &ioMsg, 0);
    if (ret != 0) {
        goto END;
    }

    MM_osRecvMesg(&queue, NULL, OS_MESG_BLOCK);

    osInvalDCache(ram, size);

END:
    return ret;
#endif
    return 0;
}

s32 MM_DmaMgr_DmaHandler(OSPiHandle* pihandle, OSIoMesg* mb, s32 direction) {
    // return MM_osEPiStartDma(pihandle, mb, direction);
}

DmaEntry* DmaMgr_FindDmaEntry(uintptr_t vrom) {
#if 0
    DmaEntry* curr;

    for (curr = dmadata; curr->vromEnd != 0; curr++) {
        if (vrom < curr->vromStart) {
            continue;
        }
        if (vrom >= curr->vromEnd) {
            continue;
        }

        return curr;
    }

    return NULL;
#endif
}

u32 DmaMgr_TranslateVromToRom(uintptr_t vrom) {
#if 0
    DmaEntry* entry = DmaMgr_FindDmaEntry(vrom);

    if (entry != NULL) {
        if (entry->romEnd == 0) {
            return vrom + entry->romStart - entry->vromStart;
        }

        if (vrom == entry->vromStart) {
            return entry->romStart;
        }

        return -1;
    }

    return -1;
#endif
}

s32 DmaMgr_FindDmaIndex(uintptr_t vrom) {
#if 0
    DmaEntry* entry = DmaMgr_FindDmaEntry(vrom);

    if (entry != NULL) {
        return entry - dmadata;
    }

    return -1;
#endif
}

const char* func_800809F4(uintptr_t a0) {
    return "??";
}

void MM_DmaMgr_ProcessMsg(DmaRequest* req) {
#if 0
    uintptr_t vrom;
    void* ram;
    size_t size;
    uintptr_t romStart;
    size_t romSize;
    DmaEntry* dmaEntry;
    s32 index;

    vrom = req->vromAddr;
    ram = req->dramAddr;
    size = req->size;

    index = DmaMgr_FindDmaIndex(vrom);

    if ((index >= 0) && (index < sNumDmaEntries)) {
        dmaEntry = &dmadata[index];
        if (dmaEntry->romEnd == 0) {
            if (dmaEntry->vromEnd < (vrom + size)) {
                MM_Fault_AddHungupAndCrash("../z_std_dma.c", 499);
            }
            MM_DmaMgr_DmaRomToRam((dmaEntry->romStart + vrom) - dmaEntry->vromStart, (u8*)ram, size);
            return;
        }

        romSize = dmaEntry->romEnd - dmaEntry->romStart;
        romStart = dmaEntry->romStart;

        if (vrom != dmaEntry->vromStart) {
            MM_Fault_AddHungupAndCrash("../z_std_dma.c", 518);
        }

        if (size != (dmaEntry->vromEnd - dmaEntry->vromStart)) {
            MM_Fault_AddHungupAndCrash("../z_std_dma.c", 525);
        }

        MM_osSetThreadPri(NULL, 10);
        MM_Yaz0_Decompress(romStart, ram, romSize);
        MM_osSetThreadPri(NULL, 17);
    } else {
        MM_Fault_AddHungupAndCrash("../z_std_dma.c", 558);
    }
#endif
}

void MM_DmaMgr_ThreadEntry(void* a0) {
#if 0
    OSMesg msg;
    DmaRequest* req;

    while (1) {
        MM_osRecvMesg(&sDmaMgrMsgQueue, &msg, OS_MESG_BLOCK);

        if (msg.ptr == NULL) {
            break;
        }

        req = (DmaRequest*)msg.ptr;

        MM_DmaMgr_ProcessMsg(req);
        if (req->notifyQueue) {
            MM_osSendMesg(req->notifyQueue, req->notifyMsg, OS_MESG_NOBLOCK);
        }
    }
#endif
}

s32 MM_DmaMgr_SendRequestImpl(DmaRequest* request, void* vramStart, uintptr_t vromStart, size_t size, UNK_TYPE4 unused,
                           OSMesgQueue* queue, OSMesg msg) {
#if 0
    if (MM_gIrqMgrResetStatus >= 2) {
        return -2;
    }

    request->vromAddr = vromStart;
    request->dramAddr = vramStart;
    request->size = size;
    request->unk14 = 0;
    request->notifyQueue = queue;
    request->notifyMsg = msg;

    MM_osSendMesg(&sDmaMgrMsgQueue, OS_MESG_PTR(request), OS_MESG_BLOCK);
#endif
    return 0;
}

s32 MM_DmaMgr_SendRequest0(void* vramStart, uintptr_t vromStart, size_t size) {
#if 0
    DmaRequest req;
    OSMesgQueue queue;
    OSMesg msg[1];
    s32 ret;

    MM_osCreateMesgQueue(&queue, msg, ARRAY_COUNT(msg));

    ret = MM_DmaMgr_SendRequestImpl(&req, vramStart, vromStart, size, 0, &queue, OS_MESG_PTR(NULL));

    if (ret == -1) {
        return ret;
    } else {
        MM_osRecvMesg(&queue, NULL, OS_MESG_BLOCK);
    }
#endif
    return 0;
}

void DmaMgr_Start(void) {
#if 0
    MM_DmaMgr_DmaRomToRam(SEGMENT_ROM_START(dmadata), dmadata, SEGMENT_ROM_SIZE(dmadata));

    {
        DmaEntry* iter = dmadata;
        u32 idx = 0;

        while (iter->vromEnd != 0) {
            iter++;
            idx++;
        }

        sNumDmaEntries = idx;
    }

    MM_osCreateMesgQueue(&sDmaMgrMsgQueue, sDmaMgrMsgs, ARRAY_COUNT(sDmaMgrMsgs));
    MM_StackCheck_Init(&sDmaMgrStackInfo, sDmaMgrStack, STACK_TOP(sDmaMgrStack), 0, 0x100, "dmamgr");
    MM_osCreateThread(&sDmaMgrThread, Z_THREAD_ID_DMAMGR, MM_DmaMgr_ThreadEntry, NULL, STACK_TOP(sDmaMgrStack),
                   Z_PRIORITY_DMAMGR);

    MM_osStartThread(&sDmaMgrThread);
#endif
}

void DmaMgr_Stop(void) {
    // MM_osSendMesg(&sDmaMgrMsgQueue, OS_MESG_PTR(NULL), OS_MESG_BLOCK);
}
