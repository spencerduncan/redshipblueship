#ifndef ACTOR_LIST_INDEX_H
#define ACTOR_LIST_INDEX_H

#ifdef RSBS_SINGLE_EXECUTABLE
// Single-exe rename (Lane C0, #392): OoT's always-linked
// soh/ObjectExtension/ActorListIndex.cpp defines identically-mangled
// GetActorListIndex/SetActorListIndex, and src/common/mm_stubs.c used to
// paper over currentActorListIndex with an `int` stub behind this `s16`
// declaration. MM's ActorListIndex TU is compiled now; every MM caller
// (z_actor.c included) reaches these names through this header, so the MM_
// prefix here re-points reference and definition together.
#define GetActorListIndex MM_GetActorListIndex
#define SetActorListIndex MM_SetActorListIndex
#define currentActorListIndex MM_currentActorListIndex
#endif

#ifdef __cplusplus
extern "C" {
#include "z64actor.h"
#endif

int16_t GetActorListIndex(const Actor* actor);
void SetActorListIndex(const Actor* actor, int16_t index);
extern s16 currentActorListIndex;

#ifdef __cplusplus
}
#endif

#endif // ACTOR_LIST_INDEX_H
