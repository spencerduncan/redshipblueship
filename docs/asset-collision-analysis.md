# Asset Path Collision Analysis

## Summary

**COLLISIONS EXIST** - OoT and MM share 151 object names (40% of OoT's objects). Direct dual-archive loading will cause conflicts.

## Findings

### Object Collisions

| Game | Total Objects | Shared Objects |
|------|---------------|----------------|
| OoT  | 381           | 151 (40%)      |
| MM   | 464           | 151 (33%)      |

Asset paths follow pattern: `objects/{object_name}/{asset_name}`

With 151 shared object folders, loading both archives into a single namespace would result in undefined behavior when requesting assets from colliding paths.

### Sample Collisions

Critical shared objects include:
- `object_link_boy` / `object_link_child` - Player models
- `gameplay_keep` / `gameplay_field_keep` / `gameplay_dangeon_keep` - Core gameplay assets
- Common actors: `object_dekubaba`, `object_dodongo`, `object_bubble`, etc.

### Other Asset Categories

| Category | OoT | MM | Collisions |
|----------|-----|-----|------------|
| Scenes   | 5 dirs | 102 dirs | Not checked (different ID spaces) |
| Textures | 24 | 0 | 0 |
| Overlays | Many | Many | Likely high |

## Recommendation

**Do NOT merge into single namespace.** Use game-specific archive loading:

```cpp
namespace Combo {
    enum class Game { OOT, MM };

    // Load from specific game's archive
    std::shared_ptr<IResource> LoadGameResource(Game game, const std::string& path);
}
```

libultraship's ArchiveManager supports multiple archives. The solution is:
1. Load both `oot.otr` and `mm.otr` as separate archives
2. Track which archive belongs to which game
3. Provide game-specific loading API that targets the correct archive

This keeps both games' assets intact without renaming or prefixing.

## Files

Full collision list: `/tmp/object_collisions.txt` (generated during analysis)
