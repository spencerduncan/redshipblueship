/**
 * @file warp_songs_mm.c
 * @brief Ocarina of Time's six warp songs as Majora's Mask items (#1169 lane
 *        XW5): the game-neutral rules and the ownership byte. The reasoning is in
 *        warp_songs_mm.h and on gComboCtx.mmWarpSongsOwned (context.h).
 */

#include "warp_songs_mm.h"

#include "context.h"       // gComboCtx.mmWarpSongsOwned
#include "foreign_items.h" // RSBS_XITEM2_SONG_*_MM, RSBS_XITEM2_WARP_SONG_SHARED_SHIFT

#include <stddef.h>

/** One song: its extension rule, its ownership bit and its names. */
typedef struct MMWarpSongInfo {
    uint32_t ruleBit;
    uint8_t ownedBit;
    const char* name;    // OoTMM's option name, the MM item's name
    const char* ootName; // Ocarina of Time's own item name
    const char* tooltip;
} MMWarpSongInfo;

static const MMWarpSongInfo kMMWarpSongs[RSBS_MM_WARP_SONG_COUNT] = {
    { RSBS_XITEM2_SONG_MINUET_MM, RSBS_MM_WARP_SONG_OWNED_MINUET, "Minuet of Forest (MM)", "Minuet of Forest",
      RSBS_MM_WARP_SONG_TOOLTIP_MINUET },
    { RSBS_XITEM2_SONG_BOLERO_MM, RSBS_MM_WARP_SONG_OWNED_BOLERO, "Bolero of Fire (MM)", "Bolero of Fire",
      RSBS_MM_WARP_SONG_TOOLTIP_BOLERO },
    { RSBS_XITEM2_SONG_SERENADE_MM, RSBS_MM_WARP_SONG_OWNED_SERENADE, "Serenade of Water (MM)", "Serenade of Water",
      RSBS_MM_WARP_SONG_TOOLTIP_SERENADE },
    { RSBS_XITEM2_SONG_REQUIEM_MM, RSBS_MM_WARP_SONG_OWNED_REQUIEM, "Requiem of Spirit (MM)", "Requiem of Spirit",
      RSBS_MM_WARP_SONG_TOOLTIP_REQUIEM },
    { RSBS_XITEM2_SONG_NOCTURNE_MM, RSBS_MM_WARP_SONG_OWNED_NOCTURNE, "Nocturne of Shadow (MM)", "Nocturne of Shadow",
      RSBS_MM_WARP_SONG_TOOLTIP_NOCTURNE },
    { RSBS_XITEM2_SONG_PRELUDE_MM, RSBS_MM_WARP_SONG_OWNED_PRELUDE, "Prelude of Light (MM)", "Prelude of Light",
      RSBS_MM_WARP_SONG_TOOLTIP_PRELUDE },
};

RSBS_CTX_STATIC_ASSERT(RSBS_MM_WARP_SONG_OWNED_MINUET == (RSBS_XITEM2_SONG_MINUET_MM >> 1) &&
                           RSBS_MM_WARP_SONG_OWNED_PRELUDE == (RSBS_XITEM2_SONG_PRELUDE_MM >> 1) &&
                           RSBS_MM_WARP_SONG_OWNED_ALL ==
                               ((RSBS_XITEM2_WARP_SONG_EXTENSIONS & ~(uint32_t)RSBS_XITEM2_SONG_SOARING_OOT) >> 1),
                       "each MM warp song's ownership bit is its extension rule bit shifted down by one (#1169 XW5)");

static const MMWarpSongInfo* InfoForRule(uint32_t ruleBit) {
    for (size_t i = 0; i < RSBS_MM_WARP_SONG_COUNT; i++) {
        if (kMMWarpSongs[i].ruleBit == ruleBit) {
            return &kMMWarpSongs[i];
        }
    }
    return NULL;
}

uint8_t Combo_MMWarpSongOwnedBit(uint32_t ruleBit) {
    const MMWarpSongInfo* info = InfoForRule(ruleBit);
    return info != NULL ? info->ownedBit : (uint8_t)0u;
}

uint32_t Combo_MMWarpSongRuleBit(uint8_t ownedBit) {
    for (size_t i = 0; i < RSBS_MM_WARP_SONG_COUNT; i++) {
        if (kMMWarpSongs[i].ownedBit == ownedBit) {
            return kMMWarpSongs[i].ruleBit;
        }
    }
    return 0u;
}

uint32_t Combo_MMWarpSongCopies(uint32_t rules2, uint32_t ruleBit) {
    if (InfoForRule(ruleBit) == NULL || (rules2 & ruleBit) == 0u) {
        return 0u; // not one of the six, or OoTMM's songMinuetMm .. off: MM never gains it
    }
    if ((rules2 & (ruleBit << RSBS_XITEM2_WARP_SONG_SHARED_SHIFT)) != 0u) {
        // OoTMM's sharedSongMinuet ..: the added copy and OoT's own are one shared
        // item. The family is lane XW10's; MM's separate copy is held back, never
        // placed.
        return 0u;
    }
    return 1u;
}

const char* Combo_MMWarpSongName(uint32_t ruleBit) {
    const MMWarpSongInfo* info = InfoForRule(ruleBit);
    return info != NULL ? info->name : NULL;
}

const char* Combo_MMWarpSongOoTName(uint32_t ruleBit) {
    const MMWarpSongInfo* info = InfoForRule(ruleBit);
    return info != NULL ? info->ootName : NULL;
}

const char* Combo_MMWarpSongTooltip(uint32_t ruleBit) {
    const MMWarpSongInfo* info = InfoForRule(ruleBit);
    return info != NULL ? info->tooltip : NULL;
}

uint8_t Combo_MMWarpSongsOwned(void) {
    return (uint8_t)(gComboCtx.mmWarpSongsOwned & RSBS_MM_WARP_SONG_OWNED_ALL);
}

bool Combo_MMWarpSongOwned(uint32_t ruleBit) {
    const uint8_t bit = Combo_MMWarpSongOwnedBit(ruleBit);
    return bit != 0u && (Combo_MMWarpSongsOwned() & bit) != 0u;
}

void Combo_MMWarpSongGive(uint32_t ruleBit) {
    const uint8_t bit = Combo_MMWarpSongOwnedBit(ruleBit);
    if (bit != 0u) {
        gComboCtx.mmWarpSongsOwned = (uint8_t)(Combo_MMWarpSongsOwned() | bit);
    }
}

void Combo_MMWarpSongsSetOwned(uint8_t owned) {
    gComboCtx.mmWarpSongsOwned = (uint8_t)(owned & RSBS_MM_WARP_SONG_OWNED_ALL);
}
