#include "ActorBehavior.h"

extern "C" {
#include "overlays/actors/ovl_En_Sob1/z_en_sob1.h"
}

#ifdef RSBS_SINGLE_EXECUTABLE
#include "2s2h/Rando/Foreign.h"
#include "2s2h/Rando/ForeignModel.h"
#endif

void EnSob1_DrawCustomItem(Actor* thisx, PlayState* play) {
    auto randoSaveCheck = RANDO_SAVE_CHECKS[RC_BOMB_SHOP_ITEM_01];

    MM_Matrix_Scale(20.0f, 20.0f, 20.0f, MTXMODE_APPLY);
    MM_Matrix_Translate(23.0f, 0.0f, -43.0f, MTXMODE_APPLY);

    s16 rotX = (s16)((-90.0f / 180.0f) * 32768.0f);
    MM_Matrix_RotateZYX(rotX, 0, 0, MTXMODE_APPLY);

#ifdef RSBS_SINGLE_EXECUTABLE
    // #800 pass 1: the slot may host an OoT item while holding MM's junk cover.
    // The owner holds the origin's own model, or, with no drawable model, MM's
    // model-less form (RI_NONE), never the cover's model.
    if (Rando::Foreign::IsForeignCheck(RC_BOMB_SHOP_ITEM_01)) {
        if (!Rando::Foreign::DrawForeignModelForCheck(RC_BOMB_SHOP_ITEM_01, play)) {
            Rando::DrawItem(RI_NONE);
        }
        return;
    }
#endif

    Rando::DrawItem(randoSaveCheck.randoItemId);
}

static MtxF sLeftHandMtxF;

void Rando::ActorBehavior::InitEnSob1Behavior() {
    COND_VB_SHOULD(VB_DRAW_ITEM_FROM_SOB1, IS_RANDO, {
        Actor* actor = va_arg(args, Actor*);
        if (RANDO_SAVE_CHECKS[RC_BOMB_SHOP_ITEM_01].shuffled) {
            MM_Matrix_MtxFCopy(&sLeftHandMtxF, MM_Matrix_GetCurrent());
            *should = false;
        }
    });

    COND_ID_HOOK(OnActorDraw, ACTOR_EN_OSSAN, IS_RANDO, [](Actor* actor) {
        if (RANDO_SAVE_CHECKS[RC_BOMB_SHOP_ITEM_01].shuffled && actor->params == 2) { // Bomb Shop Owner
            MM_Matrix_Put(&sLeftHandMtxF);
            EnSob1_DrawCustomItem(actor, MM_gPlayState);
        }
    });
}