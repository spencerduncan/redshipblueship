#include "global.h"
#include "vt.h"

// this should probably go elsewhere but right now viconfig.o is the only object between idle and z_std_dma
OSPiHandle* OoT_gCartHandle = 0;

void OoT_ViConfig_UpdateVi(u32 mode) {
    if (mode != 0) {
        osSyncPrintf(VT_COL(YELLOW, BLACK) "osViSetYScale1(%f);\n" VT_RST, 1.0f);

        if (osTvType == OS_TV_PAL) {
            osViSetMode(&osViModePalLan1);
        }

        osViSetYScale(1.0f);
    } else {
        osViSetMode(&OoT_gViConfigMode);

        if (OoT_gViConfigAdditionalScanLines != 0) {
            osViExtendVStart(OoT_gViConfigAdditionalScanLines);
        }

        if (OoT_gViConfigFeatures != 0) {
            osViSetSpecialFeatures(OoT_gViConfigFeatures);
        }

        if (OoT_gViConfigXScale != 1.0f) {
            osViSetXScale(OoT_gViConfigXScale);
        }

        if (OoT_gViConfigYScale != 1.0f) {
            osSyncPrintf(VT_COL(YELLOW, BLACK) "osViSetYScale3(%f);\n" VT_RST, OoT_gViConfigYScale);
            osViSetYScale(OoT_gViConfigYScale);
        }
    }

    gViConfigUseDefault = mode;
}

void OoT_ViConfig_UpdateBlack(void) {
    if (gViConfigUseDefault != 0) {
        osViBlack(1);
    } else {
        osViBlack(0);
    }
}
