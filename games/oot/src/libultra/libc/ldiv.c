#include "global.h"

ldiv_t OoT_ldiv(s32 num, s32 denom) {
    ldiv_t ret;

    ret.quot = num / denom;
    ret.rem = num - denom * ret.quot;
    if (ret.quot < 0 && ret.rem > 0) {
        ret.quot++;
        ret.rem -= denom;
    }

    return ret;
}

lldiv_t OoT_lldiv(s64 num, s64 denom) {
    lldiv_t ret;

    ret.quot = num / denom;
    ret.rem = num - denom * ret.quot;
    if (ret.quot < 0 && ret.rem > 0) {
        ret.quot++;
        ret.rem -= denom;
    }

    return ret;
}
