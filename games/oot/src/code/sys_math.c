#include "global.h"

f32 OoT_sFactorialTbl[] = { 1.0f,    1.0f,     2.0f,      6.0f,       24.0f,       120.0f,      720.0f,
                        5040.0f, 40320.0f, 362880.0f, 3628800.0f, 39916800.0f, 479001600.0f };

f32 OoT_Math_FactorialF(f32 n) {
    f32 ret = 1.0f;
    s32 i;

    for (i = n; i > 1; i--) {
        ret *= i;
    }
    return ret;
}

f32 OoT_Math_Factorial(s32 n) {
    f32 ret;
    s32 i;

    if ((u32)n > 12U) {
        ret = OoT_sFactorialTbl[12];
        for (i = 13; i <= n; i++) {
            ret *= i;
        }
    } else {
        ret = OoT_sFactorialTbl[n];
    }
    return ret;
}

f32 OoT_Math_PowF(f32 base, s32 exp) {
    f32 ret = 1.0f;

    while (exp > 0) {
        exp--;
        ret *= base;
    }
    return ret;
}

f32 OoT_Math_SinF(f32 angle) {
    return OoT_sins((s16)(angle * (32767.0f / M_PI))) * SHT_MINV;
}

f32 OoT_Math_CosF(f32 angle) {
    return OoT_coss((s16)(angle * (32767.0f / M_PI))) * SHT_MINV;
}
