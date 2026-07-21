#ifndef RANDO_SPOILER_H
#define RANDO_SPOILER_H

#include <vector>
#include <string>
#include "nlohmann/json.hpp"

namespace Rando {

namespace Spoiler {

extern std::vector<std::string> spoilerOptions;
void RefreshOptions();
nlohmann::json GenerateFromSaveContext();
void SaveToFile(const std::string& fileName, nlohmann::json spoiler);
nlohmann::json LoadFromFile(const std::string& filePath);
void ApplyToSaveContext(nlohmann::json spoiler);
bool HandleFileDropped(char* path);

#ifdef RSBS_SINGLE_EXECUTABLE
// Lane C1 follow-up (#392): the spoiler-LOAD counterpart of generation's
// Rando::Foreign::PlaceForeignItems. Rebuilds gComboCtx.foreignPlacements from
// a loaded spoiler's "foreign" section so a paired MM world entered via LOAD
// (not fresh generation) yields its foreign items instead of degrading to the
// junk-class MM host item. Called by ApplyToSaveContext; exposed for the
// MMRandoGen CTest lock.
//
// Returns the number of placements reconstructed (>= 0), 0 for an absent
// "foreign" section, or -1 when it PRESERVES an already-populated (live-
// session) table rather than overwriting it. Throws std::runtime_error on a
// malformed section (validate-then-commit; the table is left untouched). Never
// reads or writes gComboCtx.sharedItemsTagged, so redeemed crossings survive.
// See Apply.cpp for the full overwrite semantics.
int ReconstructForeignPlacements(const nlohmann::json& spoiler);
#endif

} // namespace Spoiler

} // namespace Rando

#endif
