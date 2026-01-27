#include "prevent_bss_reordering.h"
#include "buffers.h"
#include "irqmgr.h"
#include "main.h"
#include "stack.h"
#include "stackcheck.h"
#include "z64thread.h"

// Variables are put before most headers as a hacky way to bypass bss reordering
IrqMgr MM_gIrqMgr;
STACK(MM_sIrqMgrStack, 0x500);
StackEntry MM_sIrqMgrStackInfo;
OSThread sMainThread;
STACK(MM_sMainStack, 0x900);
StackEntry MM_sMainStackInfo;
OSMesg MM_sPiMgrCmdBuff[50];
OSMesgQueue gPiMgrCmdQueue;
OSViMode MM_gViConfigMode;
u8 gViConfigModeType;

#include "global.h"
#include "buffers.h"
#include "idle.h"

u8 D_80096B20 = 1;
vu8 gViConfigUseBlack = true;
u8 MM_gViConfigAdditionalScanLines = 0;
u32 MM_gViConfigFeatures = OS_VI_DITHER_FILTER_ON | OS_VI_GAMMA_OFF;
f32 MM_gViConfigXScale = 1.0f;
f32 MM_gViConfigYScale = 1.0f;

void Main_ClearMemory(void* begin, void* end) {
    // if (begin < end) {
    //     bzero(begin, (uintptr_t)end - (uintptr_t)begin);
    // }
}

void Main_InitFramebuffer(u32* framebuffer, size_t numBytes, u32 value) {
    // for (; numBytes > 0; numBytes -= sizeof(u32)) {
    //     *framebuffer++ = value;
    // }
}

void Main_InitScreen(void) {
#if 0
    Main_InitFramebuffer((u32*)gLoBuffer.framebuffer, sizeof(gLoBuffer.framebuffer),
                         (GPACK_RGBA5551(0, 0, 0, 1) << 16) | GPACK_RGBA5551(0, 0, 0, 1));
    MM_ViConfig_UpdateVi(false);
    MM_osViSwapBuffer(gLoBuffer.framebuffer);
    MM_osViBlack(false);
#endif
}

void Main_InitMemory(void) {
#if 0
    void* memStart = (void*)0x80000400;
    void* memEnd = OS_PHYSICAL_TO_K0(MM_osMemSize);

    Main_ClearMemory(memStart, SEGMENT_START(framebuffer_lo));

    // Clear the rest of the buffer that was not initialized by Main_InitScreen
    Main_ClearMemory(&gLoBuffer.skyboxBuffer, SEGMENT_END(framebuffer_lo));

    // Clear all the buffers after the `code` segment, up to the end of the available RAM space
    Main_ClearMemory(SEGMENT_END(code), memEnd);
#endif
}

void Main_Init(void) {
#if 0
    DmaRequest dmaReq;
    OSMesgQueue mq;
    OSMesg msg[1];
    size_t prevSize;

    MM_osCreateMesgQueue(&mq, msg, ARRAY_COUNT(msg));

    prevSize = MM_gDmaMgrDmaBuffSize;
    MM_gDmaMgrDmaBuffSize = 0;

    MM_DmaMgr_SendRequestImpl(&dmaReq, SEGMENT_START(code), SEGMENT_ROM_START(code), SEGMENT_ROM_SIZE_ALT(code), 0, &mq,
                           NULL);
    Main_InitScreen();
    Main_InitMemory();
    MM_osRecvMesg(&mq, NULL, OS_MESG_BLOCK);

    MM_gDmaMgrDmaBuffSize = prevSize;

    Main_ClearMemory(SEGMENT_BSS_START(code), SEGMENT_BSS_END(code));
#endif
}

void MM_Main_ThreadEntry(void* arg) {
#if 0
    MM_StackCheck_Init(&MM_sIrqMgrStackInfo, MM_sIrqMgrStack, STACK_TOP(MM_sIrqMgrStack), 0, 0x100, "irqmgr");
    MM_IrqMgr_Init(&MM_gIrqMgr, STACK_TOP(MM_sIrqMgrStack), Z_PRIORITY_IRQMGR, 1);
    DmaMgr_Start();
    Main_Init();
    Main(arg);
    DmaMgr_Stop();
#endif
}

void Idle_InitVideo(void) {
#if 0
    MM_osCreateViManager(OS_PRIORITY_VIMGR);

    MM_gViConfigFeatures = OS_VI_DITHER_FILTER_ON | OS_VI_GAMMA_OFF;
    MM_gViConfigXScale = 1.0f;
    MM_gViConfigYScale = 1.0f;

    switch (MM_osTvType) {
        case OS_TV_NTSC:
            gViConfigModeType = OS_VI_NTSC_LAN1;
            MM_gViConfigMode = MM_osViModeNtscLan1;
            break;

        case OS_TV_MPAL:
            gViConfigModeType = OS_VI_MPAL_LAN1;
            MM_gViConfigMode = MM_osViModeMpalLan1;
            break;

        case OS_TV_PAL:
            gViConfigModeType = OS_VI_FPAL_LAN1;
            MM_gViConfigMode = MM_osViModeFpalLan1;
            MM_gViConfigYScale = 0.833f;
            break;
    }

    D_80096B20 = 1;
#endif
}

void MM_Idle_ThreadEntry(void* arg) {
    // Idle_InitVideo();
    // MM_osCreatePiManager(OS_PRIORITY_PIMGR, &gPiMgrCmdQueue, MM_sPiMgrCmdBuff, ARRAY_COUNT(MM_sPiMgrCmdBuff));
    // MM_StackCheck_Init(&MM_sMainStackInfo, MM_sMainStack, STACK_TOP(MM_sMainStack), 0, 0x400, "main");
    // MM_osCreateThread(&sMainThread, Z_THREAD_ID_MAIN, MM_Main_ThreadEntry, arg, STACK_TOP(MM_sMainStack), Z_PRIORITY_MAIN);
    // MM_osStartThread(&sMainThread);
    // MM_osSetThreadPri(NULL, OS_PRIORITY_IDLE);

    // for (;;) {}
}
