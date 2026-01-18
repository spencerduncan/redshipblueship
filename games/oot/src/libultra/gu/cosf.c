#include <libultraship/libultra.h>
#include "global.h"

static const du OoT_P[] = {
    { 0x3FF00000, 0x00000000 }, { 0xBFC55554, 0xBC83656D }, { 0x3F8110ED, 0x3804C2A0 },
    { 0xBF29F6FF, 0xEEA56814 }, { 0x3EC5DBDF, 0x0E314BFE },
};

static const du OoT_rpi = { 0x3FD45F30, 0x6DC9C883 };

static const du OoT_pihi = { 0x400921FB, 0x50000000 };

static const du OoT_pilo = { 0x3E6110B4, 0x611A6263 };

static const fu OoT_zero = { 0x00000000 };

f32 cosf(f32 x) {
    f32 absx;
    f64 dx;
    f64 xSq;
    f64 polyApprox;
    f64 dn;
    s32 n;
    f64 result;
    s32 ix = *(s32*)&x;
    s32 xpt = (ix >> 22);

    xpt &= 0x1FF;

    if (xpt < 0x136) {
        absx = (x > 0) ? x : -x;
        dx = absx;

        dn = dx * OoT_rpi.d + 0.5;
        n = ROUND(dn);
        dn = n;

        dn -= 0.5;

        dx -= dn * OoT_pihi.d;
        dx -= dn * OoT_pilo.d;

        xSq = SQ(dx);

        polyApprox = ((OoT_P[4].d * xSq + OoT_P[3].d) * xSq + OoT_P[2].d) * xSq + OoT_P[1].d;

        result = dx + (dx * xSq) * polyApprox;

        if (!(n & 1)) {
            return (f32)result;
        }
        return -(f32)result;
    }
    if (x != x) {
        return __libm_qnan_f;
    }

    return OoT_zero.f;
}
