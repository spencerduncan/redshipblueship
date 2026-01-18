#include "global.h"
#include "vt.h"

// this should probably go elsewhere but right now viconfig.o is the only object between idle and z_std_dma
OSPiHandle* OoT_gCartHandle = 0;

void OoT_ViConfig_UpdateVi(u32 mode) {
    if (mode != 0) {
        osSyncPrintf(VT_COL(YELLOW, BLACK) "osViSetYScale1(%f);\n" VT_RST, 1.0f);

        if (osTvType == OS_TV_PAL) {
            OoT_osViSetMode(&OoT_osViModePalLan1);
        }

        OoT_osViSetYScale(1.0f);
    } else {
        OoT_osViSetMode(&OoT_gViConfigMode);

        if (OoT_gViConfigAdditionalScanLines != 0) {
            OoT_osViExtendVStart(OoT_gViConfigAdditionalScanLines);
        }

        if (OoT_gViConfigFeatures != 0) {
            OoT_osViSetSpecialFeatures(OoT_gViConfigFeatures);
        }

        if (OoT_gViConfigXScale != 1.0f) {
            OoT_osViSetXScale(OoT_gViConfigXScale);
        }

        if (OoT_gViConfigYScale != 1.0f) {
            osSyncPrintf(VT_COL(YELLOW, BLACK) "osViSetYScale3(%f);\n" VT_RST, OoT_gViConfigYScale);
            OoT_osViSetYScale(OoT_gViConfigYScale);
        }
    }

    gViConfigUseDefault = mode;
}

void OoT_ViConfig_UpdateBlack(void) {
    if (gViConfigUseDefault != 0) {
        OoT_osViBlack(1);
    } else {
        OoT_osViBlack(0);
    }
}
