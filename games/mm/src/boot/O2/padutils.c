#include "padutils.h"
#include <ultratypes.h>
#include <string.h>

void MM_PadUtils_Init(Input* input) {
    memset(input, 0, sizeof(Input));
}

void func_80085150(void) {
}

void MM_PadUtils_ResetPressRel(Input* input) {
    input->press.button = 0;
    input->rel.button = 0;
}

u32 MM_PadUtils_CheckCurExact(Input* input, u16 value) {
    return value == input->cur.button;
}

u32 MM_PadUtils_CheckCur(Input* input, u16 key) {
    return key == (input->cur.button & key);
}

u32 MM_PadUtils_CheckPressed(Input* input, u16 key) {
    return key == (input->press.button & key);
}

u32 MM_PadUtils_CheckReleased(Input* input, u16 key) {
    return key == (input->rel.button & key);
}

u16 MM_PadUtils_GetCurButton(Input* input) {
    return input->cur.button;
}

u16 MM_PadUtils_GetPressButton(Input* input) {
    return input->press.button;
}

s8 MM_PadUtils_GetCurX(Input* input) {
    return input->cur.stick_x;
}

s8 MM_PadUtils_GetCurY(Input* input) {
    return input->cur.stick_y;
}

void MM_PadUtils_SetRelXY(Input* input, s32 x, s32 y) {
    input->rel.stick_x = x;
    input->rel.stick_y = y;
}

s8 MM_PadUtils_GetRelXImpl(Input* input) {
    return input->rel.stick_x;
}

s8 MM_PadUtils_GetRelYImpl(Input* input) {
    return input->rel.stick_y;
}

s8 MM_PadUtils_GetRelX(Input* input) {
    return MM_PadUtils_GetRelXImpl(input);
}

s8 MM_PadUtils_GetRelY(Input* input) {
    return MM_PadUtils_GetRelYImpl(input);
}

void MM_PadUtils_UpdateRelXY(Input* input) {
    s32 curX = MM_PadUtils_GetCurX(input);
    s32 curY = MM_PadUtils_GetCurY(input);
    s32 relX;
    s32 relY;

    if (curX > 7) {
        relX = (curX < 0x43) ? curX - 7 : 0x43 - 7;
    } else if (curX < -7) {
        relX = (curX > -0x43) ? curX + 7 : -0x43 + 7;
    } else {
        relX = 0;
    }

    if (curY > 7) {
        relY = (curY < 0x43) ? curY - 7 : 0x43 - 7;

    } else if (curY < -7) {
        relY = (curY > -0x43) ? curY + 7 : -0x43 + 7;
    } else {
        relY = 0;
    }

    MM_PadUtils_SetRelXY(input, relX, relY);
}
