#include "MiscBehavior.h"
#include "Rando/Rando.h"
#include "Rando/Spoiler/Spoiler.h"
#include "Rando/Logic/Logic.h"
#include "2s2h/ShipUtils.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "ClockShuffle.h"
#include <spdlog/spdlog.h>
#include <cstdio>
#ifdef RSBS_SINGLE_EXECUTABLE
#include "Rando/Foreign.h" // Lane C1 (#392): paired-world seed derivation + foreign placement
#include "foreign_items.h" // src/common — Combo_ClearForeignPlacements
#endif

extern "C" {
#include "functions.h"
#include "variables.h"
#include "ShipUtils.h"
#include "overlays/actors/ovl_En_Sth/z_en_sth.h"
}

// Very primitive randomizer implementation, when a save is created, if rando is enabled
// we set the save type to rando and shuffle all checks and persist the results to the save
void Rando::MiscBehavior::OnFileCreate(s16 fileNum) {
#ifdef RSBS_SINGLE_EXECUTABLE
    // Lane C1 (#392): a live paired OoT rando world (Lane B's carrier stamped
    // in gComboCtx) makes a new MM file the MM HALF of that world — the MVP's
    // "one seed produces a paired OoT+MM world". This is deliberately not
    // gated on gRando.Enabled: MM's rando menu is link-elided in the single
    // exe (2ship_rando_ui), so there is no in-game surface that could set the
    // CVar; the paired OoT generation IS the user's opt-in.
    const bool rsbsPaired = Rando::Foreign::PairingActive();
    // Unconditional diagnostic: the paired-vs-solo decision and its inputs are
    // the first thing to check when a paired world fails to pair (greppable in
    // CI logs and operator sessions alike).
    fprintf(stderr, "[MM] OnFileCreate: paired=%d (sourceIsRando=%d settingsHash=%08X masterSeed=%08X)\n",
            rsbsPaired ? 1 : 0, gComboCtx.sourceIsRando ? 1 : 0, gComboCtx.sharedRandoSettingsHash,
            gComboCtx.sharedRandoSeed);
    if (CVarGetInteger("gRando.Enabled", 0) || rsbsPaired) {
#else
    if (CVarGetInteger("gRando.Enabled", 0)) {
#endif
        gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
        // Zero out the rando struct
        memset(&gSaveContext.save.shipSaveInfo.rando, 0, sizeof(gSaveContext.save.shipSaveInfo.rando));
        // Copy whatever the current dungeon keys are, they're initialized as -1 in the save, not 0
        memcpy(&gSaveContext.save.shipSaveInfo.rando.foundDungeonKeys,
               &gSaveContext.save.saveInfo.inventory.dungeonKeys,
               sizeof(gSaveContext.save.saveInfo.inventory.dungeonKeys));

        // Skip the first cycle, in Rando we start as Human at south clock town.
        gSaveContext.save.entrance = ENTRANCE(SOUTH_CLOCK_TOWN, 0);
        gSaveContext.save.cutsceneIndex = 0;
        gSaveContext.save.hasTatl = true;
        gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
        gSaveContext.save.saveInfo.playerData.threeDayResetCount = 1;
        gSaveContext.save.isFirstCycle = true;
        SET_WEEKEVENTREG(WEEKEVENTREG_59_04);                                                  // Tatl
        SET_WEEKEVENTREG(WEEKEVENTREG_31_04);                                                  // Tatl
        gSaveContext.save.saveInfo.permanentSceneFlags[SCENE_INSIDETOWER].switch0 |= (1 << 0); // Happy Mask Salesman

        // Remove Sword & Shield
        SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
        BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = ITEM_NONE;
        SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_NONE);

#ifdef RSBS_SINGLE_EXECUTABLE
        // Lane C1 (#392): a new MM rando file defines a new world — placements
        // recorded for any previous world must not leak into it (they are
        // re-placed below iff this world pairs with a live OoT rando world).
        Combo_ClearForeignPlacements();
#endif

        try {
            // SpoilerFileIndex == 0 means we're generating a new one
#ifdef RSBS_SINGLE_EXECUTABLE
            // A paired world is always GENERATED: its identity comes from the
            // shared master seed, never from a previously saved spoiler.
            // Without this, a stale gRando.SpoilerFileIndex (RefreshOptions
            // repoints it at the last written spoiler after every successful
            // generation) would silently LOAD the old world instead of
            // deriving the paired one — caught by MMRandoGen's paired phase.
            if (CVarGetInteger("gRando.SpoilerFileIndex", 0) == 0 || rsbsPaired) {
#else
            if (CVarGetInteger("gRando.SpoilerFileIndex", 0) == 0) {
#endif
                bool hadInputSeed = true;
                std::string inputSeed = Ship_RemoveSpecialCharacters(CVarGetString("gRando.InputSeed", ""));
                if (inputSeed.empty()) {
                    inputSeed = std::to_string(Ship_Random(0, 1000000));
                    hadInputSeed = false;
                }

#ifdef RSBS_SINGLE_EXECUTABLE
                if (rsbsPaired) {
                    // One seed -> paired world (Lane B's carrier contract):
                    // when a live OoT rando world exists, THIS file is its MM
                    // half, so the MM seed derives from the shared master
                    // seed rather than user input. The derived input-seed
                    // string also names the spoiler file deterministically.
                    inputSeed = Rando::Foreign::PairedInputSeedString();
                    hadInputSeed = true;
                    SPDLOG_INFO("Paired-world generation: MM seed derived from shared master seed ({})", inputSeed);
                }
#endif

                uint32_t finalSeed = Ship_Hash(inputSeed);
                Ship_Random_Seed(finalSeed);

                // Persist options to the save
                gSaveContext.save.shipSaveInfo.rando.finalSeed = finalSeed;
#ifdef RSBS_SINGLE_EXECUTABLE
                // The profile — the option copy, the skulltula correction, the
                // paired logic default and the profile digest — is resolved by
                // ONE callable (#499 step 2). It used to be three inline blocks
                // here, so the only way to observe it was to run a whole fill;
                // the display-free MMPairedProfile lock drives this function
                // directly. See Rando/Foreign.h for the ordering contract.
                Rando::Foreign::ResolvePairedProfile(rsbsPaired);
#else
                for (auto& [randoOptionId, randoStaticOption] : Rando::StaticData::Options) {
                    RANDO_SAVE_OPTIONS[randoOptionId] =
                        (uint32_t)CVarGetInteger(randoStaticOption.cvar, randoStaticOption.defaultValue);
                }

                // If Skulltula tokens are not shuffled, use the vanilla requirement
                if (!RANDO_SAVE_OPTIONS[RO_SHUFFLE_GOLD_SKULLTULAS]) {
                    RANDO_SAVE_OPTIONS[RO_MINIMUM_SKULLTULA_TOKENS] = SPIDER_HOUSE_TOKENS_REQUIRED;
                }
#endif

                // ------------------------------------------------------------
                // ONE generation attempt, from starting items through the
                // foreign-placement pass. Factored into a callable because the
                // paired path below runs it under the deterministic attempt
                // ladder (ADR 0010 increment 1.2) while the solo path keeps
                // its single-shot upstream behavior. Everything inside is a
                // pure function of (the save state at entry, the live RNG
                // stream, the resolved options) — the ladder restores the
                // first two per attempt, which is what makes a retry a
                // genuinely fresh derivation rather than a mutation of a
                // half-failed one.
                // ------------------------------------------------------------
                auto runGenerationOnce = [&]() {
                    // Persist StartingItems to the save
                    auto startingItems = Rando::GetStartingItemsFromConfig();
                    Rando::SetStartingItemsInSave(gSaveContext.save.shipSaveInfo.rando, startingItems);

                    std::vector<RandoCheckId> checkPool;
                    std::vector<RandoItemId> itemPool;
                    Rando::Logic::GeneratePools(gSaveContext.save.shipSaveInfo.rando, checkPool, itemPool);

                    if (checkPool.empty()) {
                        throw std::runtime_error("No checks in logic");
                    }
                    if (itemPool.empty()) {
                        throw std::runtime_error("No items in logic");
                    }

                    // Balance pools
                    int heartPiecesRemoved = 0;
                    while (checkPool.size() != itemPool.size()) {
                        if (checkPool.size() > itemPool.size()) {
                            itemPool.push_back(RI_JUNK);
                        } else {
                            // First, remove junk items if we have any
                            bool removedJunk = false;
                            for (int i = 0; i < itemPool.size(); i++) {
                                if (Rando::StaticData::Items[itemPool[i]].randoItemType == RITYPE_JUNK) {
                                    itemPool.erase(itemPool.begin() + i);
                                    removedJunk = true;
                                    break;
                                }
                            }
                            if (removedJunk) {
                                continue;
                            }

                            // Next replace 4 heart pieces with a heart container
                            bool removedHeartPieces = false;
                            for (int i = 0; i < itemPool.size(); i++) {
                                if (Rando::StaticData::Items[itemPool[i]].randoItemId == RI_HEART_PIECE) {
                                    if (heartPiecesRemoved == 0) {
                                        itemPool[i] = RI_HEART_CONTAINER;
                                    } else {
                                        itemPool.erase(itemPool.begin() + i);
                                    }

                                    removedHeartPieces = true;
                                    heartPiecesRemoved++;
                                    if (heartPiecesRemoved == 4) {
                                        heartPiecesRemoved = 0;
                                    }
                                    break;
                                }
                            }

                            if (removedHeartPieces) {
                                continue;
                            }

                            SPDLOG_ERROR("Could not match item pool size to check pool size {}/{}", itemPool.size(),
                                         checkPool.size());
                            throw std::runtime_error("Could not match item pool size to check pool size");
                        }
                    }

                    // Grant the starting stuff
                    Rando::GrantStartingItems();

                    if (RANDO_SAVE_OPTIONS[RO_LOGIC] == RO_LOGIC_VANILLA) {
                        GiveItem(RI_SWORD_KOKIRI);
                        GiveItem(RI_SHIELD_HERO);

                        for (auto& [randoCheckId, randoStaticCheck] : Rando::StaticData::Checks) {
                            if (randoStaticCheck.randoCheckId != RC_UNKNOWN) {
                                RANDO_SAVE_CHECKS[randoCheckId].randoItemId = randoStaticCheck.randoItemId;
                                RANDO_SAVE_CHECKS[randoCheckId].shuffled = true;
                            }
                        }
                    } else if (RANDO_SAVE_OPTIONS[RO_LOGIC] == RO_LOGIC_NO_LOGIC) {
                        Rando::Logic::ApplyNoLogicToSaveContext(checkPool, itemPool);
                    } else if (RANDO_SAVE_OPTIONS[RO_LOGIC] == RO_LOGIC_NEARLY_NO_LOGIC) {
                        Rando::Logic::ApplyNearlyNoLogicToSaveContext(checkPool, itemPool);
                    } else if (RANDO_SAVE_OPTIONS[RO_LOGIC] == RO_LOGIC_GLITCHLESS) {
                        Rando::Logic::ApplyGlitchlessLogicToSaveContext(checkPool, itemPool);
                    } else {
                        throw std::runtime_error("Logic option not implemented: " +
                                                 std::to_string(RANDO_SAVE_OPTIONS[RO_LOGIC]));
                    }

#ifdef RSBS_SINGLE_EXECUTABLE
                    if (rsbsPaired) {
                        // Lane C1 (#392): swap deterministically-chosen junk
                        // placements for the pinned OoT foreign items, recorded in
                        // gComboCtx.foreignPlacements (the MM table keeps its
                        // junk-class MM item — ADR 0002). Runs before the spoiler write so the
                        // spoiler's foreign section describes this world.
                        //
                        // #488: a SHORT placement is a fatal generation failure,
                        // not a warning. Every pool item is pinned OoT progression;
                        // one that never got a host is not "missing loot", it is a
                        // world the player cannot finish, and before this throw the
                        // return value was simply discarded. Tier A host
                        // eligibility cut the candidate set from ~2000 to a few
                        // dozen, so exhaustion is now plausible rather than
                        // theoretical. Throwing lands in the attempt ladder below,
                        // which restores the pre-attempt save and re-derives —
                        // a re-rolled MM fill scatters junk differently, so the
                        // eligible-host set genuinely changes per attempt.
                        const ComboForeignItemDef* pool = nullptr;
                        const int poolCount = Combo_GetForeignItemPool(&pool);
                        const int placed = Rando::Foreign::PlaceForeignItems();
                        if (placed < poolCount) {
                            // Two distinct causes land here — the eligible-host
                            // candidate set ran dry, or Combo_SetForeignPlacement
                            // refused an insert (cap/duplicate). PlaceForeignItems
                            // logs which to stderr immediately above; this message
                            // names both rather than asserting one.
                            throw std::runtime_error("Paired world could not host every foreign item: placed " +
                                                     std::to_string(placed) + " of " + std::to_string(poolCount) +
                                                     " (eligible hosts exhausted, or the placement table refused an "
                                                     "insert — see the [MM] foreign placement log above)");
                        }
                    }
#endif
                };

#ifdef RSBS_SINGLE_EXECUTABLE
                int rsbsWinningAttempt = 0;
                if (rsbsPaired) {
                    // --------------------------------------------------------
                    // The DETERMINISTIC ATTEMPT LADDER (ADR 0010 increment
                    // 1.2; accepted answer O3). Attempt n's final seed is
                    // Rando::Foreign::MixPairedFinalSeedForAttempt(n) — a pure
                    // function of (master seed, resolved MM options string, n)
                    // with attempt 0 byte-identical to the pre-ladder
                    // derivation, mirroring OoT's Hash(seed + settingsStr)
                    // double-reseed (Lane B contract). Nothing consumed the
                    // RNG between the first Ship_Random_Seed above and here,
                    // and every consumer (starting items, pools, fill) runs
                    // inside the attempt.
                    //
                    // Each retry restores the FULL pre-attempt save image and
                    // clears the aborted attempt's foreign placements, so an
                    // attempt can never observe a predecessor's partial fill
                    // (the fill's own error path also memcpy-restores, but the
                    // ladder does not rely on every throw site being that
                    // disciplined). Exhaustion (kPairedGenMaxAttempts
                    // dead-ends) rethrows into the catch below — vanilla
                    // revert, never a partial world — and the ARRIVAL gate
                    // (MM_Rando_PairOnCrossGameArrival) turns that into the
                    // loud #533 refusal surface: slot latched, player toasted,
                    // never a silent vanilla Termina.
                    // --------------------------------------------------------
                    Rando::Foreign::NotePairedGenerationOutcome(0, false);
                    gComboCtx.mmPairedAttempt = 0;
                    static SaveContext sPreAttemptSave;
                    memcpy(&sPreAttemptSave, &gSaveContext, sizeof(SaveContext));
                    for (int attempt = 0;; attempt++) {
                        if (attempt > 0) {
                            memcpy(&gSaveContext, &sPreAttemptSave, sizeof(SaveContext));
                            Combo_ClearForeignPlacements();
                        }
                        finalSeed = Rando::Foreign::MixPairedFinalSeedForAttempt((uint32_t)attempt);
                        gSaveContext.save.shipSaveInfo.rando.finalSeed = finalSeed;
                        Ship_Random_Seed(finalSeed);
                        try {
                            runGenerationOnce();
                            rsbsWinningAttempt = attempt;
                            Rando::Foreign::NotePairedGenerationOutcome(attempt + 1, false);
                            break;
                        } catch (const std::exception& attemptError) {
                            fprintf(stderr, "[MM] paired generation: attempt %d/%d dead-ended (%s)\n", attempt + 1,
                                    Rando::Foreign::kPairedGenMaxAttempts, attemptError.what());
                            if (attempt + 1 >= Rando::Foreign::kPairedGenMaxAttempts) {
                                Rando::Foreign::NotePairedGenerationOutcome(attempt + 1, true);
                                throw std::runtime_error(
                                    "Paired generation exhausted the attempt ladder: all " +
                                    std::to_string(Rando::Foreign::kPairedGenMaxAttempts) +
                                    " deterministic attempts dead-ended (last: " + std::string(attemptError.what()) +
                                    ")");
                            }
                        }
                    }
                    // Record the winning attempt as provenance, displaced by
                    // one so 0 stays the growth contract's "unset" (context.h):
                    // the spoiler carries the raw index, the combo record the
                    // displaced one. Regeneration under the same identity
                    // re-converges here by construction (the ladder is
                    // deterministic); the record makes that replay checkable.
                    gComboCtx.mmPairedAttempt = (uint32_t)rsbsWinningAttempt + 1u;
                    if (rsbsWinningAttempt > 0) {
                        fprintf(stderr, "[MM] paired generation: converged on ladder attempt %d (of max %d)\n",
                                rsbsWinningAttempt + 1, Rando::Foreign::kPairedGenMaxAttempts);
                    }
                } else {
                    runGenerationOnce();
                }
#else
                runGenerationOnce();
#endif

                if (CVarGetInteger("gRando.GenerateSpoiler", 1)) {
                    // The spoiler is a REPORT of the world, not part of it: the
                    // fill above already succeeded and the save already holds a
                    // complete, playable world. Before #439 a filesystem
                    // failure here (missing directory, read-only install)
                    // propagated to the outer catch, which reverts the save to
                    // vanilla — turning "could not write a log file" into
                    // "your paired world silently did not happen". Contain it:
                    // log loudly, keep the world.
                    try {
                        nlohmann::json spoiler = Rando::Spoiler::GenerateFromSaveContext();
                        spoiler["inputSeed"] = inputSeed;
#ifdef RSBS_SINGLE_EXECUTABLE
                        if (rsbsPaired) {
                            // ADR 0010 increment 1.2: the winning attempt-
                            // ladder index (0-based), recorded so the spoiler
                            // names the exact derivation that produced this
                            // world — Foreign.h documents the recipe.
                            spoiler["rsbsPairedAttempt"] = rsbsWinningAttempt;
                        }
#endif

                        std::string fileName = inputSeed + ".json";
                        Rando::Spoiler::SaveToFile(fileName, spoiler);

                        if (hadInputSeed) {
                            CVarSetString("gRando.SpoilerFile", fileName.c_str());
                        }
                        Rando::Spoiler::RefreshOptions();
                    } catch (const std::exception& e) {
                        SPDLOG_ERROR("Spoiler write failed (world is still valid): {}", e.what());
                        fprintf(stderr, "[MM] spoiler: NOT written — %s (the generated world is unaffected)\n",
                                e.what());
                        fflush(stderr);
                    }
                }

                Audio_PlaySfx(NA_SE_SY_ATTENTION_SOUND);
            } else {
                std::string fileName = CVarGetString("gRando.SpoilerFile", "");
                nlohmann::json spoiler = Rando::Spoiler::LoadFromFile(fileName);

                Rando::Spoiler::ApplyToSaveContext(spoiler);
                // Grant the starting stuff
                Rando::GrantStartingItems();

                Audio_PlaySfx(NA_SE_SY_ATTENTION_SOUND);
            }

            RANDO_SAVE_CHECKS[RC_STARTING_ITEM_DEKU_MASK].eligible = true;
            RANDO_SAVE_CHECKS[RC_STARTING_ITEM_SONG_OF_HEALING].eligible = true;

            S2H::GameHooks::Execute<GameInteractor::OnRandoSeedGeneration>();

        } catch (const std::exception& e) {
            SPDLOG_ERROR("Error with randomizer save creation: {}", e.what());
            Audio_PlaySfx(NA_SE_SY_QUIZ_INCORRECT);
#ifdef RSBS_SINGLE_EXECUTABLE
            // A failed generation reverts to a vanilla save below; any foreign
            // placements recorded for the aborted world must not survive it —
            // and neither may a stale attempt-provenance stamp (ADR 0010
            // increment 1.2): the reverted file authored no paired world.
            Combo_ClearForeignPlacements();
            gComboCtx.mmPairedAttempt = 0;
#endif
            gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_VANILLA;
            char invalidName[8] = { 18, 23, 31, 10, 21, 18, 13, 62 };
            memcpy(gSaveContext.save.saveInfo.playerData.playerName, invalidName, sizeof(invalidName));
            gSaveContext.save.saveInfo.playerData.newf[0] = '\0';
        }
    }
}
