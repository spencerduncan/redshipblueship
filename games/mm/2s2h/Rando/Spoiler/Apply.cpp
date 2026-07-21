#include "Spoiler.h"
#include "Rando/Rando.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "ShipUtils.h"

#ifdef RSBS_SINGLE_EXECUTABLE
#include <cstdio>
#include <stdexcept>
#include <vector>
#include "foreign_items.h" // src/common — placement table + pinned-pool reverse lookup (Lane C1, #392)
#endif

extern "C" {
#include "overlays/actors/ovl_En_Sth/z_en_sth.h"
}

namespace Rando {

namespace Spoiler {

#ifdef RSBS_SINGLE_EXECUTABLE
// Lane C1 follow-up (#392): rebuild gComboCtx.foreignPlacements from a loaded
// spoiler's "foreign" section — the spoiler-LOAD counterpart of generation's
// Rando::Foreign::PlaceForeignItems (games/mm/2s2h/Rando/Foreign.cpp). Without
// this a paired MM world entered via the LOAD path never rebuilds the placement
// table, and its foreign checks silently degrade to the junk-class MM item they
// physically hold (the gap the C1 landing on #392 noted).
//
// Overwrite semantics (grounded in save.cpp's .redsave load, which validates
// the whole record and only then commits the ComboContext whole-struct):
//
//  (1) REDEMPTION IS STRUCTURALLY SAFE. This function writes ONLY
//      foreignPlacements; it never reads or writes sharedItemsTagged, where
//      RSBS_SHARED_ITEM_REDEEMED lives. A spoiler load therefore can never lose
//      or re-award a redeemed crossing — the same guarantee shared_items.h
//      already relies on (redeemed entries are never cleared).
//
//  (2) REFUSE-OR-PRESERVE, never silent overwrite. If foreignPlacements is
//      already populated when this runs (the "load over a live paired session"
//      hazard — the caller did not clear first), the existing table is
//      PRESERVED and reconstruction refuses, because the live sharedItemsTagged
//      crossings reference the world that table describes. The normal new-file
//      path clears placements before applying (OnFileCreate), so it always
//      reconstructs from an empty table; the guard only bites on an overwrite
//      of live state.
//
//  (3) VALIDATE-THEN-COMMIT, atomic. The whole "foreign" section is validated
//      into a staging buffer before any write, with the same invariants
//      generation enforces (OoT-origin only; a named pinned-pool item, so the
//      SharedItem is the tagged pool entry and never a raw RG_*; a known MM
//      check; no duplicate host check; bounded by RSBS_FOREIGN_PLACEMENT_CAP).
//      A malformed section throws before touching the table (consistent with
//      ApplyToSaveContext's existing throw-on-malformed-check and LoadFromFile's
//      throw-on-bad-type-tag). An ABSENT section is not an error: a solo or
//      pre-C1 world legitimately hosts no foreign items.
int ReconstructForeignPlacements(const nlohmann::json& spoiler) {
    // (2) Never silently overwrite a live session's placement table.
    if (Combo_CountForeignPlacements() > 0) {
        fprintf(stderr,
                "[MM] spoiler-load: foreignPlacements already populated (%d); preserving live session, "
                "not overwriting from the spoiler\n",
                Combo_CountForeignPlacements());
        return -1;
    }

    // Absent section: not paired / pre-C1 — leave the (cleared) table empty.
    if (!spoiler.contains("foreign")) {
        return 0;
    }
    if (!spoiler["foreign"].is_object()) {
        throw std::runtime_error("Spoiler 'foreign' section is not an object");
    }

    // (3) Stage-and-validate every entry before committing anything.
    struct Pending {
        uint16_t checkId;
        SharedItem item;
    };
    std::vector<Pending> pending;
    for (auto& [checkName, entry] : spoiler["foreign"].items()) {
        if (!entry.is_object() || !entry.contains("originGame") || !entry.contains("item")) {
            throw std::runtime_error("Spoiler foreign entry '" + checkName + "' is malformed");
        }
        const std::string originGame = entry["originGame"].get<std::string>();
        if (originGame != "OOT") {
            // The MVP is one-directional (OoT items into MM checks); anything
            // else is a spoiler from an incompatible/newer build.
            throw std::runtime_error("Spoiler foreign entry '" + checkName + "' has unsupported originGame '" +
                                     originGame + "'");
        }
        const std::string itemName = entry["item"].get<std::string>();
        SharedItem item;
        if (!Combo_GetForeignItemByName(itemName.c_str(), &item)) {
            throw std::runtime_error("Spoiler foreign entry '" + checkName + "' names unknown foreign item '" +
                                     itemName + "'");
        }
        if (item.originGame != (uint8_t)GAME_OOT) {
            throw std::runtime_error("Spoiler foreign entry '" + checkName + "' resolved to a non-OoT-tagged item");
        }
        const RandoCheckId checkId = Rando::StaticData::GetCheckIdFromName(checkName.c_str());
        if (checkId == RC_UNKNOWN) {
            throw std::runtime_error("Spoiler foreign section names unknown MM check '" + checkName + "'");
        }
        for (const Pending& p : pending) {
            if (p.checkId == (uint16_t)checkId) {
                throw std::runtime_error("Spoiler foreign section lists MM check '" + checkName + "' twice");
            }
        }
        if (pending.size() >= (size_t)RSBS_FOREIGN_PLACEMENT_CAP) {
            throw std::runtime_error("Spoiler foreign section exceeds the placement table capacity");
        }
        pending.push_back({ (uint16_t)checkId, item });
    }

    // Commit: the table was empty (guard above); rebuild it through the SAME
    // Combo_SetForeignPlacement generation uses, so the tagged-only /
    // no-duplicate / bounded invariants are enforced by one code path.
    Combo_ClearForeignPlacements();
    int placed = 0;
    for (const Pending& p : pending) {
        if (Combo_SetForeignPlacement(p.checkId, p.item) >= 0) {
            placed++;
            // ADR 0002 host invariant, restored on load: the MM save table must
            // keep a LEGAL junk-class MM item at a foreign host check (never a
            // raw RG_*). The spoiler's human-readable "checks" map stored this
            // check as "<item> (Ocarina of Time)", which GetItemIdFromName
            // cannot resolve, so ApplyToSaveContext just left RI_UNKNOWN there;
            // the underlying junk item is not recoverable from the spoiler, so
            // normalize to the canonical junk sentinel. Inert while the
            // placement stands (the give path reads foreignPlacements, not this
            // item), but it keeps the table legal and makes the check degrade to
            // junk — not RI_UNKNOWN — if the placement is ever absent.
            RANDO_SAVE_CHECKS[(RandoCheckId)p.checkId].randoItemId = RI_JUNK;
            RANDO_SAVE_CHECKS[(RandoCheckId)p.checkId].shuffled = true;
        }
    }
    fprintf(stderr, "[MM] spoiler-load: reconstructed %d foreign placement(s) from the spoiler's foreign section\n",
            placed);
    return placed;
}
#endif

void ApplyToSaveContext(nlohmann::json spoiler) {
    gSaveContext.save.shipSaveInfo.rando.finalSeed = spoiler["finalSeed"].get<uint32_t>();

    for (auto& [randoOptionId, randoStaticOption] : Rando::StaticData::Options) {
        RANDO_SAVE_OPTIONS[randoOptionId] = spoiler["options"][randoStaticOption.name].get<uint32_t>();
    }

    if (!RANDO_SAVE_OPTIONS[RO_SHUFFLE_GOLD_SKULLTULAS]) {
        RANDO_SAVE_OPTIONS[RO_MINIMUM_SKULLTULA_TOKENS] = SPIDER_HOUSE_TOKENS_REQUIRED;
    }

    auto startingItems = Rando::GetStartingItemsFromSpoiler(spoiler);
    Rando::SetStartingItemsInSave(gSaveContext.save.shipSaveInfo.rando, startingItems);

    for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
        if (randoStaticCheck.randoCheckId == RC_UNKNOWN) {
            continue;
        }

        if (!spoiler["checks"].contains(randoStaticCheck.name)) {
            RANDO_SAVE_CHECKS[randoCheckId].randoItemId = randoStaticCheck.randoItemId;
            RANDO_SAVE_CHECKS[randoCheckId].shuffled = false;
            continue;
        }

        // Check if it's an object or a string
        if (spoiler["checks"][randoStaticCheck.name].is_object()) {
            std::string itemName = spoiler["checks"][randoStaticCheck.name]["randoItemId"].get<std::string>();
            RandoItemId randoItemId = Rando::StaticData::GetItemIdFromName(itemName.c_str());

            RANDO_SAVE_CHECKS[randoCheckId].randoItemId = randoItemId;
            RANDO_SAVE_CHECKS[randoCheckId].shuffled = true;

            // If it has a price, set it
            if (spoiler["checks"][randoStaticCheck.name].contains("price")) {
                RANDO_SAVE_CHECKS[randoCheckId].price =
                    spoiler["checks"][randoStaticCheck.name]["price"].get<uint16_t>();
            }
        } else {
            std::string itemName = spoiler["checks"][randoStaticCheck.name].get<std::string>();
            RandoItemId randoItemId = Rando::StaticData::GetItemIdFromName(itemName.c_str());

            RANDO_SAVE_CHECKS[randoCheckId].randoItemId = randoItemId;
            RANDO_SAVE_CHECKS[randoCheckId].shuffled = true;
        }
    }

#ifdef RSBS_SINGLE_EXECUTABLE
    // Lane C1 follow-up (#392): a paired MM world entered via the spoiler-LOAD
    // path must rebuild its cross-game placements from the spoiler, or its
    // foreign checks degrade to the junk they physically hold. This is the LOAD
    // counterpart of generation's Rando::Foreign::PlaceForeignItems. Runs after
    // the checks are applied; never touches sharedItemsTagged (redemption-safe).
    ReconstructForeignPlacements(spoiler);
#endif
}

} // namespace Spoiler

} // namespace Rando
