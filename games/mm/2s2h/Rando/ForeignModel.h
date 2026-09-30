#ifndef RANDO_FOREIGN_MODEL_H
#define RANDO_FOREIGN_MODEL_H

/**
 * A foreign (OoT-origin) item's real get-item MODEL in MM (#577 M3, the first
 * consumer of src/common/foreign_model.h). Implemented in
 * ForeignModelSingleExe.cpp, beside MM's own source for the other direction.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#ifdef __cplusplus
#include "Rando/Types.h" // RandoCheckId

struct PlayState;

namespace Rando {
namespace Foreign {

/** Draw the origin game's model of the foreign item `randoCheckId` hosts, under
 *  the current matrix, and return true; or draw nothing and return false when
 *  the check hosts nothing, the origin's answer is not a drawable DESCRIPTOR
 *  (no model, or a colliding model MM has no host-native row for), or a part's
 *  path is not in a mounted archive. false leaves the caller's stand-in. */
bool DrawForeignModelForCheck(RandoCheckId randoCheckId, PlayState* play);

} // namespace Foreign
} // namespace Rando
#endif

#endif // RSBS_SINGLE_EXECUTABLE
#endif // RANDO_FOREIGN_MODEL_H
