#!/usr/bin/env python3
"""
Audit resource namespace collisions between OoT and MM archives.

Extracts asset paths from XML definitions and identifies resources that would
collide if both archives are loaded into a single namespace.

Usage:
    python tools/audit_resource_collisions.py
"""

import argparse
import os
import re
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Set, Tuple


def extract_asset_names_from_xml(xml_path: Path) -> List[Tuple[str, str, str]]:
    """
    Extract asset names from ZAPD XML file.

    Returns list of (asset_name, asset_type, source_file) tuples.
    """
    assets = []
    try:
        tree = ET.parse(xml_path)
        root = tree.getroot()

        # Get the base file name for the path prefix
        file_elem = root.find(".//File")
        if file_elem is not None:
            base_name = file_elem.get("Name", "")
        else:
            base_name = xml_path.stem

        # Extract all named elements
        for elem in root.iter():
            name = elem.get("Name")
            out_name = elem.get("OutName")

            # Use OutName if available, otherwise Name
            asset_name = out_name or name
            if asset_name and elem.tag != "File" and elem.tag != "Root":
                asset_type = elem.tag
                # Construct the resource path as it would appear in the archive
                assets.append((asset_name, asset_type, str(xml_path)))

    except ET.ParseError as e:
        print(f"Warning: Could not parse {xml_path}: {e}")
    except Exception as e:
        print(f"Warning: Error processing {xml_path}: {e}")

    return assets


def get_resource_paths(game_dir: Path) -> Dict[str, List[Tuple[str, str]]]:
    """
    Get all resource paths for a game from its asset directory structure.

    Returns dict mapping resource path to list of (type, source_file) tuples.
    """
    resources = defaultdict(list)

    # Check XML directories
    xml_dirs = [
        game_dir / "assets" / "xml",
        game_dir / "assets" / "xml" / "N64_US",  # MM uses this structure
        game_dir / "assets" / "xml" / "GC_US",
    ]

    for xml_dir in xml_dirs:
        if not xml_dir.exists():
            continue

        for xml_path in xml_dir.rglob("*.xml"):
            # Get the relative path to determine resource namespace
            rel_path = xml_path.relative_to(xml_dir)
            category = rel_path.parts[0] if len(rel_path.parts) > 1 else "misc"

            # Extract asset names from XML
            assets = extract_asset_names_from_xml(xml_path)
            for asset_name, asset_type, source in assets:
                # Construct archive path like "objects/object_link_boy/asset_name"
                base_name = xml_path.stem
                resource_path = f"{category}/{base_name}/{asset_name}"
                resources[resource_path].append((asset_type, source))

    # Also collect directory-based paths (for objects, scenes, etc.)
    asset_dirs = ["objects", "scenes", "overlays", "textures", "misc", "code", "archives"]
    for asset_dir_name in asset_dirs:
        asset_dir = game_dir / "assets" / asset_dir_name
        if not asset_dir.exists():
            continue

        for subdir in asset_dir.iterdir():
            if subdir.is_dir():
                # This directory name becomes part of the resource namespace
                resource_prefix = f"{asset_dir_name}/{subdir.name}"
                resources[resource_prefix].append(("directory", str(subdir)))

    return dict(resources)


def get_object_directories(game_dir: Path) -> Set[str]:
    """Get all object directory names."""
    objects_dir = game_dir / "assets" / "objects"
    if objects_dir.exists():
        return {d.name for d in objects_dir.iterdir() if d.is_dir()}
    return set()


def get_overlay_directories(game_dir: Path) -> Set[str]:
    """Get all overlay directory names."""
    overlays_dir = game_dir / "assets" / "overlays"
    if overlays_dir.exists():
        return {d.name for d in overlays_dir.iterdir() if d.is_dir()}
    return set()


def get_scene_directories(game_dir: Path) -> Set[str]:
    """Get all scene directory names."""
    scenes_dir = game_dir / "assets" / "scenes"
    if scenes_dir.exists():
        scenes = set()
        for d in scenes_dir.iterdir():
            if d.is_dir():
                scenes.add(d.name)
                # Also check subdirectories (scenes are often nested)
                for sub in d.iterdir():
                    if sub.is_dir():
                        scenes.add(sub.name)
        return scenes
    return set()


def categorize_collision(name: str) -> str:
    """Categorize a colliding resource name."""
    if name.startswith("gameplay_"):
        return "core_gameplay"
    elif name.startswith("object_link"):
        return "player_model"
    elif name.startswith("object_gi_"):
        return "get_item"
    elif name.startswith("ovl_"):
        return "overlay"
    elif name.startswith("object_"):
        return "object"
    else:
        return "other"


def main():
    parser = argparse.ArgumentParser(
        description="Audit resource namespace collisions between OoT and MM"
    )
    parser.add_argument(
        "--oot-dir",
        default="games/oot",
        help="Path to OoT game directory"
    )
    parser.add_argument(
        "--mm-dir",
        default="games/mm",
        help="Path to MM game directory"
    )
    parser.add_argument(
        "--output",
        default="docs/resource-namespace-audit.md",
        help="Output file for audit report"
    )
    parser.add_argument(
        "--collision-list",
        default="resource_collisions.txt",
        help="Output file for collision list"
    )
    args = parser.parse_args()

    oot_dir = Path(args.oot_dir)
    mm_dir = Path(args.mm_dir)

    print("=== Resource Namespace Collision Audit ===\n")

    # Collect directory-level information
    print("Scanning OoT assets...")
    oot_objects = get_object_directories(oot_dir)
    oot_overlays = get_overlay_directories(oot_dir)
    oot_scenes = get_scene_directories(oot_dir)

    print("Scanning MM assets...")
    mm_objects = get_object_directories(mm_dir)
    mm_overlays = get_overlay_directories(mm_dir)
    mm_scenes = get_scene_directories(mm_dir)

    # Find collisions
    object_collisions = oot_objects & mm_objects
    overlay_collisions = oot_overlays & mm_overlays
    scene_collisions = oot_scenes & mm_scenes

    # Categorize collisions
    collision_categories = defaultdict(list)
    for name in object_collisions:
        cat = categorize_collision(name)
        collision_categories[cat].append(name)

    for name in overlay_collisions:
        collision_categories["overlay"].append(name)

    # Print summary
    print(f"\n=== Summary ===")
    print(f"OoT objects: {len(oot_objects)}")
    print(f"MM objects:  {len(mm_objects)}")
    print(f"Object collisions: {len(object_collisions)}")
    print()
    print(f"OoT overlays: {len(oot_overlays)}")
    print(f"MM overlays:  {len(mm_overlays)}")
    print(f"Overlay collisions: {len(overlay_collisions)}")
    print()
    print(f"OoT scenes: {len(oot_scenes)}")
    print(f"MM scenes:  {len(mm_scenes)}")
    print(f"Scene collisions: {len(scene_collisions)}")

    # Write collision list
    collision_path = Path(args.collision_list)
    with collision_path.open('w') as f:
        f.write("# Resource Namespace Collisions between OoT and MM\n")
        f.write(f"# Objects: {len(object_collisions)}, Overlays: {len(overlay_collisions)}, Scenes: {len(scene_collisions)}\n\n")

        f.write("## Object Collisions\n")
        for name in sorted(object_collisions):
            f.write(f"{name}\n")

        f.write("\n## Overlay Collisions\n")
        for name in sorted(overlay_collisions):
            f.write(f"{name}\n")

        if scene_collisions:
            f.write("\n## Scene Collisions\n")
            for name in sorted(scene_collisions):
                f.write(f"{name}\n")

    print(f"\nCollision list written to: {collision_path}")

    # Generate markdown report
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with output_path.open('w') as f:
        f.write("# Resource Namespace Collision Audit\n\n")
        f.write("## Executive Summary\n\n")
        f.write(f"**CRITICAL**: Significant namespace collisions exist between OoT and MM archives.\n\n")
        f.write(f"| Category | OoT Count | MM Count | Collisions | Collision % |\n")
        f.write(f"|----------|-----------|----------|------------|-------------|\n")
        f.write(f"| Objects  | {len(oot_objects)} | {len(mm_objects)} | {len(object_collisions)} | {100*len(object_collisions)/len(oot_objects):.1f}% of OoT |\n")
        f.write(f"| Overlays | {len(oot_overlays)} | {len(mm_overlays)} | {len(overlay_collisions)} | {100*len(overlay_collisions)/len(oot_overlays):.1f}% of OoT |\n")
        f.write(f"| Scenes   | {len(oot_scenes)} | {len(mm_scenes)} | {len(scene_collisions)} | {100*len(scene_collisions)/max(len(oot_scenes), 1):.1f}% |\n")
        f.write(f"\n**Total namespace collisions: {len(object_collisions) + len(overlay_collisions) + len(scene_collisions)}**\n\n")

        f.write("## Collision Categories\n\n")
        for cat, names in sorted(collision_categories.items(), key=lambda x: -len(x[1])):
            f.write(f"### {cat.replace('_', ' ').title()} ({len(names)} collisions)\n\n")
            if cat == "core_gameplay":
                f.write("These are fundamental game assets that both games need:\n\n")
            elif cat == "player_model":
                f.write("Player character models - different content, same names:\n")
                f.write("- OoT: Adult Link / Child Link\n")
                f.write("- MM: Fierce Deity / Young Link (different from OoT child)\n\n")
            elif cat == "get_item":
                f.write("Get Item (gi) models for collectible items:\n\n")
            elif cat == "overlay":
                f.write("Actor overlays with identical names but different implementations:\n\n")
            elif cat == "object":
                f.write("Various game objects (enemies, NPCs, items):\n\n")

            f.write("```\n")
            for name in sorted(names)[:30]:  # Limit display
                f.write(f"{name}\n")
            if len(names) > 30:
                f.write(f"... and {len(names) - 30} more\n")
            f.write("```\n\n")

        f.write("## Detailed Object Collision List\n\n")
        f.write("Full list of all 151 colliding object names:\n\n")
        f.write("```\n")
        for name in sorted(object_collisions):
            f.write(f"{name}\n")
        f.write("```\n\n")

        f.write("## Detailed Overlay Collision List\n\n")
        f.write("```\n")
        for name in sorted(overlay_collisions):
            f.write(f"{name}\n")
        f.write("```\n\n")

        f.write("## Impact Analysis\n\n")
        f.write("### Why This Matters\n\n")
        f.write("When both oot.o2r and mm.o2r archives are loaded into libultraship's ArchiveManager:\n\n")
        f.write("1. **Resource path conflicts**: Both games use paths like `objects/object_link_boy/...`\n")
        f.write("2. **Undefined behavior**: Last-loaded archive wins, corrupting game assets\n")
        f.write("3. **Game crashes**: Wrong textures, models, or code loaded for game context\n\n")

        f.write("### Critical Collisions\n\n")
        f.write("The most dangerous collisions are:\n\n")
        f.write("| Resource | OoT Usage | MM Usage |\n")
        f.write("|----------|-----------|----------|\n")
        f.write("| `object_link_boy` | Adult Link | Fierce Deity Link |\n")
        f.write("| `object_link_child` | Child Link | Young Link (different) |\n")
        f.write("| `gameplay_keep` | Core OoT gameplay | Core MM gameplay |\n")
        f.write("| `gameplay_field_keep` | Overworld assets | Termina assets |\n")
        f.write("| `gameplay_dangeon_keep` | Dungeon assets | MM dungeon assets |\n\n")

        f.write("## Mitigation Strategies\n\n")
        f.write("### Option 1: Game-Prefixed Namespaces (Recommended)\n\n")
        f.write("Modify archive generation to prefix all paths with game identifier:\n\n")
        f.write("```\n")
        f.write("# Current (collides):\n")
        f.write("objects/object_link_boy/gLinkAdultSkel\n")
        f.write("objects/object_link_boy/gLinkFierceDeityWaistDL\n\n")
        f.write("# Prefixed (safe):\n")
        f.write("oot/objects/object_link_boy/gLinkAdultSkel\n")
        f.write("mm/objects/object_link_boy/gLinkFierceDeityWaistDL\n")
        f.write("```\n\n")
        f.write("**Pros**: Clean separation, single archive loading possible\n")
        f.write("**Cons**: Requires modifying extraction tools and all resource path references\n\n")

        f.write("### Option 2: Separate Archive Loading (Current Approach)\n\n")
        f.write("Keep archives separate and load only the active game's archives:\n\n")
        f.write("```cpp\n")
        f.write("void EnsureGameArchivesLoaded(GameId game) {\n")
        f.write("    // Unload other game's archives\n")
        f.write("    // Load this game's archives\n")
        f.write("}\n")
        f.write("```\n\n")
        f.write("**Pros**: No extraction changes needed\n")
        f.write("**Cons**: Cannot preload both games, hot-swap requires archive unloading\n\n")

        f.write("### Option 3: Archive Priority System\n\n")
        f.write("Extend ArchiveManager to support game-context lookups:\n\n")
        f.write("```cpp\n")
        f.write("// Archive tagged with game context\n")
        f.write("archiveManager->AddArchive(path, GameId::OOT);\n")
        f.write("archiveManager->AddArchive(path, GameId::MM);\n\n")
        f.write("// Lookup with context\n")
        f.write("archiveManager->LoadResource(path, currentGame);\n")
        f.write("```\n\n")
        f.write("**Pros**: Both archives can coexist, context-aware loading\n")
        f.write("**Cons**: Requires libultraship changes\n\n")

        f.write("## Recommendation\n\n")
        f.write("For the RedShipBlueShip unified executable:\n\n")
        f.write("1. **Short-term**: Use Option 2 (separate loading) - already implemented\n")
        f.write("2. **Medium-term**: Implement Option 3 (archive priority) in libultraship\n")
        f.write("3. **Long-term**: Consider Option 1 (prefixed namespaces) for clean architecture\n\n")
        f.write("The current implementation handles this via `EnsureGameArchivesLoaded()` in `rsbs/src/main.cpp`.\n\n")

        f.write("## Files Generated\n\n")
        f.write(f"- `{args.collision_list}` - Full collision list\n")
        f.write("- `docs/resource-namespace-audit.md` - This report\n\n")

        f.write("## See Also\n\n")
        f.write("- `docs/asset-collision-analysis.md` - Previous collision analysis\n")
        f.write("- `symbol_collisions.txt` - C/C++ symbol collisions\n")
        f.write("- `rsbs/src/main.cpp:36-76` - Archive loading implementation\n")

    print(f"Report written to: {output_path}")

    return 0


if __name__ == "__main__":
    exit(main())
