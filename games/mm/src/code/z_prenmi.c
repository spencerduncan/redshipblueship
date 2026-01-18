#include "global.h"
#include "z_prenmi.h"

void PreNMI_Stop(PreNMIState* this) {
    STOP_GAMESTATE(&this->state);
    SET_NEXT_GAMESTATE(&this->state, NULL, 0);
}

void MM_PreNMI_Update(PreNMIState* this) {
    if (this->timer == 0) {
        MM_ViConfig_UpdateVi(true);
        PreNMI_Stop(this);
    } else {
        this->timer--;
    }
}

void MM_PreNMI_Draw(PreNMIState* this) {
    GraphicsContext* gfxCtx = this->state.gfxCtx;

    func_8012CF0C(gfxCtx, true, true, 0, 0, 0);

    OPEN_DISPS(gfxCtx);

    Gfx_SetupDL36_Opa(gfxCtx);

    gDPSetFillColor(POLY_OPA_DISP++, (GPACK_RGBA5551(255, 255, 255, 1) << 16) | GPACK_RGBA5551(255, 255, 255, 1));
    gDPFillRectangle(POLY_OPA_DISP++, 0, this->timer + 100, SCREEN_WIDTH - 1, this->timer + 100);

    CLOSE_DISPS(gfxCtx);
}

void MM_PreNMI_Main(GameState* thisx) {
    PreNMIState* this = (PreNMIState*)thisx;

    MM_PreNMI_Update(this);
    MM_PreNMI_Draw(this);

    this->state.unk_A3 = 1;
}

void MM_PreNMI_Destroy(GameState* thisx) {
}

void MM_PreNMI_Init(GameState* thisx) {
    PreNMIState* this = (PreNMIState*)thisx;

    this->state.main = MM_PreNMI_Main;
    this->state.destroy = MM_PreNMI_Destroy;
    this->timer = 30;
    this->unk_A8 = 10;

    GameState_SetFramerateDivisor(&this->state, 1);
}
