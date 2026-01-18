#include "global.h"
#include "vt.h"

#include <string.h>

//#define KALEIDO_OVERLAY(name)                                                                                \
//    {                                                                                                        \
//        NULL, (uintptr_t)_ovl_##name##SegmentRomStart, (uintptr_t)_ovl_##name##SegmentRomEnd, _ovl_##name##SegmentStart, \
//            _ovl_##name##SegmentEnd, 0, #name,                                                               \
//    }

#define KALEIDO_OVERLAY(name) \
    { 0 }

KaleidoMgrOverlay OoT_gKaleidoMgrOverlayTable[] = {
    KALEIDO_OVERLAY(kaleido_scope),
    KALEIDO_OVERLAY(player_actor),
};

void* OoT_sKaleidoAreaPtr = NULL;
KaleidoMgrOverlay* OoT_gKaleidoMgrCurOvl = NULL;
u8 gBossMarkState = 0;

void OoT_KaleidoManager_LoadOvl(KaleidoMgrOverlay* ovl) {
    // LOG_CHECK_NULL_POINTER("KaleidoArea_allocp", OoT_sKaleidoAreaPtr);

    ovl->loadedRamAddr = OoT_sKaleidoAreaPtr;
    OoT_Overlay_Load(ovl->vromStart, ovl->vromEnd, ovl->vramStart, ovl->vramEnd, ovl->loadedRamAddr);

    // osSyncPrintf(VT_FGCOL(GREEN));
    // osSyncPrintf("OVL(k):Seg:%08x-%08x Ram:%08x-%08x Off:%08x %s\n", ovl->vramStart, ovl->vramEnd,
    // ovl->loadedRamAddr, (uintptr_t)ovl->loadedRamAddr + (uintptr_t)ovl->vramEnd - (uintptr_t)ovl->vramStart,
    //(uintptr_t)ovl->vramStart - (uintptr_t)ovl->loadedRamAddr, ovl->name);
    // osSyncPrintf(VT_RST);

    ovl->offset = (uintptr_t)ovl->loadedRamAddr - (uintptr_t)ovl->vramStart;
    OoT_gKaleidoMgrCurOvl = ovl;
}

void OoT_KaleidoManager_ClearOvl(KaleidoMgrOverlay* ovl) {
    if (ovl->loadedRamAddr != NULL) {
        ovl->offset = 0;
        memset(ovl->loadedRamAddr, 0, (uintptr_t)ovl->vramEnd - (uintptr_t)ovl->vramStart);
        ovl->loadedRamAddr = NULL;
        OoT_gKaleidoMgrCurOvl = NULL;
    }
}

void OoT_KaleidoManager_Init(PlayState* play) {
    ptrdiff_t largestSize = 0;
    ptrdiff_t size;
    u32 i;

    for (i = 0; i < ARRAY_COUNT(OoT_gKaleidoMgrOverlayTable); i++) {
        size = (uintptr_t)OoT_gKaleidoMgrOverlayTable[i].vramEnd - (uintptr_t)OoT_gKaleidoMgrOverlayTable[i].vramStart;
        if (size > largestSize) {
            largestSize = size;
        }
    }

    osSyncPrintf(VT_FGCOL(GREEN));
    osSyncPrintf("KaleidoArea の最大サイズは %d バイトを確保します\n", largestSize);
    osSyncPrintf(VT_RST);

    OoT_sKaleidoAreaPtr = GAMESTATE_ALLOC_MC(&play->state, largestSize);
    LOG_CHECK_NULL_POINTER("KaleidoArea_allocp", OoT_sKaleidoAreaPtr);

    osSyncPrintf(VT_FGCOL(GREEN));
    osSyncPrintf("KaleidoArea %08x - %08x\n", OoT_sKaleidoAreaPtr, (uintptr_t)OoT_sKaleidoAreaPtr + largestSize);
    osSyncPrintf(VT_RST);

    OoT_gKaleidoMgrCurOvl = 0;
}

void OoT_KaleidoManager_Destroy() {
    if (OoT_gKaleidoMgrCurOvl != NULL) {
        OoT_KaleidoManager_ClearOvl(OoT_gKaleidoMgrCurOvl);
        OoT_gKaleidoMgrCurOvl = NULL;
    }

    OoT_sKaleidoAreaPtr = NULL;
}

// NOTE: this function looks messed up and probably doesn't work how it was intended to
void* OoT_KaleidoManager_GetRamAddr(void* vram) {
    return vram;

#if 0
    KaleidoMgrOverlay* iter = OoT_gKaleidoMgrCurOvl;
    KaleidoMgrOverlay* ovl = iter;
    u32 i;

    if (ovl == NULL) {
        iter = &OoT_gKaleidoMgrOverlayTable[0];
        for (i = 0; i < ARRAY_COUNT(OoT_gKaleidoMgrOverlayTable); i++) {
            if (((uintptr_t)vram >= (uintptr_t)iter->vramStart) && ((uintptr_t)iter->vramEnd >= (uintptr_t)vram)) {
                OoT_KaleidoManager_LoadOvl(iter);
                ovl = iter;
                goto KaleidoManager_GetRamAddr_end;
            }
            //! @bug Probably missing iter++ here
        }

        osSyncPrintf("異常\n"); // "Abnormal"
        return NULL;
    }

KaleidoManager_GetRamAddr_end:
    if ((ovl == NULL) || ((uintptr_t)vram < (uintptr_t)ovl->vramStart) || ((uintptr_t)vram >= (uintptr_t)ovl->vramEnd)) {
        return NULL;
    }

    return (void*)((uintptr_t)vram + ovl->offset);
#endif
}
