# Lane M3 (wave 5) — #705: loose (unpacked) asset mods for both games

**Branch:** `claude/705-loose-asset-mods`

**Task:** Verify the premise first: nothing in the tree mounts a directory
as an archive today. Then make a loose directory under each game's mods
partition (PR #704/#716: OoT owns the `mods/` root, MM owns `mods/mm/`, one
shared walk, the root-collapse gate) mount as an archive with the same
override precedence as a packed mod, for BOTH games, respecting the
partition. Document the layout in `docs/MODDING.md`. Lock with a
redship-tier row that drops a loose file under each partition and observes
the override applied and not crossing games. If libultraship lacks a
directory-archive primitive, STOP and report (that would be a fork PR under
a different convention). Golden rows green, golden files untouched. Builds;
both tiers.

**Files owned:** the mods mount path in both games, `src/common` mount
helpers, `docs/MODDING.md`, the new row, the shared append-only files.

**Keywords:** `Fixes #705`, `Refs #670`.
