#include "prevent_bss_reordering.h"
#include "buffers.h"

u8 MM_gGfxSPTaskYieldBuffer[OS_YIELD_DATA_SIZE] ALIGNED(16);

STACK(gGfxSPTaskStack, 0x400) ALIGNED(16);

GfxPool MM_gGfxPools[2] ALIGNED(16);
