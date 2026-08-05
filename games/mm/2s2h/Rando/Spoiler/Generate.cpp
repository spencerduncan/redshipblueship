#include "Spoiler.h"
#include "Rando/Rando.h"
#ifdef RSBS_SINGLE_EXECUTABLE
#include "Rando/Foreign.h" // Lane C1 (#392): foreign-check lookup for the spoiler
// src/common — gComboCtx, for the pairing identity the spoiler now stamps
// alongside its cross-game section (#610). Included OUTSIDE any extern "C"
// block: context.h manages its own linkage (see its header comment).
#include "context.h"
#endif

namespace Rando {

namespace Spoiler {

nlohmann::json GenerateFromSaveContext() {
    nlohmann::json spoiler;
    spoiler["type"] = "2S2H_RANDO_SPOILER";
    spoiler["commitHash"] = gSaveContext.save.shipSaveInfo.commitHash;
    spoiler["finalSeed"] = gSaveContext.save.shipSaveInfo.rando.finalSeed;

    spoiler["options"] = nlohmann::json::object();
    for (auto& [randoOptionId, randoStaticOption] : Rando::StaticData::Options) {
        spoiler["options"][randoStaticOption.name] = RANDO_SAVE_OPTIONS[randoOptionId];
    }

    auto startingItems = Rando::GetStartingItemsFromSave(gSaveContext.save.shipSaveInfo.rando);
    Rando::SetStartingItemsInSpoiler(spoiler, startingItems);

    spoiler["checks"] = nlohmann::json::object();
    for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
        if (randoStaticCheck.randoCheckId == RC_UNKNOWN) {
            continue;
        }

        if (!RANDO_SAVE_CHECKS[randoCheckId].shuffled) {
            continue;
        }

#ifdef RSBS_SINGLE_EXECUTABLE
        // Lane C1 (#392): a check hosting a foreign item reads as that item,
        // not as the RI_JUNK physically stored in the MM table — the human-
        // readable checks list must describe what the check actually yields.
        // The machine-readable record of the crossing is the "foreign"
        // section below.
        if (const char* foreignName = Rando::Foreign::ForeignNameForCheck(randoCheckId)) {
            spoiler["checks"][randoStaticCheck.name] = std::string(foreignName) + " (Ocarina of Time)";
            continue;
        }
#endif

        if (randoStaticCheck.randoCheckType == RCTYPE_SHOP || randoStaticCheck.randoCheckType == RCTYPE_TINGLE_SHOP) {
            spoiler["checks"][randoStaticCheck.name] = nlohmann::json::object();
            spoiler["checks"][randoStaticCheck.name]["randoItemId"] =
                Rando::StaticData::Items[RANDO_SAVE_CHECKS[randoCheckId].randoItemId].spoilerName;
            spoiler["checks"][randoStaticCheck.name]["price"] = RANDO_SAVE_CHECKS[randoCheckId].price;
        } else {
            spoiler["checks"][randoStaticCheck.name] =
                Rando::StaticData::Items[RANDO_SAVE_CHECKS[randoCheckId].randoItemId].spoilerName;
        }
    }

#ifdef RSBS_SINGLE_EXECUTABLE
    // Lane C1 (#392): every cross-game placement, described — check name,
    // item name, origin game — the MVP contract's "a spoiler log describes
    // it". Only present (possibly empty) when this world was generated as the
    // MM half of a paired world.
    if (Rando::Foreign::PairingActive()) {
        // #610: the "foreign" section below is a COMBO COMMIT, not a
        // description — reloading it writes gComboCtx.foreignPlacements, from
        // which every durable cross-game record flows. A spoiler is an
        // untrusted file on disk (HandleFileDropped copies in whatever was
        // dragged onto the window), so it has to NAME the world it was authored
        // for or the LOAD path has nothing to compare against and must refuse.
        //
        // These are exactly the terms #570 freezes at creation, in exactly the
        // form the arrival gate compares. They are written HERE rather than
        // unconditionally because an unpaired world has no cross-game identity
        // to name, and stamping a zeroed one would make "no pairing" look like
        // a pairing that happens to be all zeros.
        spoiler["rsbsPairing"] = {
            { "sharedRandoSeed", gComboCtx.sharedRandoSeed },
            { "sharedRandoSettingsHash", gComboCtx.sharedRandoSettingsHash },
            { "mmProfileDigest", gComboCtx.mmProfileDigest },
        };

        spoiler["foreign"] = nlohmann::json::object();
        for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
            if (randoStaticCheck.randoCheckId == RC_UNKNOWN || !Rando::Foreign::IsForeignCheck(randoCheckId)) {
                continue;
            }
            const char* foreignName = Rando::Foreign::ForeignNameForCheck(randoCheckId);
            spoiler["foreign"][randoStaticCheck.name] = {
                { "originGame", "OOT" },
                { "item", foreignName != nullptr ? foreignName : "(unknown foreign item)" },
            };
        }

        // ADR 0010 increment 1.3 under-supply rule: when the reachability gate
        // left fewer eligible hosts than pool items, FEWER were placed (cap ≠
        // promise) and the spoiler is the durable, loud record of it — a
        // player reading "4 items promised, 2 crossed" learns it here, not
        // from a bug report. Absent whenever the pool placed in full.
        const Rando::Foreign::PlacementStats& stats = Rando::Foreign::LastPlacementStats();
        if (stats.placed < stats.requested) {
            spoiler["foreignShortfall"] = {
                { "requested", stats.requested },
                { "placed", stats.placed },
                { "eligibleHosts", stats.eligibleHosts },
                { "reachableEligibleHosts", stats.reachableEligibleHosts },
            };
        }
    }
#endif

    return spoiler;
}

} // namespace Spoiler

} // namespace Rando
