#ifndef IDLE_H
#define IDLE_H

#include "ultra64.h"
#include "irqmgr.h"

extern IrqMgr MM_gIrqMgr;
extern OSMesgQueue gPiMgrCmdQueue;
extern OSViMode MM_gViConfigMode;
extern u8 gViConfigModeType;

extern u8 D_80096B20;
extern vu8 gViConfigUseBlack;
extern u8 MM_gViConfigAdditionalScanLines;
extern u32 MM_gViConfigFeatures;
extern f32 MM_gViConfigXScale;
extern f32 MM_gViConfigYScale;

void MM_Idle_ThreadEntry(void* arg);

#endif
