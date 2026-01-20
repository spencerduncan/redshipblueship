#include <libultraship/libultra.h>
#include "sintable.c"

s16 OoT_sins(u16 x) {
    s16 value;

    x >>= 4;

    if (x & 0x400) {
        value = OoT_sintable[0x3FF - (x & 0x3FF)];
    } else {
        value = OoT_sintable[x & 0x3FF];
    }

    if (x & 0x800) {
        return -value;
    } else {
        return value;
    }
}
