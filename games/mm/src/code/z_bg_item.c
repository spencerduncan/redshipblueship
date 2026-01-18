#include "global.h"

/**
 * @param transformFlags How other actors standing on the dynapoly actor's collision move when the dynapoly actor moves.
 *   See `DYNA_TRANSFORM_POS`, `DYNA_TRANSFORM_ROT_Y`.
 */
void MM_DynaPolyActor_Init(DynaPolyActor* dynaActor, s32 transformFlags) {
    dynaActor->bgId = -1;
    dynaActor->pushForce = 0.0f;
    dynaActor->unk14C = 0.0f;
    dynaActor->transformFlags = transformFlags;
    dynaActor->interactFlags = 0;
}

void DynaPolyActor_LoadMesh(PlayState* play, DynaPolyActor* dynaActor, CollisionHeader* meshHeader) {
    CollisionHeader* header = NULL;

    MM_CollisionHeader_GetVirtual(meshHeader, &header);
    dynaActor->bgId = MM_DynaPoly_SetBgActor(play, &play->colCtx.dyna, &dynaActor->actor, header);
}

void MM_DynaPolyActor_UnsetAllInteractFlags(DynaPolyActor* dynaActor) {
    dynaActor->interactFlags = 0;
}

void MM_DynaPolyActor_SetActorOnTop(DynaPolyActor* dynaActor) {
    dynaActor->interactFlags |= DYNA_INTERACT_ACTOR_ON_TOP;
}

void MM_DynaPolyActor_SetPlayerOnTop(DynaPolyActor* dynaActor) {
    dynaActor->interactFlags |= DYNA_INTERACT_PLAYER_ON_TOP;
}

void MM_DynaPoly_SetPlayerOnTop(CollisionContext* colCtx, s32 bgId) {
    DynaPolyActor* dynaActor = MM_DynaPoly_GetActor(colCtx, bgId);

    if (dynaActor != NULL) {
        MM_DynaPolyActor_SetPlayerOnTop(dynaActor);
    }
}

void MM_DynaPolyActor_SetPlayerAbove(DynaPolyActor* dynaActor) {
    dynaActor->interactFlags |= DYNA_INTERACT_PLAYER_ABOVE;
}

void MM_DynaPoly_SetPlayerAbove(CollisionContext* colCtx, s32 bgId) {
    DynaPolyActor* dynaActor = MM_DynaPoly_GetActor(colCtx, bgId);

    if (dynaActor != NULL) {
        MM_DynaPolyActor_SetPlayerAbove(dynaActor);
    }
}

void MM_DynaPolyActor_SetSwitchPressed(DynaPolyActor* dynaActor) {
    dynaActor->interactFlags |= DYNA_INTERACT_ACTOR_SWITCH_PRESSED;
}

void DynaPolyActor_SetHeavySwitchPressed(DynaPolyActor* dynaActor) {
    dynaActor->interactFlags |= DYNA_INTERACT_ACTOR_HEAVY_SWITCH_PRESSED;
}

s32 MM_DynaPolyActor_IsActorOnTop(DynaPolyActor* dynaActor) {
    if (dynaActor->interactFlags & DYNA_INTERACT_ACTOR_ON_TOP) {
        return true;
    } else {
        return false;
    }
}

s32 MM_DynaPolyActor_IsPlayerOnTop(DynaPolyActor* dynaActor) {
    if (dynaActor->interactFlags & DYNA_INTERACT_PLAYER_ON_TOP) {
        return true;
    } else {
        return false;
    }
}

s32 MM_DynaPolyActor_IsPlayerAbove(DynaPolyActor* dynaActor) {
    if (dynaActor->interactFlags & DYNA_INTERACT_PLAYER_ABOVE) {
        return true;
    } else {
        return false;
    }
}

s32 MM_DynaPolyActor_IsSwitchPressed(DynaPolyActor* dynaActor) {
    if (dynaActor->interactFlags & DYNA_INTERACT_ACTOR_SWITCH_PRESSED) {
        return true;
    } else {
        return false;
    }
}

s32 DynaPolyActor_IsHeavySwitchPressed(DynaPolyActor* dynaActor) {
    if (dynaActor->interactFlags & DYNA_INTERACT_ACTOR_HEAVY_SWITCH_PRESSED) {
        return true;
    } else {
        return false;
    }
}

s32 DynaPolyActor_ValidateMove(PlayState* play, DynaPolyActor* dynaActor, s16 startRadius, s16 endRadius,
                               s16 startHeight) {
    Vec3f startPos;
    Vec3f endPos;
    Vec3f intersectionPos;
    f32 sin = MM_Math_SinS(dynaActor->yRotation);
    f32 cos = MM_Math_CosS(dynaActor->yRotation);
    s32 bgId;
    CollisionPoly* poly;
    f32 adjustedStartRadius;
    f32 adjustedEndRadius;
    f32 sign = (0.0f <= dynaActor->pushForce) ? 1.0f : -1.0f;

    adjustedStartRadius = (f32)startRadius - 0.1f;
    startPos.x = dynaActor->actor.world.pos.x + (adjustedStartRadius * cos);
    startPos.y = dynaActor->actor.world.pos.y + startHeight;
    startPos.z = dynaActor->actor.world.pos.z - (adjustedStartRadius * sin);

    adjustedEndRadius = (f32)endRadius - 0.1f;
    endPos.x = sign * adjustedEndRadius * sin + startPos.x;
    endPos.y = startPos.y;
    endPos.z = sign * adjustedEndRadius * cos + startPos.z;

    if (MM_BgCheck_EntityLineTest3(&play->colCtx, &startPos, &endPos, &intersectionPos, &poly, true, false, false, true,
                                &bgId, &dynaActor->actor, 0.0f)) {
        return false;
    }

    startPos.x = (dynaActor->actor.world.pos.x * 2.0f) - startPos.x;
    startPos.z = (dynaActor->actor.world.pos.z * 2.0f) - startPos.z;
    endPos.x = sign * adjustedEndRadius * sin + startPos.x;
    endPos.z = sign * adjustedEndRadius * cos + startPos.z;

    if (MM_BgCheck_EntityLineTest3(&play->colCtx, &startPos, &endPos, &intersectionPos, &poly, true, false, false, true,
                                &bgId, &dynaActor->actor, 0.0f)) {
        return false;
    }

    return true;
}
