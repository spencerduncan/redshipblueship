#include "Spoiler.h"
#include "Rando/Rando.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "ShipUtils.h"

#ifdef RSBS_SINGLE_EXECUTABLE
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include "foreign_items.h" // src/common — placement table + pinned-pool reverse lookup (Lane C1, #392)
#include "Rando/Foreign.h" // Rando::Foreign::IsEligibleHost — the host rule the LOAD path must reapply (#488)
// The #533/#568 REFUSED machinery and its player-visible half. The identity
// gate below reports through exactly the surface #570's arrival refusal uses
// (#610) — a divergent spoiler and a divergent arrival are the same class of
// corruption and must not be surfaced two different ways.
#include "save.h"
#include "notification_bridge.h"
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
// ---------------------------------------------------------------------------
// The spoiler-LOAD identity gate (#610).
//
// A spoiler is an UNTRUSTED FILE. Rando::Spoiler::HandleFileDropped copies
// whatever was dragged onto the window into the spoiler folder and selects it,
// and OnFileCreate's LOAD branch then applies it. Everything that branch does
// to MM's OWN world is upstream behavior and is deliberately left alone.
//
// The "foreign" section is different in kind: it is not a description of MM's
// world, it is a COMBO COMMIT. Reconstructing it writes
// gComboCtx.foreignPlacements, and every durable cross-game record flows from
// there — Rando::Foreign::RecordForeignPickup -> Combo_RecordSharedItem ->
// redeemed by whatever paired OoT world arrives next (foreign_items.h's
// give-time flow, which carries no evidence of where a record came from). So
// committing a section out of a file that names a DIFFERENT world injects items
// into a world whose fill never placed them and whose logic graph never
// accounted for them.
//
// Under one-game semantics (#564) the pairing identity freezes at creation, so
// the question this gate answers is #601's: was this spoiler authored FOR the
// world this session is playing? Only an identity comparison answers it, and
// the terms compared are exactly the ones #570 freezes at creation — which is
// why the writer now stamps them into the spoiler (Spoiler/Generate.cpp's
// "rsbsPairing" block).
//
// Divergence REFUSES rather than absorbing: nothing is committed, nothing is
// re-derived, and the refusal surfaces through the #533/#568 machinery exactly
// as #570's arrival refusal does. The MM world itself still loads — the gate is
// on the combo commit, not on the spoiler feature.
// ---------------------------------------------------------------------------

/** Read one identity term. False when it is missing or not an unsigned number —
 *  an unreadable term is treated as absent, never as zero. */
static bool ReadIdentityTerm(const nlohmann::json& identity, const char* key, uint32_t& out) {
    if (!identity.contains(key) || !identity[key].is_number_unsigned()) {
        return false;
    }
    out = identity[key].get<uint32_t>();
    return true;
}

static std::string IdentityMismatchDetail(const char* label, uint32_t spoilerValue, uint32_t sessionValue) {
    char buf[160];
    snprintf(buf, sizeof(buf), "%s: spoiler names %08X, this session is playing %08X", label, (unsigned)spoilerValue,
             (unsigned)sessionValue);
    return buf;
}

/**
 * True when the spoiler's foreign section must NOT be committed. `outTerm`
 * receives the divergent term's NAME (the machine term, so the refusal points
 * at one thing rather than saying "something is different") and `outDetail` the
 * numbers behind it.
 */
static bool ForeignIdentityDiverges(const nlohmann::json& spoiler, std::string& outTerm, std::string& outDetail) {
    // (1) There must be a live pairing at all. These are the two halves of
    //     Combo_ForeignPairingActive(), split apart so the refusal can name
    //     WHICH one is missing. Both-false is the issue's exact scenario: MM's
    //     default/boot state, or the state after a vanilla OoT session, where
    //     there is no world these crossings could possibly belong to.
    if (!gComboCtx.sourceIsRando) {
        outTerm = "sourceIsRando";
        outDetail = "this session is not playing a generated cross-game world";
        return true;
    }
    if (gComboCtx.sharedRandoSettingsHash == 0) {
        outTerm = "sharedRandoSettingsHash";
        outDetail = "this session recorded no settings profile, so the worlds must not pair";
        return true;
    }

    // (2) The spoiler must NAME a world. A foreign section with no identity
    //     block was written by a build predating this stamp, or by hand: it
    //     cannot be SHOWN to describe this world, and "cannot be shown" is
    //     refused rather than assumed. A pre-stamp paired spoiler is therefore
    //     no longer loadable as a paired world — regenerate it; the alternative
    //     is keeping the hole open for every file that omits the block.
    if (!spoiler.contains("rsbsPairing") || !spoiler["rsbsPairing"].is_object()) {
        outTerm = "rsbsPairing";
        outDetail = "the spoiler carries no cross-game identity, so it cannot be shown to name this world";
        return true;
    }
    const nlohmann::json& identity = spoiler["rsbsPairing"];

    uint32_t spoilerSeed = 0;
    if (!ReadIdentityTerm(identity, "sharedRandoSeed", spoilerSeed)) {
        outTerm = "sharedRandoSeed";
        outDetail = "the spoiler's identity omits the shared master seed";
        return true;
    }
    if (spoilerSeed != gComboCtx.sharedRandoSeed) {
        outTerm = "sharedRandoSeed";
        outDetail = IdentityMismatchDetail("shared master seed", spoilerSeed, gComboCtx.sharedRandoSeed);
        return true;
    }

    uint32_t spoilerSettings = 0;
    if (!ReadIdentityTerm(identity, "sharedRandoSettingsHash", spoilerSettings)) {
        outTerm = "sharedRandoSettingsHash";
        outDetail = "the spoiler's identity omits the shared settings digest";
        return true;
    }
    if (spoilerSettings != gComboCtx.sharedRandoSettingsHash) {
        outTerm = "sharedRandoSettingsHash";
        outDetail =
            IdentityMismatchDetail("shared settings digest", spoilerSettings, gComboCtx.sharedRandoSettingsHash);
        return true;
    }

    // #570's own term. ZERO on either side means "that world froze no MM
    // profile" — a legacy pre-freeze pair, which #570's arrival gate treats as
    // legacy rather than as divergent — so it is not compared instead of being
    // declared a mismatch. The seed and settings terms above have already
    // matched by the time this runs.
    uint32_t spoilerProfile = 0;
    if (ReadIdentityTerm(identity, "mmProfileDigest", spoilerProfile) && spoilerProfile != 0 &&
        gComboCtx.mmProfileDigest != 0 && spoilerProfile != gComboCtx.mmProfileDigest) {
        outTerm = "mmProfileDigest";
        outDetail = IdentityMismatchDetail("frozen MM option profile", spoilerProfile, gComboCtx.mmProfileDigest);
        return true;
    }

    return false;
}

static void RefuseForeignReconstruction(const std::string& term, const std::string& detail) {
    const int slot = RsbsSave_GetActiveSlot();
    fprintf(stderr,
            "[MM] spoiler-load: REFUSED — this spoiler's cross-game section does not name the world this session is "
            "playing (divergent term: %s; %s). No foreign placement is committed, so no durable cross-game record "
            "can be authored from it; unified-save slot %d is latched against writes this session. The Majora's "
            "Mask world itself still loads.\n",
            term.c_str(), detail.c_str(), slot);
    fflush(stderr);
    // Latch WITHOUT quarantining, the same shape #570's arrival refusal uses:
    // the on-disk .redsave is healthy — it is this SESSION that is holding a
    // spoiler for somebody else's world — so the file is left untouched and only
    // this session's writes are stopped. RsbsSave_RefuseSlotIdentity treats a
    // -1 slot as "nothing durable to protect" and is a no-op there.
    RsbsSave_RefuseSlotIdentity(slot);

    // Player-visible, immediately, on the shared overlay. stderr is not a
    // surface, and a refusal nobody sees reads as "the cross-game items just
    // silently did nothing" — #564 V7's silent vanilla revert wearing a fix's
    // clothes.
    const std::string message = "This spoiler's cross-game items were placed for a different world (divergent term: " +
                                term + "). They will not appear here, and this session will not be saved to the pair.";
    ComboNotification refusalToast;
    memset(&refusalToast, 0, sizeof(refusalToast));
    refusalToast.prefix = "Cross-game pairing REFUSED:";
    refusalToast.prefixColor[0] = 0.9f;
    refusalToast.prefixColor[1] = 0.35f;
    refusalToast.prefixColor[2] = 0.3f;
    refusalToast.prefixColor[3] = 1.0f;
    refusalToast.message = message.c_str();
    refusalToast.messageColor[0] = 1.0f;
    refusalToast.messageColor[1] = 1.0f;
    refusalToast.messageColor[2] = 1.0f;
    refusalToast.messageColor[3] = 1.0f;
    refusalToast.remainingTime = 15.0f;
    // Muted for the same reason #570's refusal toast is: the overlay's ding is
    // OoT's Audio_PlaySoundGeneral, and this runs on MM's file-create seam (and
    // in the display-free locks) where OoT's audio session is not a given.
    refusalToast.mute = 1;
    OoT_Notification_Emit(&refusalToast);
}

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
    if (spoiler["foreign"].empty()) {
        // A paired world that placed nothing. There is no commit to gate, so
        // there is nothing to refuse either — refusing here would latch a slot
        // over a section that could not corrupt anything.
        return 0;
    }

    // (2b) #610 IDENTITY GATE. Runs BEFORE the shape validation below, because
    // a spoiler for another world is refused whether or not its section is
    // well-formed — and because refusing must not depend on how far the parse
    // happened to get.
    {
        std::string divergentTerm;
        std::string divergentDetail;
        if (ForeignIdentityDiverges(spoiler, divergentTerm, divergentDetail)) {
            RefuseForeignReconstruction(divergentTerm, divergentDetail);
            return -2;
        }
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
    int rejected = 0;
    for (const Pending& p : pending) {
        RandoSaveCheck& hostSaveCheck = RANDO_SAVE_CHECKS[(RandoCheckId)p.checkId];

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
        //
        // Applied BEFORE the eligibility gate below, not after, because the
        // gate reads exactly these two fields: un-normalized, every host would
        // be rejected for holding RI_UNKNOWN, which says nothing about the
        // host's give path.
        const bool priorShuffled = hostSaveCheck.shuffled;
        hostSaveCheck.randoItemId = RI_JUNK;
        hostSaveCheck.shuffled = true;

        // #488, step 6: the LOAD path must apply the same host-eligibility rule
        // generation does. Combo_SetForeignPlacement enforces tagged-only /
        // no-duplicate / bounded and nothing about MM check semantics — it is a
        // deliberately game-agnostic layer — so without this gate a spoiler
        // written by a pre-#488 build reconstructs placements onto hosts whose
        // `.eligible` bit is never armed, and the crossing strands exactly as
        // it would have before the tightening. Reject loudly rather than
        // throw: the surrounding ApplyToSaveContext load is otherwise sound,
        // and aborting it would take a playable MM world down with the
        // unreachable crossings. Runs here rather than after the loop because
        // ApplyToSaveContext has already populated RANDO_SAVE_CHECKS by the
        // time ReconstructForeignPlacements is called.
        //
        // What this gate can and cannot see on the load path, stated precisely
        // so nobody later mistakes it for the full generate-path rule:
        //
        //  - The item-class half asserts nothing here. It is satisfied by the
        //    RI_JUNK this loop just wrote, and that is inherent — the spoiler
        //    does not preserve the host's underlying MM item, so the
        //    pre-normalization value is RI_UNKNOWN for every legitimate host.
        //  - The `.skipped` half also asserts nothing here. ApplyToSaveContext
        //    writes only randoItemId/shuffled/price, and the only writers of
        //    `.skipped` anywhere are Logic/GeneratePools.cpp (generate path)
        //    and CheckTracker.cpp (a user toggle on a live save), so on a load
        //    it is uniformly false.
        //  - The half that DOES bite is the check-class allowlist — which is
        //    the one that matters, because it is the half that decides whether
        //    the host can ever be armed.
        if (!Rando::Foreign::IsEligibleHost((RandoCheckId)p.checkId)) {
            // Restore `shuffled` only. randoItemId deliberately KEEPS the
            // RI_JUNK written above: this branch is precisely the
            // "placement is absent" case the normalization comment describes,
            // and putting the unresolvable RI_UNKNOWN back would leave a
            // shuffled check holding a non-item — which arms `.eligible` all
            // the same (OnFlagSet gates on `.shuffled` alone) and then walks
            // the give path with a sentinel. Degrading to junk is the whole
            // point of the normalization; the reject path needs it most.
            hostSaveCheck.shuffled = priorShuffled;
            rejected++;
            const auto staticIt = Rando::StaticData::Checks.find((RandoCheckId)p.checkId);
            fprintf(stderr,
                    "[MM] spoiler-load: REJECTING foreign placement on MM check %s — not an eligible host under the "
                    "current rule (pre-#488 spoiler?); this crossing is NOT reachable in-game\n",
                    staticIt != Rando::StaticData::Checks.end() ? staticIt->second.name : "<unknown>");
            continue;
        }

        if (Combo_SetForeignPlacement(p.checkId, p.item) >= 0) {
            placed++;
        } else {
            // Same reasoning as the reject branch: an insert refusal is also
            // an absent placement, so the host keeps RI_JUNK rather than
            // reverting to the unresolvable RI_UNKNOWN.
            hostSaveCheck.shuffled = priorShuffled;
        }
    }
    fprintf(stderr, "[MM] spoiler-load: reconstructed %d foreign placement(s) from the spoiler's foreign section\n",
            placed);
    if (rejected > 0) {
        fprintf(stderr,
                "[MM] spoiler-load: %d foreign placement(s) rejected as ineligible hosts — this world was generated "
                "by a build predating the #488 host rule and cannot deliver those items; regenerate it\n",
                rejected);
    }
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
