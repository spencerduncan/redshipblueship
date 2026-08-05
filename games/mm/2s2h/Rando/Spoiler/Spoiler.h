#ifndef RANDO_SPOILER_H
#define RANDO_SPOILER_H

#include <vector>
#include <string>
#include "nlohmann/json.hpp"

namespace Rando {

namespace Spoiler {

extern std::vector<std::string> spoilerOptions;

/**
 * The one directory MM spoilers are read from and written to (#439).
 *
 * Single source of truth for every spoiler path in this module — before this
 * existed, four call sites each re-derived it and the write path assumed a
 * directory nothing guaranteed to exist.
 *
 * In single-exe builds it resolves against the SHARED Ship::Context's app
 * directory (OoT creates that context, so passing MM's own "2ship" short name
 * pointed at a second app directory the combo never otherwise uses — the
 * operator's portable install had no 2ship directory at all). The folder name
 * is deliberately distinct from OoT's "Randomizer" because on Windows those
 * two names collide case-insensitively and MM's directory_iterator would list
 * OoT's spoilers as MM spoiler options.
 *
 * The directory is created (recursively) if missing; the return value is
 * always a usable directory or the call throws.
 */
std::string SpoilerDirectory();
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
// Returns the number of placements reconstructed (>= 0), 0 for an absent or
// empty "foreign" section, -1 when it PRESERVES an already-populated (live-
// session) table rather than overwriting it, or -2 when the spoiler's cross-game
// IDENTITY does not name the world this session is playing (#610) — refused
// through the #533 surface, nothing committed, MM's own world still applied.
// Throws std::runtime_error on a malformed section (validate-then-commit; the
// table is left untouched). Never reads or writes gComboCtx.sharedItemsTagged,
// so redeemed crossings survive. See Apply.cpp for the full semantics.
int ReconstructForeignPlacements(const nlohmann::json& spoiler);
#endif

} // namespace Spoiler

} // namespace Rando

#endif
