#include "libc/stdbool.h"
#include "idle.h"
#include "functions.h"

extern OSViMode osViModeNtscHpf1;
extern OSViMode osViModePalLan1;
extern OSViMode osViModeNtscHpn1;
extern OSViMode osViModeNtscLan1;
extern OSViMode osViModeMpalLan1;
extern OSViMode osViModeFpalLan1;

void MM_ViConfig_UpdateVi(u32 black) {
#if 0
    if (black) {
        switch (osTvType) {
            case OS_TV_MPAL:
                osViSetMode(&osViModeMpalLan1);
                break;

            case OS_TV_PAL:
                osViSetMode(&osViModePalLan1);
                break;

            case OS_TV_NTSC:
            default:
                osViSetMode(&osViModeNtscLan1);
                break;
        }

        if (MM_gViConfigFeatures != 0) {
            osViSetSpecialFeatures(MM_gViConfigFeatures);
        }

        if (MM_gViConfigYScale != 1) {
            osViSetYScale(1);
        }
    } else {
        osViSetMode(&MM_gViConfigMode);

        if (MM_gViConfigAdditionalScanLines != 0) {
            osViExtendVStart(MM_gViConfigAdditionalScanLines);
        }

        if (MM_gViConfigFeatures != 0) {
            osViSetSpecialFeatures(MM_gViConfigFeatures);
        }

        if (MM_gViConfigXScale != 1) {
            osViSetXScale(MM_gViConfigXScale);
        }

        if (MM_gViConfigYScale != 1) {
            osViSetYScale(MM_gViConfigYScale);
        }
    }

    gViConfigUseBlack = black;
#endif
}

void MM_ViConfig_UpdateBlack(void) {
    if (gViConfigUseBlack) {
        osViBlack(true);
    } else {
        osViBlack(false);
    }
}
