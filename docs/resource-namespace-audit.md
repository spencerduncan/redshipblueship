# Resource Namespace Collision Audit

## Executive Summary

**CRITICAL**: Significant namespace collisions exist between OoT and MM archives.

| Category | OoT Count | MM Count | Collisions | Collision % |
|----------|-----------|----------|------------|-------------|
| Objects  | 382 | 464 | 151 | 39.5% of OoT |
| Overlays | 34 | 45 | 14 | 41.2% of OoT |
| Scenes   | 116 | 102 | 0 | 0.0% |

**Total namespace collisions: 165**

## Collision Categories

### Object (105 collisions)

Various game objects (enemies, NPCs, items):

```
object_ahg
object_am
object_ani
object_aob
object_b_heart
object_bba
object_bdoor
object_bg
object_bigokuta
object_bji
object_bob
object_boj
object_bombf
object_bombiwa
object_box
object_bubble
object_cne
object_cow
object_crow
object_cs
object_d_hsblock
object_d_lift
object_daiku
object_dekubaba
object_dekunuts
object_dnk
object_dns
object_dodongo
object_dog
object_ds2
... and 75 more
```

### Get Item (41 collisions)

Get Item (gi) models for collectible items:

```
object_gi_arrow
object_gi_arrowcase
object_gi_bean
object_gi_bomb_1
object_gi_bomb_2
object_gi_bombpouch
object_gi_bosskey
object_gi_bottle
object_gi_bow
object_gi_compass
object_gi_fish
object_gi_ghost
object_gi_glasses
object_gi_golonmask
object_gi_heart
object_gi_hearts
object_gi_hookshot
object_gi_insect
object_gi_key
object_gi_ki_tan_mask
object_gi_liquid
object_gi_longsword
object_gi_m_arrow
object_gi_magicpot
object_gi_map
object_gi_melody
object_gi_milk
object_gi_nuts
object_gi_ocarina
object_gi_purse
... and 11 more
```

### Overlay (14 collisions)

Actor overlays with identical names but different implementations:

```
ovl_Arrow_Fire
ovl_Arrow_Ice
ovl_Arrow_Light
ovl_En_Clear_Tag
ovl_En_Holl
ovl_En_Kanban
ovl_En_Sda
ovl_En_Sth
ovl_Oceff_Spot
ovl_Oceff_Storm
ovl_Oceff_Wipe
ovl_Oceff_Wipe2
ovl_Oceff_Wipe3
ovl_Oceff_Wipe4
```

### Core Gameplay (3 collisions)

These are fundamental game assets that both games need:

```
gameplay_dangeon_keep
gameplay_field_keep
gameplay_keep
```

### Player Model (2 collisions)

Player character models - different content, same names:
- OoT: Adult Link / Child Link
- MM: Fierce Deity / Young Link (different from OoT child)

```
object_link_boy
object_link_child
```

## Detailed Object Collision List

Full list of all 151 colliding object names:

```
gameplay_dangeon_keep
gameplay_field_keep
gameplay_keep
object_ahg
object_am
object_ani
object_aob
object_b_heart
object_bba
object_bdoor
object_bg
object_bigokuta
object_bji
object_bob
object_boj
object_bombf
object_bombiwa
object_box
object_bubble
object_cne
object_cow
object_crow
object_cs
object_d_hsblock
object_d_lift
object_daiku
object_dekubaba
object_dekunuts
object_dnk
object_dns
object_dodongo
object_dog
object_ds2
object_dy_obj
object_efc_star_field
object_efc_tw
object_firefly
object_fish
object_fr
object_fu
object_fz
object_ge1
object_geldb
object_gi_arrow
object_gi_arrowcase
object_gi_bean
object_gi_bomb_1
object_gi_bomb_2
object_gi_bombpouch
object_gi_bosskey
object_gi_bottle
object_gi_bow
object_gi_compass
object_gi_fish
object_gi_ghost
object_gi_glasses
object_gi_golonmask
object_gi_heart
object_gi_hearts
object_gi_hookshot
object_gi_insect
object_gi_key
object_gi_ki_tan_mask
object_gi_liquid
object_gi_longsword
object_gi_m_arrow
object_gi_magicpot
object_gi_map
object_gi_melody
object_gi_milk
object_gi_nuts
object_gi_ocarina
object_gi_purse
object_gi_rabit_mask
object_gi_rupy
object_gi_shield_2
object_gi_shield_3
object_gi_soldout
object_gi_soul
object_gi_stick
object_gi_sutaru
object_gi_sword_1
object_gi_truth_mask
object_gi_zoramask
object_gla
object_gm
object_goroiwa
object_gs
object_hata
object_hintnuts
object_horse_link_child
object_hs
object_ik
object_in
object_js
object_ka
object_kanban
object_kibako2
object_kusa
object_kz
object_lightswitch
object_link_boy
object_link_child
object_ma1
object_ma2
object_mag
object_mamenoki
object_mastergolon
object_masterzoora
object_mir_ray
object_mk
object_mm
object_ms
object_mu
object_nb
object_niw
object_nwc
object_ny
object_oF1d_map
object_okuta
object_os_anime
object_owl
object_po_composer
object_po_sisters
object_ps
object_rd
object_rr
object_ru2
object_sb
object_skb
object_spot11_obj
object_ssh
object_st
object_stream
object_syokudai
object_tite
object_tk
object_toryo
object_trap
object_tsubo
object_umajump
object_vm
object_wallmaster
object_warp1
object_wf
object_wood02
object_yabusame_point
object_zg
object_zl1
object_zl4
object_zo
```

## Detailed Overlay Collision List

```
ovl_Arrow_Fire
ovl_Arrow_Ice
ovl_Arrow_Light
ovl_En_Clear_Tag
ovl_En_Holl
ovl_En_Kanban
ovl_En_Sda
ovl_En_Sth
ovl_Oceff_Spot
ovl_Oceff_Storm
ovl_Oceff_Wipe
ovl_Oceff_Wipe2
ovl_Oceff_Wipe3
ovl_Oceff_Wipe4
```

## Impact Analysis

### Why This Matters

When both oot.o2r and mm.o2r archives are loaded into libultraship's ArchiveManager:

1. **Resource path conflicts**: Both games use paths like `objects/object_link_boy/...`
2. **Undefined behavior**: Last-loaded archive wins, corrupting game assets
3. **Game crashes**: Wrong textures, models, or code loaded for game context

### Critical Collisions

The most dangerous collisions are:

| Resource | OoT Usage | MM Usage |
|----------|-----------|----------|
| `object_link_boy` | Adult Link | Fierce Deity Link |
| `object_link_child` | Child Link | Young Link (different) |
| `gameplay_keep` | Core OoT gameplay | Core MM gameplay |
| `gameplay_field_keep` | Overworld assets | Termina assets |
| `gameplay_dangeon_keep` | Dungeon assets | MM dungeon assets |

## Mitigation Strategies

### Option 1: Game-Prefixed Namespaces (Recommended)

Modify archive generation to prefix all paths with game identifier:

```
# Current (collides):
objects/object_link_boy/gLinkAdultSkel
objects/object_link_boy/gLinkFierceDeityWaistDL

# Prefixed (safe):
oot/objects/object_link_boy/gLinkAdultSkel
mm/objects/object_link_boy/gLinkFierceDeityWaistDL
```

**Pros**: Clean separation, single archive loading possible
**Cons**: Requires modifying extraction tools and all resource path references

### Option 2: Separate Archive Loading (Current Approach)

Keep archives separate and load only the active game's archives:

```cpp
void EnsureGameArchivesLoaded(GameId game) {
    // Unload other game's archives
    // Load this game's archives
}
```

**Pros**: No extraction changes needed
**Cons**: Cannot preload both games, hot-swap requires archive unloading

### Option 3: Archive Priority System

Extend ArchiveManager to support game-context lookups:

```cpp
// Archive tagged with game context
archiveManager->AddArchive(path, GameId::OOT);
archiveManager->AddArchive(path, GameId::MM);

// Lookup with context
archiveManager->LoadResource(path, currentGame);
```

**Pros**: Both archives can coexist, context-aware loading
**Cons**: Requires libultraship changes

## Recommendation

For the RedShipBlueShip unified executable:

1. **Short-term**: Use Option 2 (separate loading) - already implemented
2. **Medium-term**: Implement Option 3 (archive priority) in libultraship
3. **Long-term**: Consider Option 1 (prefixed namespaces) for clean architecture

The current implementation handles this via `EnsureGameArchivesLoaded()` in `rsbs/src/main.cpp`.

## Files Generated

- `resource_collisions.txt` - Full collision list
- `docs/resource-namespace-audit.md` - This report

## See Also

- `docs/asset-collision-analysis.md` - Previous collision analysis
- `symbol_collisions.txt` - C/C++ symbol collisions
- `rsbs/src/main.cpp:36-76` - Archive loading implementation
