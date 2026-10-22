/**
 * @file combo_mm_locations_view.c
 * @brief The seam for Randomizer > MM Locations (#1101 lane R3), as today: no
 *        table and no writer, so nothing authors gRando.ExcludedChecks.
 *
 * See combo_mm_locations_view.h for the contract the fix implements.
 */

#include "combo_mm_locations_view.h"
#include "combo_mm_options_page.h" // COMBO_RANDO_PAGES_NEXT_WORLD_NOTE

void Combo_RegisterMMLocationTable(const ComboMMLocationDesc* table, int count, bool (*vanillaLogic)(void)) {
    (void)table;
    (void)count;
    (void)vanillaLogic;
}

int Combo_MMLocationCount(void) {
    return 0;
}

const ComboMMLocationDesc* Combo_MMLocationAt(int index) {
    (void)index;
    return NULL;
}

const ComboMMLocationDesc* Combo_MMLocationById(uint16_t id) {
    (void)id;
    return NULL;
}

int Combo_MMLocationsExcluded(uint16_t* out, int cap) {
    (void)out;
    (void)cap;
    return 0;
}

bool Combo_MMLocationIsExcluded(uint16_t id) {
    (void)id;
    return false;
}

bool Combo_MMLocationsBlockedByLogic(void) {
    return false;
}

bool Combo_MMLocationsSetExcluded(const uint16_t* ids, int count, bool excluded) {
    (void)ids;
    (void)count;
    (void)excluded;
    return false;
}

bool Combo_MMLocationSetExcluded(uint16_t id, bool excluded) {
    return Combo_MMLocationsSetExcluded(&id, 1, excluded);
}

bool Combo_MMLocationsClearExcluded(void) {
    return false;
}

const char* Combo_MMLocationsPage_StatusNote(void) {
    return COMBO_RANDO_PAGES_NEXT_WORLD_NOTE;
}
