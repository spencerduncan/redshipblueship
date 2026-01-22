#ifndef FAULT_INTERNAL_H
#define FAULT_INTERNAL_H

#include "ultra64.h"


typedef void (*FaultDrawerCallback)(void);

void MM_FaultDrawer_SetOsSyncPrintfEnabled(u32 enabled);
void MM_FaultDrawer_DrawRecImpl(s32 xStart, s32 yStart, s32 xEnd, s32 yEnd, u16 color);
void MM_FaultDrawer_FillScreen(void);
void* MM_FaultDrawer_FormatStringFunc(void* arg, const char* str, size_t count);
void FaultDrawer_SetDrawerFrameBuffer(void* frameBuffer, u16 w, u16 h);
void FaultDrawer_SetInputCallback(FaultDrawerCallback callback);
void FaultDrawer_Init(void);


#endif
