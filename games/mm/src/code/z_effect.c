#include "z64effect.h"
#include "global.h"

#define SPARK_COUNT 3
#define BLURE_COUNT 25
#define SHIELD_PARTICLE_COUNT 3
#define TIRE_MARK_COUNT 15

#define TOTAL_EFFECT_COUNT SPARK_COUNT + BLURE_COUNT + SHIELD_PARTICLE_COUNT + TIRE_MARK_COUNT

typedef struct EffectStatus {
    /* 0x0 */ u8 active;
    /* 0x1 */ u8 unk1;
    /* 0x2 */ u8 unk2;
} EffectStatus; // size = 0x3

typedef struct EffectContext {
    /* 0x0000 */ struct PlayState* play;
    struct {
        EffectStatus status;
        EffectSpark effect;
    } /* 0x0004 */ sparks[SPARK_COUNT];
    struct {
        EffectStatus status;
        EffectBlure effect;
    } /* 0x0E5C */ blures[BLURE_COUNT];
    struct {
        EffectStatus status;
        EffectShieldParticle effect;
    } /* 0x388C */ shieldParticles[SHIELD_PARTICLE_COUNT];
    struct {
        EffectStatus status;
        EffectTireMark effect;
    } /* 0x3DF0 */ tireMarks[TIRE_MARK_COUNT];
} EffectContext; // size = 0x98E0

EffectContext MM_sEffectContext;

typedef struct EffectInfo {
    /* 0x00 */ u32 size;
    /* 0x04 */ void (*init)(void* effect, void* initParams);
    /* 0x08 */ void (*destroy)(void* effect);
    /* 0x0C */ s32 (*update)(void* effect);
    /* 0x10 */ void (*draw)(void* effect, struct GraphicsContext* gfxCtx);
} EffectInfo; // size = 0x14

EffectInfo MM_sEffectInfoTable[EFFECT_MAX] = {
    {
        sizeof(EffectSpark),
        MM_EffectSpark_Init,
        MM_EffectSpark_Destroy,
        MM_EffectSpark_Update,
        MM_EffectSpark_Draw,
    },
    {
        sizeof(EffectBlure),
        MM_EffectBlure_Init1,
        MM_EffectBlure_Destroy,
        MM_EffectBlure_Update,
        MM_EffectBlure_Draw,
    },
    {
        sizeof(EffectBlure),
        MM_EffectBlure_Init2,
        MM_EffectBlure_Destroy,
        MM_EffectBlure_Update,
        MM_EffectBlure_Draw,
    },
    {
        sizeof(EffectShieldParticle),
        MM_EffectShieldParticle_Init,
        MM_EffectShieldParticle_Destroy,
        MM_EffectShieldParticle_Update,
        MM_EffectShieldParticle_Draw,
    },
    {
        sizeof(EffectTireMark),
        EffectTireMark_Init,
        EffectTireMark_Destroy,
        EffectTireMark_Update,
        EffectTireMark_Draw,
    },
};

PlayState* MM_Effect_GetPlayState(void) {
    return MM_sEffectContext.play;
}

void* MM_Effect_GetByIndex(s32 index) {
    if (index == TOTAL_EFFECT_COUNT) {
        return NULL;
    }

    if (index < SPARK_COUNT) {
        if (MM_sEffectContext.sparks[index].status.active == true) {
            return &MM_sEffectContext.sparks[index].effect;
        } else {
            return NULL;
        }
    }

    index -= SPARK_COUNT;
    if (index < BLURE_COUNT) {
        if (MM_sEffectContext.blures[index].status.active == true) {
            return &MM_sEffectContext.blures[index].effect;
        } else {
            return NULL;
        }
    }

    index -= BLURE_COUNT;
    if (index < SHIELD_PARTICLE_COUNT) {
        if (MM_sEffectContext.shieldParticles[index].status.active == true) {
            return &MM_sEffectContext.shieldParticles[index].effect;
        } else {
            return NULL;
        }
    }

    index -= SHIELD_PARTICLE_COUNT;
    if (index < TIRE_MARK_COUNT) {
        if (MM_sEffectContext.tireMarks[index].status.active == true) {
            return &MM_sEffectContext.tireMarks[index].effect;
        } else {
            return NULL;
        }
    }

    return NULL;
}

void MM_Effect_InitStatus(EffectStatus* status) {
    status->active = false;
    status->unk1 = 0;
    status->unk2 = 0;
}

void Effect_Init(PlayState* play) {
    s32 i;

    for (i = 0; i < SPARK_COUNT; i++) {
        MM_Effect_InitStatus(&MM_sEffectContext.sparks[i].status);
    }

    for (i = 0; i < BLURE_COUNT; i++) {
        MM_Effect_InitStatus(&MM_sEffectContext.blures[i].status);
    }

    for (i = 0; i < SHIELD_PARTICLE_COUNT; i++) {
        //! @bug This is probably supposed to initialize shieldParticles, not blures again
        MM_Effect_InitStatus(&MM_sEffectContext.blures[i].status);
    }

    for (i = 0; i < TIRE_MARK_COUNT; i++) {
        MM_Effect_InitStatus(&MM_sEffectContext.tireMarks[i].status);
    }

    MM_sEffectContext.play = play;
}

void MM_Effect_Add(PlayState* play, s32* pIndex, EffectType type, u8 arg3, u8 arg4, void* initParams) {
    u32 slotFound;
    s32 i;
    void* effect = NULL;
    EffectStatus* status = NULL;

    *pIndex = TOTAL_EFFECT_COUNT;

    if (MM_FrameAdvance_IsEnabled(play) != true) {
        slotFound = false;
        switch (type) {
            case EFFECT_SPARK:
                for (i = 0; i < SPARK_COUNT; i++) {
                    if (MM_sEffectContext.sparks[i].status.active == false) {
                        slotFound = true;
                        *pIndex = i;
                        effect = &MM_sEffectContext.sparks[i].effect;
                        status = &MM_sEffectContext.sparks[i].status;
                        break;
                    }
                }
                break;

            case EFFECT_BLURE1:
            case EFFECT_BLURE2:
                for (i = 0; i < BLURE_COUNT; i++) {
                    if (MM_sEffectContext.blures[i].status.active == false) {
                        slotFound = true;
                        *pIndex = i + SPARK_COUNT;
                        effect = &MM_sEffectContext.blures[i].effect;
                        status = &MM_sEffectContext.blures[i].status;
                        break;
                    }
                }
                break;

            case EFFECT_SHIELD_PARTICLE:
                for (i = 0; i < SHIELD_PARTICLE_COUNT; i++) {
                    if (MM_sEffectContext.shieldParticles[i].status.active == false) {
                        slotFound = true;
                        *pIndex = i + SPARK_COUNT + BLURE_COUNT;
                        effect = &MM_sEffectContext.shieldParticles[i].effect;
                        status = &MM_sEffectContext.shieldParticles[i].status;
                        break;
                    }
                }
                break;

            case EFFECT_TIRE_MARK:
                for (i = 0; i < TIRE_MARK_COUNT; i++) {
                    if (MM_sEffectContext.tireMarks[i].status.active == false) {
                        slotFound = true;
                        *pIndex = i + SPARK_COUNT + BLURE_COUNT + SHIELD_PARTICLE_COUNT;
                        effect = &MM_sEffectContext.tireMarks[i].effect;
                        status = &MM_sEffectContext.tireMarks[i].status;
                        break;
                    }
                }
                break;

            default:
                break;
        }

        if (slotFound) {
            MM_sEffectInfoTable[type].init(effect, initParams);
            status->unk2 = arg3;
            status->unk1 = arg4;
            status->active = true;
        }
    }
}

void MM_Effect_DrawAll(GraphicsContext* gfxCtx) {
    s32 i;

    for (i = 0; i < SPARK_COUNT; i++) {
        if (!MM_sEffectContext.sparks[i].status.active) {
            continue;
        }
        MM_sEffectInfoTable[EFFECT_SPARK].draw(&MM_sEffectContext.sparks[i].effect, gfxCtx);
    }

    for (i = 0; i < BLURE_COUNT; i++) {
        if (!MM_sEffectContext.blures[i].status.active) {
            continue;
        }
        MM_sEffectInfoTable[EFFECT_BLURE1].draw(&MM_sEffectContext.blures[i].effect, gfxCtx);
    }

    for (i = 0; i < SHIELD_PARTICLE_COUNT; i++) {
        if (!MM_sEffectContext.shieldParticles[i].status.active) {
            continue;
        }
        MM_sEffectInfoTable[EFFECT_SHIELD_PARTICLE].draw(&MM_sEffectContext.shieldParticles[i].effect, gfxCtx);
    }

    for (i = 0; i < TIRE_MARK_COUNT; i++) {
        if (!MM_sEffectContext.tireMarks[i].status.active) {
            continue;
        }
        MM_sEffectInfoTable[EFFECT_TIRE_MARK].draw(&MM_sEffectContext.tireMarks[i].effect, gfxCtx);
    }
}

void MM_Effect_UpdateAll(PlayState* play) {
    s32 i;

    for (i = 0; i < SPARK_COUNT; i++) {
        if (!MM_sEffectContext.sparks[i].status.active) {
            continue;
        }
        if (MM_sEffectInfoTable[EFFECT_SPARK].update(&MM_sEffectContext.sparks[i].effect) == 1) {
            Effect_Destroy(play, i);
        }
    }

    for (i = 0; i < BLURE_COUNT; i++) {
        if (!MM_sEffectContext.blures[i].status.active) {
            continue;
        }
        if (MM_sEffectInfoTable[EFFECT_BLURE1].update(&MM_sEffectContext.blures[i].effect) == 1) {
            Effect_Destroy(play, i + SPARK_COUNT);
        }
    }

    for (i = 0; i < SHIELD_PARTICLE_COUNT; i++) {
        if (!MM_sEffectContext.shieldParticles[i].status.active) {
            continue;
        }
        if (MM_sEffectInfoTable[EFFECT_SHIELD_PARTICLE].update(&MM_sEffectContext.shieldParticles[i].effect) == 1) {
            Effect_Destroy(play, i + SPARK_COUNT + BLURE_COUNT);
        }
    }

    for (i = 0; i < TIRE_MARK_COUNT; i++) {
        if (!MM_sEffectContext.tireMarks[i].status.active) {
            continue;
        }
        if (MM_sEffectInfoTable[EFFECT_TIRE_MARK].update(&MM_sEffectContext.tireMarks[i].effect) == 1) {
            Effect_Destroy(play, i + SPARK_COUNT + BLURE_COUNT + SHIELD_PARTICLE_COUNT);
        }
    }
}

void Effect_Destroy(PlayState* play, s32 index) {
    if (index == TOTAL_EFFECT_COUNT) {
        return;
    }

    if (index < SPARK_COUNT) {
        MM_sEffectContext.sparks[index].status.active = false;
        MM_sEffectInfoTable[EFFECT_SPARK].destroy(&MM_sEffectContext.sparks[index].effect);
        return;
    }

    index -= SPARK_COUNT;
    if (index < BLURE_COUNT) {
        MM_sEffectContext.blures[index].status.active = false;
        MM_sEffectInfoTable[EFFECT_BLURE1].destroy(&MM_sEffectContext.blures[index].effect);
        return;
    }

    index -= BLURE_COUNT;
    if (index < SHIELD_PARTICLE_COUNT) {
        MM_sEffectContext.shieldParticles[index].status.active = false;
        MM_sEffectInfoTable[EFFECT_SHIELD_PARTICLE].destroy(&MM_sEffectContext.shieldParticles[index].effect);
        return;
    }

    index -= SHIELD_PARTICLE_COUNT;
    if (index < TIRE_MARK_COUNT) {
        MM_sEffectContext.tireMarks[index].status.active = false;
        MM_sEffectInfoTable[EFFECT_TIRE_MARK].destroy(&MM_sEffectContext.tireMarks[index].effect);
        return;
    }
}

void Effect_DestroyAll(PlayState* play) {
    s32 i;

    for (i = 0; i < SPARK_COUNT; i++) {
        MM_sEffectContext.sparks[i].status.active = false;
        MM_sEffectInfoTable[EFFECT_SPARK].destroy(&MM_sEffectContext.sparks[i].effect);
    }

    for (i = 0; i < BLURE_COUNT; i++) {
        MM_sEffectContext.blures[i].status.active = false;
        MM_sEffectInfoTable[EFFECT_BLURE1].destroy(&MM_sEffectContext.blures[i].effect);
    }

    for (i = 0; i < SHIELD_PARTICLE_COUNT; i++) {
        MM_sEffectContext.shieldParticles[i].status.active = false;
        MM_sEffectInfoTable[EFFECT_SHIELD_PARTICLE].destroy(&MM_sEffectContext.shieldParticles[i].effect);
    }

    for (i = 0; i < TIRE_MARK_COUNT; i++) {
        MM_sEffectContext.tireMarks[i].status.active = false;
        MM_sEffectInfoTable[EFFECT_TIRE_MARK].destroy(&MM_sEffectContext.tireMarks[i].effect);
    }
}
