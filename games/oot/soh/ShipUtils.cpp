#include "ShipUtils.h"
#include <libultraship/libultraship.h>
#include <cstddef>
#include <random>
#include "soh_assets.h"

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"

extern float OTRGetAspectRatio();

extern f32 sFontWidths[144];
extern const char* OoT_fontTbl[140];
}

constexpr f32 fourByThree = 4.0f / 3.0f;

// Gets the additional ratio of the screen compared to the original 4:3 ratio, clamping to 1 if smaller
extern "C" f32 Ship_GetExtendedAspectRatioMultiplier() {
    f32 currentRatio = OTRGetAspectRatio();
    return MAX(currentRatio / fourByThree, 1.0f);
}

// Enables Extended Culling options on specific actors by applying an inverse ratio of the draw distance slider
// to the projected Z value of the actor. This tricks distance checks without having to replace hardcoded values.
// Requires that Ship_ExtendedCullingActorRestoreProjectedPos is called within the same function scope.
extern "C" void Ship_ExtendedCullingActorAdjustProjectedZ(Actor* actor) {
    s32 multiplier = CVarGetInteger("gEnhancements.Graphics.IncreaseActorDrawDistance", 1);
    multiplier = MAX(multiplier, 1);
    if (multiplier > 1) {
        actor->projectedPos.z /= multiplier;
    }
}

// Enables Extended Culling options on specific actors by applying an inverse ratio of the widescreen aspect ratio
// to the projected X value of the actor. This tricks distance checks without having to replace hardcoded values.
// Requires that Ship_ExtendedCullingActorRestoreProjectedPos is called within the same function scope.
extern "C" void Ship_ExtendedCullingActorAdjustProjectedX(Actor* actor) {
    if (CVarGetInteger("gEnhancements.Graphics.ActorCullingAccountsForWidescreen", 0)) {
        f32 ratioAdjusted = Ship_GetExtendedAspectRatioMultiplier();
        actor->projectedPos.x /= ratioAdjusted;
    }
}

// Restores the projectedPos values on the actor after modifications from the Extended Culling hacks.
//
// This body was commented out because it was copied verbatim from 2Ship and
// calls Actor_GetProjectedPos, which MM has (games/mm/src/code/z_actor.c) and
// OoT does not — OoT computes the same thing inline in z_actor.c:3065. The
// dead declaration plus a one-parameter no-op stub in src/common/mm_stubs.c
// meant the only definition in the whole link was that stub, and the restore
// silently did nothing for BOTH games with an arity mismatch on top (#382).
//
// Reimplemented here against OoT's own projection helper so that the declared
// symbol has a real definition. No OoT code calls it today (verified by grep:
// the only Ship_ExtendedCullingActor* call sites in the tree are MM actor
// overlays), so this is not a behavior change for OoT — it closes a latent
// trap where a future OoT caller would have gotten a silent no-op instead of
// either working code or a link error.
extern "C" void Ship_ExtendedCullingActorRestoreProjectedPos(PlayState* play, Actor* actor) {
    f32 invW = 0.0f;
    OoT_SkinMatrix_Vec3fMtxFMultXYZW(&play->viewProjectionMtxF, &actor->world.pos, &actor->projectedPos, &invW);
    invW = (invW < 1.0f) ? 1.0f : (1.0f / invW);
    (void)invW;
}

// Reports OoT's own Actor::projectedPos offset, compiled with OoT's headers
// and production flags. Consumed by the mm-culling-binding lock
// (games/mm/2s2h/mm_culling_test.cpp), which compares it against MM's to prove
// — rather than assume — that the two ports' Actor layouts disagree and so
// cannot share one culling implementation. See #382.
extern "C" size_t OoT_ActorProjectedPosOffset() {
    return offsetof(Actor, projectedPos);
}

extern "C" bool Ship_IsCStringEmpty(const char* str) {
    return str == NULL || str[0] == '\0';
}

// Build vertex coordinates for a quad command
// In order of top left, top right, bottom left, then bottom right
// Supports flipping the texture horizontally
extern "C" void Ship_CreateQuadVertexGroup(Vtx* vtxList, s32 xStart, s32 yStart, s32 width, s32 height, u8 flippedH) {
    vtxList[0].v.ob[0] = xStart;
    vtxList[0].v.ob[1] = yStart;
    vtxList[0].v.tc[0] = (flippedH ? width : 0) << 5;
    vtxList[0].v.tc[1] = 0 << 5;

    vtxList[1].v.ob[0] = xStart + width;
    vtxList[1].v.ob[1] = yStart;
    vtxList[1].v.tc[0] = (flippedH ? width * 2 : width) << 5;
    vtxList[1].v.tc[1] = 0 << 5;

    vtxList[2].v.ob[0] = xStart;
    vtxList[2].v.ob[1] = yStart + height;
    vtxList[2].v.tc[0] = (flippedH ? width : 0) << 5;
    vtxList[2].v.tc[1] = height << 5;

    vtxList[3].v.ob[0] = xStart + width;
    vtxList[3].v.ob[1] = yStart + height;
    vtxList[3].v.tc[0] = (flippedH ? width * 2 : width) << 5;
    vtxList[3].v.tc[1] = height << 5;
}

extern "C" f32 Ship_GetCharFontWidth(u8 character) {
    u8 adjustedChar = character - ' ';

    if (adjustedChar >= ARRAY_COUNTU(sFontWidths)) {
        return 0.0f;
    }

    return sFontWidths[adjustedChar];
}

extern "C" void* Ship_GetCharFontTexture(u8 character) {
    u8 adjustedChar = character - ' ';

    if (adjustedChar >= ARRAY_COUNTU(OoT_fontTbl)) {
        return (void*)gEmptyTexture;
    }

    return (void*)OoT_fontTbl[adjustedChar];
}

static bool default_init = false;
uint64_t default_state = 0;
const uint64_t multiplier = 6364136223846793005ULL;
const uint64_t increment = 11634580027462260723ULL;

// Initialize with seed specified
void ShipUtils::RandInit(uint64_t seed, uint64_t* state) {
    if (state == nullptr) {
        state = &default_state;
    }
    *state = seed;
}

uint32_t ShipUtils::next32(uint64_t* state) {
    if (state == nullptr) {
        state = &default_state;
        if (!default_init) {
            // No seed given, get a random number from device to seed
#if !defined(__SWITCH__) && !defined(__WIIU__)
            uint64_t seed = static_cast<uint64_t>(std::random_device{}());
#else
            uint64_t seed = static_cast<uint64_t>(rand());
#endif
            default_init = true;
            ShipUtils::RandInit(seed, state);
        }
    }

    *state = *state * multiplier + increment;
    uint32_t xorshifted = static_cast<uint32_t>(((*state >> 18) ^ *state) >> 27);
    uint32_t rot = static_cast<int>(*state >> 59);
    return std::rotr(xorshifted, rot);
}

// Returns a random integer in range [min, max-1]
uint32_t ShipUtils::Random(uint32_t min, uint32_t max, uint64_t* state) {
    if (min == max) {
        return min;
    }
    assert(max > min);

    uint32_t n = max - min;
    uint32_t cutoff = UINT32_MAX - UINT32_MAX % static_cast<uint32_t>(n);
    for (;;) {
        uint32_t r = next32(state);
        if (r <= cutoff) {
            return min + r % n;
        }
    }
}

// Returns a random floating point number in [0.0, 1.0)
double ShipUtils::RandomDouble(uint64_t* state) {
    return ldexp(next32(state), -32);
}
