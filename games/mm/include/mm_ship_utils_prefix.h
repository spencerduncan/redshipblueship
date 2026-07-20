#ifndef MM_SHIP_UTILS_PREFIX_H
#define MM_SHIP_UTILS_PREFIX_H

/**
 * Single-exe symbol-prefix shim for MM's extended-culling helpers.
 *
 * Both ports define an identically-named extern "C" Ship_ExtendedCullingActor*
 * API that takes an `Actor*` — and the two games' Actor structs are NOT the
 * same shape. `games/mm/2s2h/ShipUtils.cpp` is excluded from the single-exe
 * build (games/mm/CMakeLists.txt) because the rest of that file collides
 * wholesale with OoT's soh/ShipUtils.cpp, so exactly one definition of each
 * helper survived the link and it was OoT's. MM's actor overlays call these
 * unprefixed, so MM's calls executed OoT's bodies against MM's layout:
 *
 *   projectedPos is at 0x0E4 in OoT's Actor (games/oot/include/z64actor.h)
 *   and 0x0EC in MM's (games/mm/include/z64actor.h) — an 8-byte skew.
 *
 *   - AdjustProjectedZ touches OoT 0x0E4+8 = 0x0EC. In an MM Actor that is
 *     projectedPos.X. So MM's draw-distance option scaled the wrong axis and
 *     never actually extended draw distance.
 *   - AdjustProjectedX touches OoT 0x0E4. In an MM Actor that is
 *     shape.feetPos[1].y — a float the shadow/limb code reads. So MM's
 *     widescreen-culling option silently corrupted a foot position.
 *
 * Neither is a link error: there is only ONE definition, so GNU ld (this
 * repo's only working ODR gate — Windows links with /FORCE:MULTIPLE by
 * design, CMakeLists.txt:274) has nothing to reject. Silent corruption.
 *
 * The rename must be visible in EVERY MM TU that references the family, or
 * the untouched TUs keep binding OoT's bodies. Every MM caller reaches this
 * shim through "2s2h/ShipUtils.h", which is where the declarations live and
 * which all six calling overlays already include:
 *   ovl_En_Ishi, ovl_En_Kusa, ovl_En_Kusa2, ovl_Obj_Bombiwa, ovl_Obj_Hamishi,
 *   ovl_Obj_Mure.
 * Unlike the FrameInterpolation family there is no macro-expansion back door
 * (OPEN_DISPS et al.) that calls these without including the header. The
 * ROM-free lock mm-culling-binding (games/mm/2s2h/mm_culling_test.cpp)
 * asserts the macro is actually in force by comparing the address the
 * unprefixed source-level name resolves to against MM_'s.
 *
 * The soh side keeps the unprefixed names — only MM is renamed, mirroring the
 * repo-wide MM_ prefix convention for cross-game symbol collisions.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#define Ship_ExtendedCullingActorAdjustProjectedZ MM_Ship_ExtendedCullingActorAdjustProjectedZ
#define Ship_ExtendedCullingActorAdjustProjectedX MM_Ship_ExtendedCullingActorAdjustProjectedX
#define Ship_ExtendedCullingActorRestoreProjectedPos MM_Ship_ExtendedCullingActorRestoreProjectedPos

/*
 * Lane C0 (#392): 2s2h/ShipUtils.cpp is COMPILED in single-exe builds now —
 * un-eliding 2ship_rando made its MM-unique surface (Ship_Random/_Seed,
 * Ship_Hash, Ship_GetSceneName, Ship_GetItemColorTint, the NES font helpers,
 * ...) a hard link dependency of the randomizer. The names below are the
 * subset that ALSO exists on the OoT side, where exactly one definition would
 * survive the link and cross-bind (the silent-corruption class this header
 * documents above), so MM's copies get the MM_ prefix. Same rename-at-the-
 * header mechanism: every MM TU reaches these through 2s2h/ShipUtils.h.
 *
 * Ship_IsCStringEmpty / Ship_CreateQuadVertexGroup are semantically identical
 * twins of OoT's (renamed to satisfy the #375 strong-collision gate, not
 * because the cross-bind was harmful); Ship_GetExtendedAspectRatioMultiplier
 * reads the shared window so either copy is correct; LoadGuiTextures /
 * digitList / GetActor* collide with OoT declarations (soh/ShipUtils.h,
 * TimeDisplay.cpp, debugger/actorViewer.cpp) with identical manglings.
 */
#define Ship_GetExtendedAspectRatioMultiplier MM_Ship_GetExtendedAspectRatioMultiplier
#define Ship_IsCStringEmpty MM_Ship_IsCStringEmpty
#define Ship_CreateQuadVertexGroup MM_Ship_CreateQuadVertexGroup
#define LoadGuiTextures MM_LoadGuiTextures
#define digitList MM_digitList
#define GetActorDescription MM_GetActorDescription
#define GetActorDebugName MM_GetActorDebugName
#define GetActorCategoryName MM_GetActorCategoryName

#endif // RSBS_SINGLE_EXECUTABLE

#endif // MM_SHIP_UTILS_PREFIX_H
