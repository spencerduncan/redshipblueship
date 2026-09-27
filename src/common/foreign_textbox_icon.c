/**
 * @file foreign_textbox_icon.c
 * @brief Registry for the origin-answered textbox icon of a cross-game item
 *        (#607). See foreign_textbox_icon.h.
 */

#include "foreign_textbox_icon.h"

#include <string.h>

#include "game.h"

// Indexed by GameId; GAME_NONE's slot is never written.
static ComboTextboxIconFn sTextboxIconSources[3];

static int TextboxIconSlotValid(uint8_t game) {
    return game == (uint8_t)GAME_OOT || game == (uint8_t)GAME_MM;
}

void Combo_RegisterTextboxIconSource(uint8_t game, ComboTextboxIconFn source) {
    if (!TextboxIconSlotValid(game)) {
        return;
    }
    sTextboxIconSources[game] = source;
}

ComboTextboxIconFn Combo_GetTextboxIconSource(uint8_t game) {
    return TextboxIconSlotValid(game) ? sTextboxIconSources[game] : NULL;
}

int Combo_GetForeignItemTextboxIcon(SharedItem item, ComboTextboxIcon* out) {
    ComboTextboxIcon icon;
    memset(&icon, 0, sizeof(icon));
    if (out != NULL) {
        *out = icon;
    }
    if (!TextboxIconSlotValid(item.originGame)) {
        return 0;
    }
    ComboTextboxIconFn source = sTextboxIconSources[item.originGame];
    if (source == NULL || source(item.id, &icon) != 1) {
        return 0;
    }
    // A half-answer is no answer: the host would load an empty path, or read a
    // texture with a layout it cannot name.
    if (icon.texture == NULL || icon.texture[0] == '\0' || icon.shape == (uint8_t)COMBO_TEXTBOX_ICON_NONE ||
        icon.shape >= (uint8_t)COMBO_TEXTBOX_ICON_SHAPE_COUNT) {
        return 0;
    }
    if (out != NULL) {
        *out = icon;
    }
    return 1;
}
