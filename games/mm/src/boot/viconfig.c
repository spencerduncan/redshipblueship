#include "libc/stdbool.h"
#include "idle.h"
#include "functions.h"

extern OSViMode osViModeNtscHpf1;
extern OSViMode MM_osViModePalLan1;
extern OSViMode osViModeNtscHpn1;
extern OSViMode MM_osViModeNtscLan1;
extern OSViMode MM_osViModeMpalLan1;
extern OSViMode MM_osViModeFpalLan1;

void MM_ViConfig_UpdateVi(u32 black) {
#if 0
    if (black) {
        switch (osTvType) {
            case OS_TV_MPAL:
                MM_osViSetMode(&MM_osViModeMpalLan1);
                break;

            case OS_TV_PAL:
                MM_osViSetMode(&MM_osViModePalLan1);
                break;

            case OS_TV_NTSC:
            default:
                MM_osViSetMode(&MM_osViModeNtscLan1);
                break;
        }

        if (MM_gViConfigFeatures != 0) {
            MM_osViSetSpecialFeatures(MM_gViConfigFeatures);
        }

        if (MM_gViConfigYScale != 1) {
            MM_osViSetYScale(1);
        }
    } else {
        MM_osViSetMode(&MM_gViConfigMode);

        if (MM_gViConfigAdditionalScanLines != 0) {
            MM_osViExtendVStart(MM_gViConfigAdditionalScanLines);
        }

        if (MM_gViConfigFeatures != 0) {
            MM_osViSetSpecialFeatures(MM_gViConfigFeatures);
        }

        if (MM_gViConfigXScale != 1) {
            MM_osViSetXScale(MM_gViConfigXScale);
        }

        if (MM_gViConfigYScale != 1) {
            MM_osViSetYScale(MM_gViConfigYScale);
        }
    }

    gViConfigUseBlack = black;
#endif
}

void MM_ViConfig_UpdateBlack(void) {
    if (gViConfigUseBlack) {
        MM_osViBlack(true);
    } else {
        MM_osViBlack(false);
    }
}
