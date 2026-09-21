/**
 * @file Tricks.cpp
 * @brief The MM trick ROW TABLE and its accessors (#578 part 1).
 *
 * ============================================================================
 * ATTRIBUTION
 * ============================================================================
 *
 * The display names and tooltips below are taken from OoTMM's
 * `packages/core/src/settings/tricks.ts`:
 *
 *     MIT License
 *     Copyright (c) 2020-2022 OoTMM Team
 *     https://github.com/OoTMM/OoTMM  (LICENSE, verbatim MIT)
 *
 * Read at OoTMM `master` on 2026-09-17; 85 `MM_*` keys, transcribed in source
 * order with the `MM_` prefix replaced by `MMRT_`. `MMRT_GBT_BOSS_KEY_ICE` at
 * the end is OURS and its strings are original.
 *
 * EXPLICIT NON-SOURCE: **mm-rando (GPL-3.0)**. Nothing in this file came from
 * it — see Tricks.h's ATTRIBUTION section for why that matters and why the
 * checklist-only use was deliberate.
 *
 * ============================================================================
 * WHAT THIS TABLE DOES *NOT* DO YET
 * ============================================================================
 *
 * It declares the vocabulary; it does not wire it. Exactly TWO keys have a
 * binding in this PR — `MMRT_KEG_EXPLOSIVES` (Logic/Logic.h's
 * `CAN_USE_EXPLOSIVE`, #578 finding (a)) and `MMRT_GBT_BOSS_KEY_ICE`
 * (Logic/Regions/GreatBayTemple.cpp, finding (b)). Every other live-capable key
 * is declared, settable, folded into the frozen identity, and consulted by
 * nothing. That is deliberate (operator ruling 2026-09-17: #578 is split, part 1
 * is the substrate). It is also WHY the keys must land first: the trick set
 * freezes into pairing identity at creation, so a file created before the
 * vocabulary existed has no honest answer for what its frozen trick set was.
 *
 * A declared-but-unbound key is not a vacuous control in ADR 0004 §5's sense
 * while it is described honestly, which is what the pane's per-row evidence is
 * for: the row says "no logic binding yet (part 2)" rather than pretending to
 * change the world.
 */
#include "Tricks.h"

#include <cstring>
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "variables.h"
}

namespace Rando {

namespace StaticData {

/**
 * One row. The `name` and `cvar` strings are DERIVED from the enumerator rather
 * than restated, so a key and its CVar can never drift apart (the same reason
 * StaticData/Options.cpp's `RO()` macro stringifies its id).
 */
#define MMRT(key, areaArg, tagsArg, reservedArg, displayArg, tooltipArg, reasonArg)                          \
    {                                                                                                        \
        MMRT_##key, {                                                                                        \
            MMRT_##key, "MMRT_" #key, "gRando.Tricks.MMRT_" #key, areaArg, (uint32_t)(tagsArg), reservedArg, \
                displayArg, tooltipArg, reasonArg                                                            \
        }                                                                                                    \
    }

// clang-format off
std::map<MMRandoTrickId, RandoStaticTrick> Tricks = {
    MMRT(HIDDEN_GROTTOS, MMRTA_GENERAL, MMRTT_NOVICE, false,
         "Hidden Grottos (MM) without Stone of Agony",
         "All hidden grottos will no longer require the Stone of Agony for logic.", NULL),
    MMRT(LENS, MMRTA_GENERAL, MMRTT_NOVICE, false,
         "Fewer Lens Requirements (MM)",
         "Makes Lens of Truth not a required item for most checks, excluding Shiro (Stone Mask check) and climbing the wall to Darmani", NULL),
    MMRT(TUNICS, MMRTA_GENERAL, MMRTT_NOVICE, false,
         "Fewer Tunic Requirements (MM)",
         "Most things that would normally require Zora Tunic no longer will. Pirate Fortress Sewers and Pinnacle Rock will still require it.", NULL),
    MMRT(DEKU_STICK_FIGHTING, MMRTA_GENERAL, MMRTT_NOVICE, false,
         "Deku Stick Fighting",
         "Use Deku Sticks to defeat enemies instead of other weapons or a sword.", NULL),
    MMRT(PALACE_BEAN_SKIP, MMRTA_DEKU_PALACE, MMRTT_NOVICE, false,
         "Skip Planting Beans in Deku Palace",
         "Backflip onto the doorframe in the left side of Deku Palace to skip planting the beans, removing the bottle requirement for the Sonata check", NULL),
    MMRT(DARMANI_WALL, MMRTA_MOUNTAIN_VILLAGE, MMRTT_ADVANCED, false,
         "Climb Mountain Village Wall Blind",
         "Climb the Mountain Village wall without Lens of Truth.", NULL),
    MMRT(NO_SEAHORSE, MMRTA_ZORA_CAPE, MMRTT_ADVANCED, false,
         "Pinnacle Rock without Seahorse",
         "Cross Pinnacle Rock blind. The signs are your markers for turns.", NULL),
    MMRT(ZORA_HALL_HUMAN, MMRTA_ZORA_HALL, MMRTT_NOVICE, false,
         "Swim to Zora Hall as Human",
         "Swim around Zora Hall to reach the back without Zora Mask", NULL),
    MMRT(ICELESS_IKANA, MMRTA_IKANA_CANYON, MMRTT_ADVANCED, false,
         "Climb Ikana Canyon without Ice Arrows",
         "With a precise Hookshot position, you can hit the first tree directly from the riverside, removing the Ice Arrow requirement", NULL),
    MMRT(ONE_MASK_STONE_TOWER, MMRTA_STONE_TOWER, MMRTT_INTERMEDIATE, false,
         "Climb Stone Tower with One Mask",
         "Playing the Elegy of Emptiness while standing on a block to move with it, along with other clever Elegy of Emptiness usage, allows you to climb Stone Tower using just two Elegy statues", NULL),
    MMRT(ISTT_EYEGORE, MMRTA_INVERTED_STONE_TOWER_TEMPLE, MMRTT_NOVICE, false,
         "Inverted Stone Tower Temple Early Eyegore",
         "Using either Hookshot and Light Arrows, or bombs for a Recoil Flip, you can access the central bridge with the Eyegore on it early, skipping a portion of the dungeon", NULL),
    MMRT(SCT_NOTHING, MMRTA_CLOCK_TOWN, MMRTT_ADVANCED, false,
         "South Clock Town Chest with Nothing",
         "Climb the roof with a precise jump to access the South Clock Town Chest", NULL),
    MMRT(GORON_BOMB_JUMP, MMRTA_MILK_ROAD, MMRTT_NOVICE, false,
         "Bomb Jump Fences as Goron",
         "Place down bombs or a Powder Keg, then use the Goron Pound to leap into the air and get damaged mid-air by the explosion to hop over fences", NULL),
    MMRT(BOMBER_GUESS, MMRTA_CLOCK_TOWN, MMRTT_NOVICE, false,
         "Guess Bombers' Code",
         "Guess the Bombers' Code for Astral Observatory from 120 possible combinations. Grants access to the Bomber's Notebook check when entering ECT from the Bombers Hideout.", NULL),
    MMRT(CAPTAIN_SKIP, MMRTA_GREAT_BAY_COAST, MMRTT_NOVICE, false,
         "Guess Oceanside Spider House Code",
         "Guess the code on the masks you hit with arrows.", NULL),
    MMRT(ISTT_ENTRY_JUMP, MMRTA_INVERTED_STONE_TOWER_TEMPLE, MMRTT_ADVANCED, false,
         "Inverted Stone Tower Temple Long Jump to Death Armos",
         "Using a precise bomb long jump, you can make it to the switch on the left side of the room", NULL),
    MMRT(HARD_HOOKSHOT, MMRTA_GENERAL, MMRTT_ADVANCED, false,
         "Precise Short Hookshot Usage",
         "With this trick enabled, using the Short Hookshot gives you logical access to the Deku Palace Bean Grotto chest, the Road to Ikana chest and Ikana Canyon through the Road to Ikana tree", NULL),
    MMRT(PFI_BOAT_HOOK, MMRTA_PIRATES_FORTRESS, MMRTT_ADVANCED, false,
         "Enter Pirates' Fortress Interior using Hookshot from the Boats",
         "From the boats, you can make a precise shot to the barrels in front of the interior entrance", NULL),
    MMRT(PALACE_GUARD_SKIP, MMRTA_DEKU_PALACE, MMRTT_ADVANCED, false,
         "Backflip over Deku Palace Guards",
         "With a precise backflip on the fence, jump over the guards as Human Link", NULL),
    MMRT(SHT_HOT_WATER, MMRTA_SNOWHEAD_TEMPLE, MMRTT_NOVICE, false,
         "Complete Snowhead Temple using Hot Spring Water",
         "Use Hot Spring Water to melt all the ice instead of Fire Arrows or Din's Fire.", NULL),
    MMRT(SHT_STICKS_RUN, MMRTA_SNOWHEAD_TEMPLE, MMRTT_ADVANCED, false,
         "Access SHT Pillar Fireless with Precise Stick Run",
         "Use the lower torch on the third floor in the center room to light the stick, drop down to the pillar room, enter and light the closest torch. Use another stick for the two other torches.", NULL),
    MMRT(SHT_PILLARLESS, MMRTA_SNOWHEAD_TEMPLE, MMRTT_ADVANCED, false,
         "Snowhead Temple Skip Raising Pillar",
         "Destroy the snowballs with Fire Arrows or bombs and then jump down. A precise jump slash may help getting onto the platform.", NULL),
    MMRT(SHT_PILLAR_ROOM_HOOKSHOT, MMRTA_SNOWHEAD_TEMPLE, MMRTT_NOVICE, false,
         "Snowhead Temple Hookshot Up Pillar Room",
         "From the ground floor of pillar room, use a Hookshot to kill the freezards and then climb using the chest that spawns.", NULL),
    MMRT(KEG_EXPLOSIVES, MMRTA_GENERAL, MMRTT_NOVICE, false,
         "Use Powder Kegs as Explosives",
         "Allows Powder Kegs to be considered in logic for one-time explosives usages, such as blowing up boulders, opening up hidden grottos and destroying breakable walls.", NULL),
    MMRT(DOG_RACE_CHEST_NOTHING, MMRTA_ROMANI_RANCH, MMRTT_ADVANCED, false,
         "Doggy Racetrack Chest with Nothing",
         "Climb the fence and make a precise jump to get to the chest in the Doggy Racetrack area", NULL),
    MMRT(MAJORA_LOGIC, MMRTA_MOON, MMRTT_NOVICE, false,
         "Fight Majora to Reset Time",
         "Access to fight Majora logically counts as a time reset, as an alternative to playing Song of Time", NULL),
    MMRT(SOUTHERN_SWAMP_SCRUB_HP_GORON, MMRTA_SOUTHERN_SWAMP, MMRTT_NOVICE, false,
         "Southern Swamp Scrub HP as Goron",
         "Use Goron's ground pound in front of the Tourist Center door to land on the roof and reach the heart piece", NULL),
    MMRT(SOUTHERN_SWAMP_SCRUB_HP_BOOMERANG, MMRTA_SOUTHERN_SWAMP, MMRTT_INTERMEDIATE | MMRTT_COMBO, true,
         "Southern Swamp Scrub HP with Boomerang",
         "Toss the Boomerang at a specific angle and sidehop to get the Heart Piece", "needs increment 3: requires Boomerang (an OoT-side item)"),
    MMRT(GBC_COW_LIKELIKE_ELEVATOR, MMRTA_GREAT_BAY_COAST, MMRTT_NOVICE, false,
         "Great Bay Coast Cow Grotto LikeLike Elevator",
         "At night time draw a LikeLike lose to the Cow Grotto ledge to be shot on top of the ledge", NULL),
    MMRT(ZORA_HALL_SCRUB_HP_NO_DEKU, MMRTA_ZORA_HALL, MMRTT_NOVICE, false,
         "Zora Hall Scrub HP without Deku",
         "As either Goron or Zora Link, jump up to the heart piece. Linked playlist has both versions.", NULL),
    MMRT(ZORA_HALL_DOORS, MMRTA_ZORA_HALL, MMRTT_INTERMEDIATE, false,
         "Access the doors in Zora Hall using Short Hookshot Anywhere",
         "Using Hookshot Anywhere, it is possible to hookshot the bottom of the door through the Zora standing in front, and mash A to open the door. It is somewhat precise but works with all doors.", NULL),
    MMRT(IKANA_ROOF_PARKOUR, MMRTA_IKANA_CASTLE, MMRTT_NOVICE, false,
         "Jump from Ikana Castle's Roof Interior to Exterior",
         "Jump off of a piece of rubble to a sloped wall, walk up, and jump across the block to the outside of the fence. Walk around the edge to get to the other side.", NULL),
    MMRT(IKANA_PILLAR_ENTRANCE_FLOAT, MMRTA_IKANA_CASTLE, MMRTT_NOVICE, false,
         "Float from Ikana Castle's Roof to Entrance",
         "Float from the Ikana Castle roof to the main entrance of the courtyard using Deku Mask or Hover Boots", NULL),
    MMRT(IKANA_PILLAR_ENTRANCE_JUMP, MMRTA_IKANA_CASTLE, MMRTT_NOVICE, false,
         "Jump from Ikana Castle's Roof to Entrance",
         "Jump from the Ikana Castle roof to the main entrance of the courtyard", NULL),
    MMRT(POST_OFFICE_GAME, MMRTA_CLOCK_TOWN, MMRTT_NOVICE, false,
         "Post Office Timing Game without Bunny Hood",
         "Obtain the reward without an on-screen timer to help you", NULL),
    MMRT(WELL_HSW, MMRTA_IKANA_CANYON, MMRTT_NOVICE, false,
         "Well's Hot Spring Water without Killing Dexihand",
         "Grab the water before the hand grabs you.", NULL),
    MMRT(ISTT_CHUCHU_LESS, MMRTA_INVERTED_STONE_TOWER_TEMPLE, MMRTT_NOVICE, false,
         "ISTT Block Room without Chuchu Jellies",
         "Normally the room contains Chuchu Jellies to restock your arrows and magic. With this trick on, logic can expect you to do this room without the Soul of Chuchus.", NULL),
    MMRT(GBT_WATERWHEEL_GORON, MMRTA_GREAT_BAY_TEMPLE, MMRTT_ADVANCED, false,
         "Cross GBT Waterwheel Room as Goron",
         "Skip the first yellow turnkey with a precise Goron Roll", NULL),
    MMRT(GBT_ENTRANCE_BOW, MMRTA_GREAT_BAY_TEMPLE, MMRTT_INTERMEDIATE, false,
         "Great Bay Temple Entrance Chest using only Bow",
         "Light the four torches using somewhat precise arrow shots", NULL),
    MMRT(OOB_MOVEMENT, MMRTA_GENERAL, MMRTT_NOVICE, false,
         "Walk Along Surfaces Out of Bounds",
         "With this trick enabled, logic may expect you to use Short Hookshot Anywhere to reach normally inaccessible surfaces to get behind the Milk Road Boulder and (with 3 elegy statues) climb Stone Tower.", NULL),
    MMRT(ST_UPDRAFTS, MMRTA_STONE_TOWER, MMRTT_NOVICE, false,
         "Stone Tower Updrafts without Deku Mask",
         "This room can be traversed using Recoil Flips or Goron Mask instead of Deku Mask.", NULL),
    MMRT(ESCAPE_CAGE, MMRTA_DEKU_PALACE, MMRTT_NOVICE, false,
         "Escape the Monkey Cage with Hookshot Anywhere",
         "For Interior ER, it is possible to hookshot over the fence and land in a spot where Deku Mask can then be used to leave out the front entrance.", NULL),
    MMRT(GBT_FAIRY2_HOOK, MMRTA_GREAT_BAY_TEMPLE, MMRTT_ADVANCED, false,
         "GBT First Underwater Fairy with Short Hookshot Anywhere",
         "The first Stray Fairy in a bubble can be obtained with a precise Hookshot angle.", NULL),
    MMRT(GBT_CENTRAL_GEYSER, MMRTA_GREAT_BAY_TEMPLE, MMRTT_NOVICE, false,
         "GBT Central Room without Zora using Fire & Ice Arrows or an OoT Magic Spell",
         "Using Fire and Ice Arrows on the water stream above the ladder or an OoT Spell while on the spinning platform, it is possible to sink down during the cutscene, then swim into one of the tunnels.", NULL),
    MMRT(BANK_ONE_WALLET, MMRTA_CLOCK_TOWN, MMRTT_NOVICE, false,
         "Bank Rewards Require One Less Wallet",
         "The 500-Rupee item reward will only require the Child Wallet, and the 1000-rupee item reward will require the Adult Wallet.", NULL),
    MMRT(BANK_NO_WALLET, MMRTA_CLOCK_TOWN, MMRTT_NOVICE, false,
         "Bank Rewards Require No Extra Wallets",
         "All bank rewards will only require the Child Wallet.", NULL),
    MMRT(CLOCK_TOWER_WAIT, MMRTA_CLOCK_TOWN, MMRTT_NOVICE, false,
         "Wait for the Clock Tower to Open When Shuffled",
         "With this trick enabled and the Clock Tower entrance shuffled, it may be expected to wait for the Clock Tower to open without a way to quickly advance time.", NULL),
    MMRT(WFT_RUPEES_ICE, MMRTA_WOODFALL_TEMPLE, MMRTT_NOVICE, false,
         "Collect the Pillar Rupees in Woodfall Temple using Ice Arrows",
         "The rupees next to Odolwa's door can be jumped to after creating ice platforms in the water.", NULL),
    MMRT(ISTT_RUPEES_GORON, MMRTA_INVERTED_STONE_TOWER_TEMPLE, MMRTT_NOVICE, false,
         "Collect the Floating Rupees in ISTT as Goron",
         "In the room before Twinmold, Goron can collect the rupees by rolling on the platform over them.", NULL),
    MMRT(BOMBER_BACKFLIP, MMRTA_CLOCK_TOWN, MMRTT_NOVICE, false,
         "Backflip over the Bomber in East Clock Town",
         "By backwalking at an angle right next to the kid, the \"Speak\" prompt disappears, allowing you to backflip over the kid. Does not grant access to the Bomber's Notebook check when entering ECT from the Bombers Hideout.", NULL),
    MMRT(NCT_TINGLE, MMRTA_CLOCK_TOWN, MMRTT_NOVICE, false,
         "Jump Slash Tingle in North Clock Town",
         "Jump off the tree and jump slash Tingle's balloon. Sticks will not work.", NULL),
    MMRT(GBT_FIRELESS, MMRTA_GREAT_BAY_TEMPLE, MMRTT_NOVICE, false,
         "Great Bay Temple without Fire Arrows",
         "It is possible to traverse the final room in the reverse loop using Zora Mask or Adult Link with a jump slash.", NULL),
    MMRT(IGOS_DINS, MMRTA_IKANA_CASTLE, MMRTT_NOVICE | MMRTT_COMBO, true,
         "Burn Igos' curtain with Din's Fire",
         "Igos' left curtain can be burned by standing at the top of the stairs next to his throne.", "needs increment 3: requires Din's Fire (an OoT-side item)"),
    MMRT(BIO_BABA_CHU, MMRTA_GREAT_BAY_COAST, MMRTT_ADVANCED, false,
         "Destroy the Bio Baba Grotto Hives with a Bombchu",
         "The hives in this grotto can be destroyed with a precise bombchu placement.", NULL),
    MMRT(BIO_BABA_LUCK, MMRTA_GREAT_BAY_COAST, MMRTT_NOVICE, false,
         "Bio Baba Grotto Lilypad Luck",
         "If the item happens to land on one of the lilypads, you can get it without Zora Mask or Iron Boots.", NULL),
    MMRT(WF_SHRINE_HOVERS, MMRTA_WOODFALL, MMRTT_ADVANCED | MMRTT_COMBO, true,
         "Woodfall Owl Chest with Hover Boots and Jump Slash",
         "The chest can be reached with a tight jump slash at the end of the hover duration.", "needs increment 3: requires Hover Boots (an OoT-side item)"),
    MMRT(WFT_LOBBY_HOVERS, MMRTA_WOODFALL_TEMPLE, MMRTT_EXPERT | MMRTT_COMBO, true,
         "Woodfall Temple Lobby with Damage Boost and Hover Boots",
         "The lobby can be traversed by damaging yourself with bombs at certain points. Very difficult.", "needs increment 3: requires Hover Boots (an OoT-side item)"),
    MMRT(SOARING_ZORA, MMRTA_WOODFALL, MMRTT_NOVICE, false,
         "Zora Long Jump to the Soaring Tablet",
         "The Soaring Tablet can be long jumped to as Zora from the giant flower in the back of Poison Swamp or from the Woodfall entrance.", NULL),
    MMRT(SOARING_HOVERS, MMRTA_WOODFALL, MMRTT_ADVANCED | MMRTT_COMBO, true,
         "Jump Slash or Damage Boost to the Soaring Tablet with Hover Boots and Bunny Hood",
         "Using Bunny Hood for extra speed, Hover Boots can be used to reach the Soaring Tablet with a tight jump slash or a damage boost with explosives.", "needs increment 3: requires Hover Boots (an OoT-side item)"),
    MMRT(LULLABY_SKIP_IRONS, MMRTA_SNOWHEAD, MMRTT_NOVICE | MMRTT_COMBO, true,
         "Skip playing Goron Lullaby by using Iron Boots",
         "By combining Iron Boots to not be blown away by the wind and Goron to roll up the slope, Goron Lullaby can be skipped.", "needs increment 3: requires Iron Boots (an OoT-side item)"),
    MMRT(PATH_SNOWHEAD_HOVERS, MMRTA_SNOWHEAD, MMRTT_NOVICE | MMRTT_COMBO, true,
         "Cross Path to Snowhead using Hover Boots",
         "Hover Boots can be used to slide off the slopes on the side and get across the gaps.", "needs increment 3: requires Hover Boots (an OoT-side item)"),
    MMRT(GBT_WATERWHEEL_HOVERS, MMRTA_GREAT_BAY_TEMPLE, MMRTT_NOVICE | MMRTT_COMBO, true,
         "Cross GBT Waterwheel Room using Hover Boots",
         "Similar to using Goron, Hover Boots can be used to get on the water wheel and then reach the central room.", "needs increment 3: requires Hover Boots (an OoT-side item)"),
    MMRT(GBT_CENTER_POT_IRONS, MMRTA_GREAT_BAY_TEMPLE, MMRTT_NOVICE | MMRTT_COMBO, true,
         "Great Bay Temple Center Underwater Pot using only Iron Boots",
         "The pot can be broken by rolling into it, allowing you to collect the Stray Fairy.", "needs increment 3: requires Iron Boots (an OoT-side item)"),
    MMRT(GBT_RED1_HOVERS, MMRTA_GREAT_BAY_TEMPLE, MMRTT_ADVANCED | MMRTT_COMBO, true,
         "Reach the First Red Turnkey in GBT using Hover Boots, Bunny Hood, and a Jump Slash",
         "The red turnkey in front of Wart can be reached with a precise jump slash after the hover duration, using Bunny Hood for extra speed.", "needs increment 3: requires Hover Boots (an OoT-side item)"),
    MMRT(GBT_GREEN2_UPPER_HOVERS, MMRTA_GREAT_BAY_TEMPLE, MMRTT_NOVICE | MMRTT_COMBO, true,
         "Reach the Upper Chest in GBT's Second Green Room using Hover Boots",
         "The upper chest in the green water wheel room can be reached by using Hover Boots to move between the wheel's blades.", "needs increment 3: requires Hover Boots (an OoT-side item)"),
    MMRT(GYORG_IRONS, MMRTA_GREAT_BAY_TEMPLE, MMRTT_NOVICE | MMRTT_COMBO, true,
         "Fight Gyorg as Human using Iron Boots and Hookshot",
         "Gyorg can be damaged with Hookshot. Turning this trick on will enable the fight in logic with Iron Boots and Hookshot.", "needs increment 3: requires Iron Boots (an OoT-side item)"),
    MMRT(STT_LAVA_BLOCK_HOVERS, MMRTA_STONE_TOWER_TEMPLE, MMRTT_NOVICE | MMRTT_COMBO, true,
         "Stone Tower Temple Map Chest using Hover Boots",
         "After using the Hookshot to reach the chest on the high platform in the lava room, one can then use the Hover Boots to land on top of the sun block.", "needs increment 3: requires Hover Boots (an OoT-side item)"),
    MMRT(ISTT_ENTRY_HOVER, MMRTA_INVERTED_STONE_TOWER_TEMPLE, MMRTT_NOVICE | MMRTT_COMBO, true,
         "Inverted Stone Tower Temple Death Armos using Hover Boots and Bunny Hood",
         "The Death Armos switch can be reached by using the Hover Boots and then sidehopping onto the platform, with Bunny Hood for extra speed.", "needs increment 3: requires Hover Boots (an OoT-side item)"),
    MMRT(GYORG_POTS_DIVE, MMRTA_GREAT_BAY_TEMPLE, MMRTT_NOVICE, false,
         "Dive Down for Gyorg's Pots with Blast Mask",
         "After defeating Gyorg, dive down and blow up the pots with Blast Mask.", NULL),
    MMRT(STT_POT_BOMBCHU_DIVE, MMRTA_STONE_TOWER_TEMPLE, MMRTT_NOVICE, false,
         "STT Water Room Shallow Pots Dive with Bombchu",
         "The pots closer to the surface in the water room can be broken using Bombchus, then dived down to.", NULL),
    MMRT(STOCK_POT_WAIT, MMRTA_CLOCK_TOWN, MMRTT_NOVICE, false,
         "Wait outside Stock Pot Inn's roof for closing",
         "Normally, logic will not force you to wait on the roof for night inn checks. Enabling this will remove that restriction.", NULL),
    MMRT(STAGE_LIGHTS_DIN, MMRTA_ZORA_HALL, MMRTT_NOVICE | MMRTT_COMBO, true,
         "Light the Zora Hall Stage Lights using Din's Fire, Bow, and Hookshot",
         "The item can be obtained without Fire Arrows by first reaching either torch with the hookshot, using Din's Fire, then shooting an arrow to the other torch.", "needs increment 3: requires Din's Fire (an OoT-side item)"),
    MMRT(RANCH_FARORE, MMRTA_ROMANI_RANCH, MMRTT_NOVICE | MMRTT_COMBO, true,
         "Romani's Ranch locations using Farore's Wind and Time Reset",
         "It is possible to place Farore's Wind in Romani Ranch on Day 3, reset time and warp back to Romani Ranch to obtain the Day 1 and 2 checks without the Powder Keg", "needs increment 3: requires Farore's Wind (an OoT-side item)"),
    MMRT(EVAN_FARORE, MMRTA_ZORA_HALL, MMRTT_NOVICE | MMRTT_COMBO, true,
         "Evan's Piece of Heart using Farore's Wind and Time Reset",
         "After beating Great Bay Temple, the door to Evan's Room will no longer be blocked. Use Farore's Wind inside the empty room, reset time so that Evan reappears in his room, and warp back.", "needs increment 3: requires Farore's Wind (an OoT-side item)"),
    MMRT(KEG_TRIAL_HEATLESS, MMRTA_MOUNTAIN_VILLAGE, MMRTT_NOVICE, false,
         "Powder Keg Trial without Thawing Ice using Hookshot Anywhere",
         "With Hookshot Anywhere, it is possible to hookshot through the little gap in the left bottom corner.", NULL),
    MMRT(KEG_HOOKBUNNY, MMRTA_MILK_ROAD, MMRTT_NOVICE, false,
         "Powder Keg Trial with only Long Hookshot and Bunny Hood",
         "With the Strength 3 for MM and Keg usable by Human Settings enabled; you can carry the keg from Medigoron to the Racetrack Boulder with Long Hookshot via the scarecrows in both areas and throwing the keg up the slopes.", NULL),
    MMRT(KEG_HOVERBUNNY, MMRTA_MILK_ROAD, MMRTT_NOVICE | MMRTT_COMBO, true,
         "Powder Keg Trial with only Hoverboots and Bunny Hood",
         "With the Strength 3 for MM and Keg usable by Human Settings enabled; you can side hop up the slopes in both areas while throwing the keg up the slopes as you go.", "needs increment 3: requires Hover Boots (an OoT-side item)"),
    MMRT(STT_LAVA_SWITCH_HAMMER, MMRTA_STONE_TOWER_TEMPLE, MMRTT_EXPERT | MMRTT_COMBO, true,
         "Stone Tower Temple Lava Room switch without Goron",
         "The switch in the lava room with updrafts can be pressed without Goron by using Megaton Hammer, Iron Boots, and Bunny Hood. It has pretty tight timing.", "needs increment 3: requires Iron Boots + Megaton Hammer (an OoT-side item)"),
    MMRT(HIVE_BOMBCHU, MMRTA_GENERAL, MMRTT_INTERMEDIATE, false,
         "Destroy Beehives using Bombchu (MM)",
         "Use some careful timing with a Bombchu to blow up the beehives across Termina.", NULL),
    MMRT(TWINMOLD_BOW, MMRTA_INVERTED_STONE_TOWER_TEMPLE, MMRTT_NOVICE, false,
         "Twinmold with Bow (MM)",
         "Defeat Twinmold using only the Bow.", NULL),
    MMRT(TWINMOLD_FIRE_AND_ICE, MMRTA_INVERTED_STONE_TOWER_TEMPLE, MMRTT_NOVICE, false,
         "Twinmold with Fire and Ice Arrows (MM)",
         "Defeat Twinmold using only Fire and Ice Arrows.", NULL),
    MMRT(KEG_RED_BOULDER, MMRTA_GENERAL, MMRTT_NOVICE, false,
         "Break Red Boulders using Powder Keg (MM)",
         "Use a Powder Keg to break Red Boulders. Only relevant with Strengths in MM", NULL),
    MMRT(GBT_BABA_ENTRY_BOMBCHU, MMRTA_GREAT_BAY_TEMPLE, MMRTT_ADVANCED, false,
         "Enter the Bio Baba room by using a precise Bombchu launch.",
         "By standing next to the Stray Fairy pot, it is possible to get an angle for the Bombchu to crawl along and blow up the Dexihands blocking access.", NULL),
    MMRT(CAPE_LIKE_LIKE_BOMBCHU, MMRTA_ZORA_CAPE, MMRTT_ADVANCED, false,
         "Defeat the waterfall Like Like in Zora Cape by using a precise Bombchu launch.",
         "It is possible to get a precise angle for the Bombchu to crawl along and blow up the Like Like for the Piece of Heart.", NULL),
    MMRT(ALIENS_DIN, MMRTA_ROMANI_RANCH, MMRTT_INTERMEDIATE | MMRTT_COMBO, true,
         "Defend Romani Ranch from the aliens using only Din's Fire",
         "Din's Fire is able to defeat the aliens and with careful usage of it can defend the ranch for the entire duration, needing only one refill of magic.", "needs increment 3: requires Din's Fire (an OoT-side item)"),
    /*
     * OURS (finding (b)). Strings original; no OoTMM equivalent exists, so the
     * tooltip states the route AND the provenance, because a player turning it
     * on is re-enabling an edge upstream removed.
     */
    MMRT(GBT_BOSS_KEY_ICE, MMRTA_GREAT_BAY_TEMPLE, MMRTT_INTERMEDIATE, false,
         "Great Bay Temple Boss Key Room with Ice Arrows",
         "Freeze the compass room's water with Ice Arrows as Zora to reach the boss key chest. Upstream 2Ship treats this as a trick and ships it off; with this on, redship's logic may expect it.", NULL),
};
// clang-format on

#undef MMRT

MMRandoTrickId GetTrickIdFromName(const char* name) {
    if (name == nullptr) {
        return MMRT_MAX;
    }
    for (auto& [mmRandoTrickId, randoStaticTrick] : Tricks) {
        if (strcmp(name, randoStaticTrick.name) == 0) {
            return mmRandoTrickId;
        }
    }
    return MMRT_MAX;
}

const char* GetTrickAreaName(MMRandoTrickArea area) {
    switch (area) {
        case MMRTA_GENERAL:
            return "General";
        case MMRTA_CLOCK_TOWN:
            return "Clock Town";
        case MMRTA_TERMINA_FIELD:
            return "Termina Field";
        case MMRTA_SOUTHERN_SWAMP:
            return "Southern Swamp";
        case MMRTA_DEKU_PALACE:
            return "Deku Palace";
        case MMRTA_WOODFALL:
            return "Woodfall";
        case MMRTA_WOODFALL_TEMPLE:
            return "Woodfall Temple";
        case MMRTA_MOUNTAIN_VILLAGE:
            return "Mountain Village";
        case MMRTA_SNOWHEAD:
            return "Snowhead";
        case MMRTA_SNOWHEAD_TEMPLE:
            return "Snowhead Temple";
        case MMRTA_MILK_ROAD:
            return "Milk Road";
        case MMRTA_ROMANI_RANCH:
            return "Romani Ranch";
        case MMRTA_GREAT_BAY_COAST:
            return "Great Bay Coast";
        case MMRTA_ZORA_CAPE:
            return "Zora Cape";
        case MMRTA_ZORA_HALL:
            return "Zora Hall";
        case MMRTA_PIRATES_FORTRESS:
            return "Pirates' Fortress";
        case MMRTA_GREAT_BAY_TEMPLE:
            return "Great Bay Temple";
        case MMRTA_IKANA_CANYON:
            return "Ikana Canyon";
        case MMRTA_IKANA_CASTLE:
            return "Ikana Castle";
        case MMRTA_STONE_TOWER:
            return "Stone Tower";
        case MMRTA_STONE_TOWER_TEMPLE:
            return "Stone Tower Temple";
        case MMRTA_INVERTED_STONE_TOWER_TEMPLE:
            return "Inverted Stone Tower Temple";
        case MMRTA_MOON:
            return "The Moon";
        default:
            // Visible placeholder rather than NULL: every caller is a
            // printf-family or ImGui text call, where a NULL is UB rather than
            // an empty header.
            return "(unknown area)";
    }
}

const char* GetTrickTagName(MMRandoTrickTag tag) {
    switch (tag) {
        case MMRTT_NOVICE:
            return "Novice";
        case MMRTT_INTERMEDIATE:
            return "Intermediate";
        case MMRTT_ADVANCED:
            return "Advanced";
        case MMRTT_EXPERT:
            return "Expert";
        case MMRTT_EXPERIMENTAL:
            return "Experimental";
        case MMRTT_GLITCH:
            return "Glitch";
        case MMRTT_COMBO:
            return "Needs OoT items";
        default:
            return "(unknown tag)";
    }
}

bool IsTrickEnabled(MMRandoTrickId mmRandoTrickId) {
    // Bounds first. A condition is evaluated inside the fill for every region on
    // every pass, so an out-of-range id must read false rather than index off
    // the end of a save array the generator then trusts.
    if (mmRandoTrickId < 0 || mmRandoTrickId >= MMRT_MAX) {
        return false;
    }
    // A RESERVED key is inert whatever the save says. This is the second half of
    // the reserved contract (Tricks.h): the writer refuses to arm one, and the
    // reader refuses to honour one that was armed anyway — by a hand-edited
    // save, by a .redsave written before the key was reserved, or by a future
    // pane bug. Increment 3 flips the flag and both halves come alive together.
    auto it = Tricks.find(mmRandoTrickId);
    if (it == Tricks.end() || it->second.reserved) {
        return false;
    }
    // The FROZEN per-file set, never the CVar. See Tricks.h.
    return gSaveContext.save.shipSaveInfo.rando.randoSaveTricks[mmRandoTrickId] != 0;
}

bool ResolveTrickFromCVar(MMRandoTrickId mmRandoTrickId) {
    if (mmRandoTrickId < 0 || mmRandoTrickId >= MMRT_MAX) {
        return false;
    }
    auto it = Tricks.find(mmRandoTrickId);
    if (it == Tricks.end() || it->second.reserved) {
        return false;
    }
    return CVarGetInteger(it->second.cvar, 0) != 0;
}

} // namespace StaticData

} // namespace Rando
