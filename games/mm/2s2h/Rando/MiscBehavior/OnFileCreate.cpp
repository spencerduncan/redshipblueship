#include "MiscBehavior.h"
#include "Rando/Rando.h"
#include "Rando/Spoiler/Spoiler.h"
#include "Rando/Logic/Logic.h"
#include "2s2h/ShipUtils.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "ClockShuffle.h"
#include <spdlog/spdlog.h>
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
            if (CVarGetInteger("gRando.SpoilerFileIndex", 0) == 0) {
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
                for (auto& [randoOptionId, randoStaticOption] : Rando::StaticData::Options) {
                    RANDO_SAVE_OPTIONS[randoOptionId] =
                        (uint32_t)CVarGetInteger(randoStaticOption.cvar, randoStaticOption.defaultValue);
                }

                // If Skulltula tokens are not shuffled, use the vanilla requirement
                if (!RANDO_SAVE_OPTIONS[RO_SHUFFLE_GOLD_SKULLTULAS]) {
                    RANDO_SAVE_OPTIONS[RO_MINIMUM_SKULLTULA_TOKENS] = SPIDER_HOUSE_TOKENS_REQUIRED;
                }

#ifdef RSBS_SINGLE_EXECUTABLE
                if (rsbsPaired) {
                    // Pinned MM profile for the paired world (#392/#426): the
                    // vendored glitchless fill deterministically dead-ends, so
                    // the MVP pairs under Nearly No Logic — the spoiler log
                    // carries what logic would otherwise carry (Lane D).
                    // Pinned BEFORE the settings mix below so the profile is
                    // part of the world identity.
                    if (RANDO_SAVE_OPTIONS[RO_LOGIC] != RO_LOGIC_NEARLY_NO_LOGIC &&
                        RANDO_SAVE_OPTIONS[RO_LOGIC] != RO_LOGIC_NO_LOGIC &&
                        RANDO_SAVE_OPTIONS[RO_LOGIC] != RO_LOGIC_VANILLA) {
                        SPDLOG_INFO("Paired-world generation: pinning MM logic to Nearly No Logic (#426)");
                        RANDO_SAVE_OPTIONS[RO_LOGIC] = RO_LOGIC_NEARLY_NO_LOGIC;
                    }
                    // Re-seed with Hash(master seed + MM's finalized options),
                    // mirroring OoT's Hash(seed + settingsStr) double-reseed
                    // (Lane B contract): the same master seed reproduces this
                    // world only under the same MM profile. Nothing consumed
                    // the RNG between the first Ship_Random_Seed above and
                    // here, and every consumer (starting items, pools, fill)
                    // runs after this point.
                    finalSeed = Rando::Foreign::MixPairedFinalSeed();
                    gSaveContext.save.shipSaveInfo.rando.finalSeed = finalSeed;
                    Ship_Random_Seed(finalSeed);
                }
#endif

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
                    // gComboCtx.foreignPlacements (the MM table keeps RI_JUNK
                    // — ADR 0002). Runs before the spoiler write so the
                    // spoiler's foreign section describes this world.
                    Rando::Foreign::PlaceForeignItems();
                }
#endif

                if (CVarGetInteger("gRando.GenerateSpoiler", 1)) {
                    nlohmann::json spoiler = Rando::Spoiler::GenerateFromSaveContext();
                    spoiler["inputSeed"] = inputSeed;

                    std::string fileName = inputSeed + ".json";
                    Rando::Spoiler::SaveToFile(fileName, spoiler);

                    if (hadInputSeed) {
                        CVarSetString("gRando.SpoilerFile", fileName.c_str());
                    }
                    Rando::Spoiler::RefreshOptions();
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
            // placements recorded for the aborted world must not survive it.
            Combo_ClearForeignPlacements();
#endif
            gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_VANILLA;
            char invalidName[8] = { 18, 23, 31, 10, 21, 18, 13, 62 };
            memcpy(gSaveContext.save.saveInfo.playerData.playerName, invalidName, sizeof(invalidName));
            gSaveContext.save.saveInfo.playerData.newf[0] = '\0';
        }
    }
}
