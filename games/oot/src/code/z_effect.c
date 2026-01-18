#include "global.h"

EffectContext OoT_sEffectContext;

EffectInfo OoT_sEffectInfoTable[] = {
    {
        sizeof(EffectSpark),
        OoT_EffectSpark_Init,
        OoT_EffectSpark_Destroy,
        OoT_EffectSpark_Update,
        OoT_EffectSpark_Draw,
    },
    {
        sizeof(EffectBlure),
        OoT_EffectBlure_Init1,
        OoT_EffectBlure_Destroy,
        OoT_EffectBlure_Update,
        OoT_EffectBlure_Draw,
    },
    {
        sizeof(EffectBlure),
        OoT_EffectBlure_Init2,
        OoT_EffectBlure_Destroy,
        OoT_EffectBlure_Update,
        OoT_EffectBlure_Draw,
    },
    {
        sizeof(EffectShieldParticle),
        OoT_EffectShieldParticle_Init,
        OoT_EffectShieldParticle_Destroy,
        OoT_EffectShieldParticle_Update,
        OoT_EffectShieldParticle_Draw,
    },
};

PlayState* OoT_Effect_GetPlayState(void) {
    return OoT_sEffectContext.play;
}

void* OoT_Effect_GetByIndex(s32 index) {
    if (index == TOTAL_EFFECT_COUNT) {
        return NULL;
    }

    if (index < SPARK_COUNT) {
        if (OoT_sEffectContext.sparks[index].status.active == true) {
            return &OoT_sEffectContext.sparks[index].effect;
        } else {
            return NULL;
        }
    }

    index -= SPARK_COUNT;
    if (index < BLURE_COUNT) {
        if (OoT_sEffectContext.blures[index].status.active == true) {
            return &OoT_sEffectContext.blures[index].effect;
        } else {
            return NULL;
        }
    }

    index -= BLURE_COUNT;
    if (index < SHIELD_PARTICLE_COUNT) {
        if (OoT_sEffectContext.shieldParticles[index].status.active == true) {
            return &OoT_sEffectContext.shieldParticles[index].effect;
        } else {
            return NULL;
        }
    }

    return NULL;
}

void OoT_Effect_InitStatus(EffectStatus* status) {
    status->active = false;
    status->unk_01 = 0;
    status->unk_02 = 0;
}

void Effect_InitContext(PlayState* play) {
    s32 i;

    for (i = 0; i < SPARK_COUNT; i++) {
        OoT_Effect_InitStatus(&OoT_sEffectContext.sparks[i].status);
    }

    for (i = 0; i < BLURE_COUNT; i++) {
        OoT_Effect_InitStatus(&OoT_sEffectContext.blures[i].status);
    }

    for (i = 0; i < SHIELD_PARTICLE_COUNT; i++) {
        //! @bug This is supposed to initialize shieldParticles, not blures again
        OoT_Effect_InitStatus(&OoT_sEffectContext.blures[i].status);
    }

    OoT_sEffectContext.play = play;
}

void OoT_Effect_Add(PlayState* play, s32* pIndex, s32 type, u8 arg3, u8 arg4, void* initParams) {
    s32 i;
    u32 slotFound;
    void* effect = NULL;
    EffectStatus* status = NULL;

    *pIndex = TOTAL_EFFECT_COUNT;

    if (OoT_FrameAdvance_IsEnabled(play) != true) {
        slotFound = false;
        switch (type) {
            case EFFECT_SPARK:
                for (i = 0; i < SPARK_COUNT; i++) {
                    if (OoT_sEffectContext.sparks[i].status.active == false) {
                        slotFound = true;
                        *pIndex = i;
                        effect = &OoT_sEffectContext.sparks[i].effect;
                        status = &OoT_sEffectContext.sparks[i].status;
                        break;
                    }
                }
                break;
            case EFFECT_BLURE1:
            case EFFECT_BLURE2:
                for (i = 0; i < BLURE_COUNT; i++) {
                    if (OoT_sEffectContext.blures[i].status.active == false) {
                        slotFound = true;
                        *pIndex = i + SPARK_COUNT;
                        effect = &OoT_sEffectContext.blures[i].effect;
                        status = &OoT_sEffectContext.blures[i].status;
                        break;
                    }
                }
                break;
            case EFFECT_SHIELD_PARTICLE:
                for (i = 0; i < SHIELD_PARTICLE_COUNT; i++) {
                    if (OoT_sEffectContext.shieldParticles[i].status.active == false) {
                        slotFound = true;
                        *pIndex = i + SPARK_COUNT + BLURE_COUNT;
                        effect = &OoT_sEffectContext.shieldParticles[i].effect;
                        status = &OoT_sEffectContext.shieldParticles[i].status;
                        break;
                    }
                }
                break;
        }

        if (!slotFound) {
            // "EffectAdd(): I cannot secure it. Be careful. Type %d"
            osSyncPrintf("EffectAdd():確保できません。注意してください。Type%d\n", type);
            osSyncPrintf("エフェクト追加せずに終了します。\n"); // "Exit without adding the effect."
        } else {
            OoT_sEffectInfoTable[type].init(effect, initParams);
            status->unk_02 = arg3;
            status->unk_01 = arg4;
            status->active = true;
        }
    }
}

void OoT_Effect_DrawAll(GraphicsContext* gfxCtx) {
    s32 i;

    for (i = 0; i < SPARK_COUNT; i++) {
        if (OoT_sEffectContext.sparks[i].status.active) {
            OoT_sEffectInfoTable[EFFECT_SPARK].draw(&OoT_sEffectContext.sparks[i].effect, gfxCtx);
        }
    }

    for (i = 0; i < BLURE_COUNT; i++) {
        if (OoT_sEffectContext.blures[i].status.active) {
            OoT_sEffectInfoTable[EFFECT_BLURE1].draw(&OoT_sEffectContext.blures[i].effect, gfxCtx);
        }
    }

    for (i = 0; i < SHIELD_PARTICLE_COUNT; i++) {
        if (OoT_sEffectContext.shieldParticles[i].status.active) {
            OoT_sEffectInfoTable[EFFECT_SHIELD_PARTICLE].draw(&OoT_sEffectContext.shieldParticles[i].effect, gfxCtx);
        }
    }
}

void OoT_Effect_UpdateAll(PlayState* play) {
    s32 i;

    for (i = 0; i < SPARK_COUNT; i++) {
        if (OoT_sEffectContext.sparks[i].status.active) {
            if (OoT_sEffectInfoTable[EFFECT_SPARK].update(&OoT_sEffectContext.sparks[i].effect) == 1) {
                Effect_Delete(play, i);
            }
        }
    }

    for (i = 0; i < BLURE_COUNT; i++) {
        if (OoT_sEffectContext.blures[i].status.active) {
            if (OoT_sEffectInfoTable[EFFECT_BLURE1].update(&OoT_sEffectContext.blures[i].effect) == 1) {
                Effect_Delete(play, i + SPARK_COUNT);
            }
        }
    }

    for (i = 0; i < SHIELD_PARTICLE_COUNT; i++) {
        if (OoT_sEffectContext.shieldParticles[i].status.active) {
            if (OoT_sEffectInfoTable[EFFECT_SHIELD_PARTICLE].update(&OoT_sEffectContext.shieldParticles[i].effect) == 1) {
                Effect_Delete(play, i + SPARK_COUNT + BLURE_COUNT);
            }
        }
    }
}

void Effect_Delete(PlayState* play, s32 index) {
    if (index == TOTAL_EFFECT_COUNT) {
        return;
    }

    if (index < SPARK_COUNT) {
        OoT_sEffectContext.sparks[index].status.active = false;
        OoT_sEffectInfoTable[EFFECT_SPARK].destroy(&OoT_sEffectContext.sparks[index].effect);
        return;
    }

    index -= SPARK_COUNT;
    if (index < BLURE_COUNT) {
        OoT_sEffectContext.blures[index].status.active = false;
        OoT_sEffectInfoTable[EFFECT_BLURE1].destroy(&OoT_sEffectContext.blures[index].effect);
        return;
    }

    index -= BLURE_COUNT;
    if (index < SHIELD_PARTICLE_COUNT) {
        OoT_sEffectContext.shieldParticles[index].status.active = false;
        OoT_sEffectInfoTable[EFFECT_SHIELD_PARTICLE].destroy(&OoT_sEffectContext.shieldParticles[index].effect);
        return;
    }
}

void Effect_DeleteAll(PlayState* play) {
    s32 i;

    osSyncPrintf("エフェクト総て解放\n"); // "All effect release"

    for (i = 0; i < SPARK_COUNT; i++) {
        OoT_sEffectContext.sparks[i].status.active = false;
        OoT_sEffectInfoTable[EFFECT_SPARK].destroy(&OoT_sEffectContext.sparks[i].effect);
    }

    for (i = 0; i < BLURE_COUNT; i++) {
        OoT_sEffectContext.blures[i].status.active = false;
        OoT_sEffectInfoTable[EFFECT_BLURE1].destroy(&OoT_sEffectContext.blures[i].effect);
    }

    for (i = 0; i < SHIELD_PARTICLE_COUNT; i++) {
        OoT_sEffectContext.shieldParticles[i].status.active = false;
        OoT_sEffectInfoTable[EFFECT_SHIELD_PARTICLE].destroy(&OoT_sEffectContext.shieldParticles[i].effect);
    }

    osSyncPrintf("エフェクト総て解放 終了\n"); // "All effects release End"
}
