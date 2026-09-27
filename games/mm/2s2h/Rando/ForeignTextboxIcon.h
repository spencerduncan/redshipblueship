#ifndef RANDO_FOREIGN_TEXTBOX_ICON_H
#define RANDO_FOREIGN_TEXTBOX_ICON_H

/**
 * The foreign item's icon inside MM's blue get-item textbox (#607, Tier 2b of
 * #494). Implemented in ForeignTextboxIconSingleExe.cpp; see its header comment
 * for the whole path (origin answer -> CustomMessage::Entry -> the one fenced
 * hook in z_message.c's Message_DecodeHeader).
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <stdint.h>

#ifdef __cplusplus
#include "Rando/Types.h" // RandoCheckId

namespace Rando {
namespace Foreign {

/** The icon to draw for the foreign item hosted by `randoCheckId`: true with the
 *  origin game's texture path and the MM item id whose textbox branch draws that
 *  texture's layout, or false (and nullptr / 0xFE) when the check hosts nothing,
 *  the origin has no icon for the item, or the texture's archive is not mounted.
 *  false is the pre-#607 presentation: the textbox carries no icon. */
bool ForeignTextboxIconForCheck(RandoCheckId randoCheckId, const char** texture, uint8_t* textboxItemId);

} // namespace Foreign
} // namespace Rando

extern "C" {
#endif

struct PlayState;

/** Called by CustomMessage::LoadCustomMessageIntoFont for EVERY custom message:
 *  arms the icon for the message just loaded, or (nullptr / 0xFE) disarms. */
void MM_ForeignTextboxIcon_Arm(const char* texture, uint8_t textboxItemId);

/** The fenced hook at the end of z_message.c's header decode. Consumes the arm;
 *  applies it only to the custom message it was armed for. */
void MM_ForeignTextboxIcon_OnDecodeHeader(struct PlayState* play);

#ifdef __cplusplus
}
#endif

#endif // RSBS_SINGLE_EXECUTABLE
#endif // RANDO_FOREIGN_TEXTBOX_ICON_H
