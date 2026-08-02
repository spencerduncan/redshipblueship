#!/usr/bin/env python3
"""Build redship.o2r -- the curated cross-game asset archive (#577).

RedShipBlueShip draws models by RESOURCE PATH, not by object-bank slot: the
Fast3D interpreter takes a `__OTR__objects/<obj>/<sym>` string straight out of
the display-list command word and hands it to
ResourceManager::GetResourceRawPointer (libultraship/src/fast/interpreter.cpp,
gfx_dl_otr_filepath_handler_custom).  Nothing in that path consults an object
id, a bank slot, a segment or a DMA table.  So the only thing standing between
OoT and an MM model is whether the MM path is MOUNTED when OoT is running --
and mm.o2r is not mounted until the player has actually entered MM.

This script carves the curated subset out of the already-extracted game
archives so it can be mounted unconditionally, at every boot, for both games.

Design constraints, in the order they matter:

  1. PATHS ARE COPIED VERBATIM.  An extracted display list refers to its own
     textures and vertices by CRC64-of-path, baked into the command stream at
     export time (OTRExporter/OTRExporter/DisplayListExporter.cpp, the
     G_SETTIMG_OTR_HASH branch).  Renaming a resource on the way in would
     break every reference to it and there is no re-hashing pass we could run
     without re-exporting.  The curated archive therefore mirrors the source
     archive's namespace exactly.

  2. ONLY COLLISION-FREE PATHS MAY BE CURATED.  Because paths are verbatim and
     ArchiveManager resolution is last-added-wins over one flat CRC64 map
     (ArchiveManager::AddArchive), curating a path that the OTHER game also
     ships would make which bytes you get depend on mount order.  Every entry
     is checked against the other game's archive and the build FAILS on a
     collision rather than shipping an order-dependent archive.  Byte-identical
     duplicates are also rejected: identical today is not a contract.

  3. NO `version` ENTRY.  Archive::Load only version-gates an archive that
     carries one, and a curated archive is not a game dump; adding a `version`
     would make it fail ArchiveManager's valid-game-version check under
     whichever game's Init ran first.  soh.o2r and 2ship.o2r are version-less
     for the same reason.

The manifest is a text file of `<game> <path-prefix>` lines; `#` comments and
blank lines are ignored.  A prefix ending in `/` selects a whole object
directory.
"""

import argparse
import os
import sys
import zipfile

GAMES = ("oot", "mm")


def read_manifest(path):
    entries = []
    with open(path, "r", encoding="utf-8") as handle:
        for lineno, raw in enumerate(handle, 1):
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            parts = line.split(None, 1)
            if len(parts) != 2:
                sys.exit("%s:%d: expected '<game> <path-prefix>', got %r" % (path, lineno, raw.rstrip()))
            game, prefix = parts[0].lower(), parts[1].strip()
            if game not in GAMES:
                sys.exit("%s:%d: unknown game %r (expected one of %s)" % (path, lineno, game, ", ".join(GAMES)))
            entries.append((game, prefix, lineno))
    if not entries:
        sys.exit("%s: manifest is empty -- refusing to build an empty curated archive" % path)
    return entries


def main():
    ap = argparse.ArgumentParser(description="Build the curated cross-game asset archive (redship.o2r)")
    ap.add_argument("--oot-archive", required=True, help="path to the extracted oot.o2r")
    ap.add_argument("--mm-archive", required=True, help="path to the extracted mm.o2r")
    ap.add_argument("--manifest", required=True, help="path to the curated-asset manifest")
    ap.add_argument("--out", required=True, help="path of the redship.o2r to write")
    args = ap.parse_args()

    sources = {"oot": args.oot_archive, "mm": args.mm_archive}
    for game, path in sources.items():
        if not os.path.isfile(path):
            # Not an error: a tree that only ever extracted one game still
            # builds.  It becomes an error below if the manifest needs it.
            print("[redship.o2r] note: %s archive not present at %s" % (game, path))

    zips = {}
    for game, path in sources.items():
        if os.path.isfile(path):
            zips[game] = zipfile.ZipFile(path, "r")

    names = {game: set(z.namelist()) for game, z in zips.items()}

    entries = read_manifest(args.manifest)
    selected = []  # (game, path)
    for game, prefix, lineno in entries:
        if game not in zips:
            sys.exit("%s:%d: manifest needs the %s archive, which is not present at %s"
                     % (args.manifest, lineno, game, sources[game]))
        matches = sorted(n for n in names[game] if n.startswith(prefix))
        if not matches:
            sys.exit("%s:%d: prefix %r matched nothing in %s" % (args.manifest, lineno, prefix, sources[game]))
        selected.extend((game, n) for n in matches)

    # Collision guard (constraint 2 above).
    other = {"oot": "mm", "mm": "oot"}
    collisions = []
    for game, path in selected:
        peer = other[game]
        if peer in names and path in names[peer]:
            collisions.append((game, path))
    if collisions:
        for game, path in collisions:
            print("[redship.o2r] COLLISION: %s-owned %r also exists in the %s archive"
                  % (game, path, other[game]), file=sys.stderr)
        sys.exit("[redship.o2r] refusing to build: %d curated path(s) collide with the other game's archive; "
                 "curated paths are copied verbatim, so a collision makes the resolved bytes depend on mount "
                 "order (ArchiveManager is last-added-wins over one flat CRC64 map)." % len(collisions))

    out_dir = os.path.dirname(os.path.abspath(args.out))
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    if os.path.exists(args.out):
        os.remove(args.out)

    total = 0
    with zipfile.ZipFile(args.out, "w", zipfile.ZIP_DEFLATED) as out:
        for game, path in selected:
            data = zips[game].read(path)
            out.writestr(path, data)
            total += len(data)

    for z in zips.values():
        z.close()

    print("[redship.o2r] wrote %s: %d curated resource(s), %d bytes uncompressed"
          % (args.out, len(selected), total))
    return 0


if __name__ == "__main__":
    sys.exit(main())
