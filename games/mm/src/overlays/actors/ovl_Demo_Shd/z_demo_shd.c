/*
 * File: z_demo_shd.c
 * Overlay: ovl_Demo_Shd
 * Description:
 */

#include "z_demo_shd.h"

#define FLAGS (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

void MM_DemoShd_Init(Actor* thisx, PlayState* play);
void MM_DemoShd_Destroy(Actor* thisx, PlayState* play);
void MM_DemoShd_Update(Actor* thisx, PlayState* play);
void MM_DemoShd_Draw(Actor* thisx, PlayState* play);

ActorProfile Demo_Shd_Profile = {
    /**/ ACTOR_DEMO_SHD,
    /**/ ACTORCAT_ENEMY,
    /**/ FLAGS,
    /**/ OBJECT_FWALL,
    /**/ sizeof(DemoShd),
    /**/ MM_DemoShd_Init,
    /**/ MM_DemoShd_Destroy,
    /**/ MM_DemoShd_Update,
    /**/ MM_DemoShd_Draw,
};

void MM_DemoShd_Init(Actor* thisx, PlayState* play) {
}
void MM_DemoShd_Destroy(Actor* thisx, PlayState* play) {
}
void MM_DemoShd_Update(Actor* thisx, PlayState* play) {
}
void MM_DemoShd_Draw(Actor* thisx, PlayState* play) {
}
