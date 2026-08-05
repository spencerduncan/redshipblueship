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

This script carves that curated subset out of the already-extracted game
archives, into an archive that can be mounted alongside either game's own.
Mounting it is a separate decision and is NOT wired into the runtime yet: the
archive is registered as neither game's, and both RsbsMMArchiveFactoryDispatcher
and ResourceMgr_ListFilesForGame classify resources by owning archive (see
constraint 4).  Today the consumer is the `crossgame-model` CTest row, which
mounts it in-process through the same ArchiveManager::AddArchive call a runtime
mount would use.

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

  4. NO ARCHIVE-DISPATCHED RESOURCE TYPES.  Three loader slots -- Room/scene
     ('OROM'), Cutscene ('OCUT') and Path ('OPTH') -- are claimed by BOTH games
     with incompatible wire formats, so single-exe builds resolve them with
     RsbsMMArchiveFactoryDispatcher (games/mm/2s2h/GameExports_SingleExe.cpp),
     which picks the parser by asking whether the OWNING ARCHIVE is one
     LoadMMArchives() registered.  redship.o2r joins neither game's registry,
     so an MM scene/cutscene/path served from it would be handed to OoT's
     reader -- the exact arrangement that read past the end of the buffer and
     killed the process on std::out_of_range before 2026-07-18 (see the comment
     at GameExports_SingleExe.cpp's Path registration).  The same
     archive-membership assumption backs ResourceMgr_ListFilesForGame
     (games/oot/soh/ResourceManagerHelpers.cpp), which would file anything here
     under OoT.  Models are safe because no model resource type is dispatched;
     these three are refused at generation time so the manifest cannot quietly
     grow into that failure.

  5. NO RAW SEGMENTED TEXTURE REFERENCES (#605).  OTRExporter emits a texture
     reference in exactly one of two forms (DisplayListExporter.cpp, the
     G_SETTIMG case): a resource-path form (G_SETTIMG_OTR_HASH) when the source
     segment was registered at export time, with the host game irrelevant to
     resolving it; and a RAW gsDPSetTextureImage carrying the segmented address
     verbatim when it was not.  The raw form resolves against the HOST game's
     segment table at draw time -- silently wrong textures cross-game, the one
     honest residual risk the #577 spike identified (the hazard OoTMM needed
     ~107 hand-authored kObjectPatches[] entries for).  This is the same walk
     the `crossgame-model` CTest row (src/common/tests/test_crossgame_model.c)
     performs at load time, relocated here so a bad model is refused at BUILD
     time with a named reason instead of shipping and rendering wrong.  Unlike
     that row, this walk does not follow sub-display-list edges into other
     resources: every curated resource is scanned independently, so a raw
     reference inside a sub-display-list is caught when THAT resource is itself
     scanned (manifest entries are whole-directory prefixes), and a sub-DL
     reference that escapes the curated set entirely is already refused by the
     unresolved/OoT-served checks the CTest row performs at load time.

  6. ONLY ARRAY RESOURCES BOTH READERS PARSE IDENTICALLY (#604).  Every
     extracted object's vertex data (`*Vtx_*`) is an 'OARR' Array resource, and
     the Array factory is GAME-OWNED -- OoT registers
     SOH::ResourceFactoryBinaryArrayV0 (games/oot/soh/resource/importer/
     ArrayFactory.cpp) and MM registers S2H::ResourceFactoryBinaryArrayV0
     (games/mm/2s2h/resource/importer/ArrayFactory.cpp).  redship.o2r belongs to
     neither game's registry, so whichever game is RUNNING parses the curated
     array -- OoT's factory parses MM's vertices, and vice versa.

     That works today only by coincidence of layout, and only on part of the
     format.  The two factories are byte-identical on the VERTEX path (same
     16-byte F3DVtx, same field order, same read order), which is why the
     shipped model works.  They diverge on the SCALAR path: MM implements
     S8/U8/X8/S16/U16/X16/S32/U32/X32/S64/U64/X64, OoT implements only
     S16/U16 and falls through `default: break` on the rest -- reading ZERO
     bytes where MM reads one to eight.  A curated array carrying any of those
     widths does not merely produce a wrong value; it DESYNCS the reader, so
     every element after it is garbage.  This is not hypothetical: MM's
     `objects/object_link_zora/object_link_zora_U8_011710` is a ZSCALAR_X8
     scalar array that clears constraints 2, 4 and 5 and would be admitted.

     So rather than assume agreement, this walks each curated Array resource
     and simulates BOTH factories' byte consumption element by element,
     refusing the first element where the two disagree.  Vertex arrays are
     admitted because the two read loops are the same code; anything else is
     admitted only where both readers consume the same width.  This is the
     contained half of #604 -- it cannot lock the VERTEX path against a future
     upstream change to either port's reader, which is a code-equality property
     no archive walk can see.  See the #604 discussion for that residual.

The manifest is a text file of `<game> <path-prefix>` lines; `#` comments and
blank lines are ignored.  A prefix ending in `/` selects a whole object
directory.
"""

import argparse
import os
import sys
import zipfile

GAMES = ("oot", "mm")

# Resource types whose loader slot is resolved per-owning-archive in single-exe
# builds (constraint 4 in the module docstring).  Tags as they appear in the
# 64-byte binary OTR header's Type field, which ResourceLoader reads as a
# uint32 at offset 4 with the endianness named by byte 0
# (ResourceLoader::ReadResourceInitDataBinary).  Matched in both byte orders so
# the guard never depends on getting that right.
DISPATCHED_TYPES = {
    b"OROM": "Room/scene",
    b"OCUT": "Cutscene",
    b"OPTH": "Path",
}

OTR_HEADER_TYPE_OFFSET = 4


def dispatched_type_of(data):
    """Return the human name of the archive-dispatched type of `data`, or None.

    A curated entry shorter than the type field cannot be a resource at all, so
    it is simply not one of these.
    """
    if len(data) < OTR_HEADER_TYPE_OFFSET + 4:
        return None
    tag = data[OTR_HEADER_TYPE_OFFSET:OTR_HEADER_TYPE_OFFSET + 4]
    return DISPATCHED_TYPES.get(tag) or DISPATCHED_TYPES.get(tag[::-1])


# ------------------------------------------------------------------------
# Raw-segmented-texture guard (constraint 5, #605).
#
# Mirrors ResourceFactoryBinaryDisplayListV0::ReadResource
# (libultraship/src/fast/resource/factory/DisplayListFactory.cpp) and the
# raw-G_SETTIMG check in src/common/tests/test_crossgame_model.c closely
# enough to walk the SAME resource bytes those read at load time, without
# needing a running ResourceManager.
# ------------------------------------------------------------------------

OTR_HEADER_SIZE = 64  # libultraship/include/ship/resource/archive/Archive.h

# Fast::ResourceType::DisplayList (libultraship/include/fast/resource/ResourceType.h).
DISPLAY_LIST_TYPE = b"ODLT"

# Opcodes from libultraship/include/libultraship/libultra/gbi.h. These are the
# exact values the DisplayList factory and the CrossGameModel CTest row use to
# tell an OTR-hash (expanded, 128-bit) command from a plain one.
_OTR_SETTIMG_HASH = 0x20
_OTR_DL_HASH = 0x31
_OTR_VTX_HASH = 0x32
_OTR_MARKER = 0x33
_OTR_BRANCH_Z = 0x35
_OTR_MTX = 0x36
_OTR_MOVEMEM = 0x42
_EXPANDED_OPCODES = frozenset(
    {_OTR_SETTIMG_HASH, _OTR_DL_HASH, _OTR_VTX_HASH, _OTR_MARKER, _OTR_BRANCH_Z, _OTR_MTX, _OTR_MOVEMEM})

# RDP G_SETTIMG (0xfd) -- the RAW (non-OTR-hash) form. This is the hazard:
# OTRExporter only emits it when it could not turn the referenced segment into
# a resource path (DisplayListExporter.cpp, case G_SETTIMG, the
# !HasSegment(...) branch), so the operand is still a segmented address, which
# the interpreter resolves against whichever game's segment table is bound at
# draw time.
_RAW_SETTIMG = 0xFD

# UcodeHandlers enum order (libultraship/include/fast/ucodehandlers.h) mapped
# to each ucode family's G_ENDDL opcode (DisplayListFactory.cpp
# GetEndOpcodeByUCode). Every model OTRExporter emits uses UCODE_F3DEX2 (see
# OTRExporter/OTRExporter/DisplayListExporter.cpp), but the byte is read from
# the resource rather than assumed, exactly like the runtime factory does.
_F3DEX_ENDDL = 0xB8
_F3DEX2_ENDDL = 0xDF
_END_DL_BY_UCODE = {
    0: _F3DEX_ENDDL,  # ucode_f3db
    1: _F3DEX_ENDDL,  # ucode_f3d
    2: _F3DEX_ENDDL,  # ucode_f3dex
    3: _F3DEX_ENDDL,  # ucode_f3dexb
    4: _F3DEX2_ENDDL,  # ucode_f3dex2
    5: _F3DEX2_ENDDL,  # ucode_s2dex
}


class MalformedDisplayList(Exception):
    """A curated ODLT resource's command stream could not be parsed far enough
    to prove it carries no raw segmented texture reference. Treated as a
    refusal, not a silent pass -- a model this guard cannot verify is refused,
    not shipped."""


def is_display_list(data):
    """Return True if `data` is an ODLT (DisplayList) resource.

    Matched in both byte orders, like dispatched_type_of: OTRExporter always
    writes the per-resource header byte-order byte as Little (Exporter.cpp
    WriteHeader), so the big-endian-conceived 0x4F444C54 constant lands on
    disk as the reversed byte sequence.
    """
    if len(data) < OTR_HEADER_TYPE_OFFSET + 4:
        return False
    tag = data[OTR_HEADER_TYPE_OFFSET:OTR_HEADER_TYPE_OFFSET + 4]
    return tag == DISPLAY_LIST_TYPE or tag[::-1] == DISPLAY_LIST_TYPE


def find_raw_segmented_texture_refs(data):
    """Walk an ODLT resource's own command stream and return a list of
    (instruction_index, segment) for every raw segmented G_SETTIMG it
    contains directly (see constraint 5 in the module docstring for why this
    does not follow sub-display-list edges into other resources).

    Raises MalformedDisplayList if the stream cannot be parsed to its
    G_ENDDL.
    """
    if len(data) < OTR_HEADER_SIZE + 1:
        raise MalformedDisplayList("resource is smaller than the OTR header plus a 1-byte ucode field")

    byte_order = "little" if data[0] == 0 else "big"
    body = data[OTR_HEADER_SIZE:]
    ucode = body[0]
    end_opcode = _END_DL_BY_UCODE.get(ucode)
    if end_opcode is None:
        raise MalformedDisplayList("unrecognized ucode byte 0x%02X" % ucode)

    # ResourceFactoryBinaryDisplayListV0::ReadResource aligns to an 8-byte
    # boundary (relative to the start of the body) after the 1-byte ucode
    # field before reading the first command.
    pos = 1
    while pos % 8 != 0:
        pos += 1

    findings = []
    index = 0
    n = len(body)
    while True:
        if pos + 8 > n:
            raise MalformedDisplayList("command stream truncated before G_ENDDL (at body offset %d)" % pos)
        w0 = int.from_bytes(body[pos:pos + 4], byte_order)
        w1 = int.from_bytes(body[pos + 4:pos + 8], byte_order)
        pos += 8
        opcode = (w0 >> 24) & 0xFF

        if opcode == _RAW_SETTIMG:
            # Segment 0 is a plain physical address, not the shared-segment
            # hazard -- matches the runtime walk's
            # `operand != 0 && ((operand >> 24) & 0x0F) != 0`.
            segment = (w1 >> 24) & 0x0F
            if w1 != 0 and segment != 0:
                findings.append((index, segment))
            index += 1
            if opcode == end_opcode:
                break
            continue

        if opcode not in _EXPANDED_OPCODES:
            index += 1
            if opcode == end_opcode:
                break
            continue

        # Expanded (128-bit) command: skip the payload Gfx that follows.
        if pos + 8 > n:
            raise MalformedDisplayList("expanded command 0x%02X truncated before its payload (at body offset %d)" %
                                       (opcode, pos))
        pos += 8
        index += 2
        if opcode == end_opcode:
            break

    return findings


# ------------------------------------------------------------------------
# Array reader-agreement guard (constraint 6, #604).
#
# Simulates the byte consumption of BOTH games' Array factories over the same
# resource bytes:
#   games/oot/soh/resource/importer/ArrayFactory.cpp  (SOH::...ArrayV0)
#   games/mm/2s2h/resource/importer/ArrayFactory.cpp  (S2H::...ArrayV0)
# ------------------------------------------------------------------------

# Fast::ResourceType::Array -- 'OARR' in the OTR header's Type field.
ARRAY_TYPE = b"OARR"

# ArrayResourceType (games/{oot/soh,mm/2s2h}/resource/type/Array.h). The two
# enums are declared in the SAME order in both games, so the on-disk tag means
# the same thing to either reader; only these two members are needed here.
ARRAY_RESOURCE_TYPE_VECTOR = 24
ARRAY_RESOURCE_TYPE_VERTEX = 25

# ScalarType (same header, also identically ordered in both games).
SCALAR_TYPE_NAMES = {
    0: "ZSCALAR_NONE",
    1: "ZSCALAR_S8",
    2: "ZSCALAR_U8",
    3: "ZSCALAR_X8",
    4: "ZSCALAR_S16",
    5: "ZSCALAR_U16",
    6: "ZSCALAR_X16",
    7: "ZSCALAR_S32",
    8: "ZSCALAR_U32",
    9: "ZSCALAR_X32",
    10: "ZSCALAR_S64",
    11: "ZSCALAR_U64",
    12: "ZSCALAR_X64",
    13: "ZSCALAR_F32",
    14: "ZSCALAR_F64",
}

# Bytes each game's factory actually consumes per scalar. A type absent from a
# table is one that factory's switch does not handle: it hits `default: break`
# and reads NOTHING, which is exactly how the two desync.
OOT_SCALAR_WIDTHS = {4: 2, 5: 2}
MM_SCALAR_WIDTHS = {1: 1, 2: 1, 3: 1, 4: 2, 5: 2, 6: 2, 7: 4, 8: 4, 9: 4, 10: 8, 11: 8, 12: 8}

# F3DVtx: 3*s16 + u16 + 2*s16 + 4*u8 (libultraship/include/fast/lus_gbi.h).
VERTEX_ELEMENT_SIZE = 16


class MalformedArray(Exception):
    """A curated 'OARR' resource could not be walked far enough to prove both
    games' Array factories consume it identically. Treated as a refusal, not a
    silent pass -- same policy as MalformedDisplayList."""


def is_array(data):
    """Return True if `data` is an 'OARR' (Array) resource, in either byte
    order -- same convention as dispatched_type_of/is_display_list."""
    if len(data) < OTR_HEADER_TYPE_OFFSET + 4:
        return False
    tag = data[OTR_HEADER_TYPE_OFFSET:OTR_HEADER_TYPE_OFFSET + 4]
    return tag == ARRAY_TYPE or tag[::-1] == ARRAY_TYPE


def find_array_reader_disagreements(data):
    """Walk an 'OARR' resource and return a list of
    (element_index, scalar_type, oot_width, mm_width) for the point at which
    the two games' Array factories stop consuming the same bytes.

    At most one finding is returned: once the readers desync, every later
    element is read from a different offset by each factory, so comparing
    further would report noise rather than a second independent defect.

    Raises MalformedArray if the resource cannot be walked to its end.
    """
    if len(data) < OTR_HEADER_SIZE + 8:
        raise MalformedArray("resource is smaller than the OTR header plus the type/count fields")

    byte_order = "little" if data[0] == 0 else "big"
    body = data[OTR_HEADER_SIZE:]
    n = len(body)
    array_type = int.from_bytes(body[0:4], byte_order)
    count = int.from_bytes(body[4:8], byte_order)

    # A count larger than the payload could ever hold means the walk cannot be
    # trusted -- refuse rather than loop on a bogus length.
    if count > n:
        raise MalformedArray("element count %d exceeds the %d-byte payload" % (count, n))

    if array_type == ARRAY_RESOURCE_TYPE_VERTEX:
        # Both factories run the SAME ten reads per element here; there is no
        # width to disagree about. Only confirm the payload really holds them.
        need = 8 + count * VERTEX_ELEMENT_SIZE
        if need > n:
            raise MalformedArray("vertex array declares %d element(s) but the payload holds %d byte(s), not %d" %
                                 (count, n, need))
        return []

    pos = 8
    for index in range(count):
        if pos + 4 > n:
            raise MalformedArray("scalar element %d's type field runs past the payload (at body offset %d)" %
                                 (index, pos))
        scalar_type = int.from_bytes(body[pos:pos + 4], byte_order)
        pos += 4

        # Only ArrayResourceType::Vector carries a per-element repeat count,
        # and BOTH factories read it the same way before any scalar.
        repeat = 1
        if array_type == ARRAY_RESOURCE_TYPE_VECTOR:
            if pos + 4 > n:
                raise MalformedArray("vector element %d's repeat count runs past the payload (at body offset %d)" %
                                     (index, pos))
            repeat = int.from_bytes(body[pos:pos + 4], byte_order)
            pos += 4

        oot_width = OOT_SCALAR_WIDTHS.get(scalar_type, 0)
        mm_width = MM_SCALAR_WIDTHS.get(scalar_type, 0)
        if oot_width != mm_width:
            return [(index, scalar_type, oot_width, mm_width)]

        pos += repeat * mm_width
        if pos > n:
            raise MalformedArray("scalar element %d runs past the payload (needs body offset %d of %d)" %
                                 (index, pos, n))

    return []


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
    # Repeatable: the two extraction targets leave their archives in DIFFERENT
    # places and only one of them is normalised to the source root.
    # ExtractAssets runs copy-existing-otrs.cmake, which copies oot.o2r up to
    # ${CMAKE_SOURCE_DIR}; ExtractMMAssets copies only 2ship.o2r and leaves
    # mm.o2r in its WORKING_DIRECTORY, games/mm/.  (ExtractMMAssets declares
    # ${CMAKE_SOURCE_DIR}/mm.o2r in BYPRODUCTS, but nothing ever writes it —
    # that entry is wrong, which is easy to miss because a tree with archives
    # copied in by hand looks fine.)  Rather than encode one guess, take every
    # plausible location and use the first that exists.
    ap.add_argument("--oot-archive", required=True, action="append", metavar="PATH",
                    help="candidate path to the extracted oot.o2r; repeatable, first existing wins")
    ap.add_argument("--mm-archive", required=True, action="append", metavar="PATH",
                    help="candidate path to the extracted mm.o2r; repeatable, first existing wins")
    ap.add_argument("--manifest", required=True, help="path to the curated-asset manifest")
    ap.add_argument("--out", required=True, help="path of the redship.o2r to write")
    args = ap.parse_args()

    candidates = {"oot": args.oot_archive, "mm": args.mm_archive}
    sources = {}
    for game, paths in candidates.items():
        found = next((p for p in paths if os.path.isfile(p)), None)
        sources[game] = found if found is not None else paths[0]
        if found is None:
            # Not an error: a tree that only ever extracted one game still
            # builds.  It becomes an error below if the manifest needs it.
            print("[redship.o2r] note: no %s archive at any of: %s" % (game, ", ".join(paths)))
        else:
            print("[redship.o2r] %s archive: %s" % (game, found))

    zips = {}
    for game, path in sources.items():
        if os.path.isfile(path):
            zips[game] = zipfile.ZipFile(path, "r")

    names = {game: set(z.namelist()) for game, z in zips.items()}

    entries = read_manifest(args.manifest)
    selected = []  # (game, path)
    for game, prefix, lineno in entries:
        if game not in zips:
            sys.exit("%s:%d: manifest needs the %s archive, which is not present at any of: %s"
                     % (args.manifest, lineno, game, ", ".join(candidates[game])))
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

    # Archive-dispatched-type guard (constraint 4 above).  Read every curated
    # entry BEFORE the output archive is created, so a refusal never leaves a
    # half-written redship.o2r behind for a later build step to copy.
    payloads = {}
    dispatched = []
    for game, path in selected:
        data = zips[game].read(path)
        payloads[(game, path)] = data
        kind = dispatched_type_of(data)
        if kind is not None:
            dispatched.append((game, path, kind))
    if dispatched:
        for game, path, kind in dispatched:
            print("[redship.o2r] DISPATCHED TYPE: %s-owned %r is a %s resource"
                  % (game, path, kind), file=sys.stderr)
        sys.exit("[redship.o2r] refusing to build: %d curated resource(s) use a loader slot that single-exe builds "
                 "resolve by OWNING ARCHIVE (Room/Cutscene/Path -- RsbsMMArchiveFactoryDispatcher in "
                 "games/mm/2s2h/GameExports_SingleExe.cpp). redship.o2r is registered as neither game's archive, so "
                 "these would be parsed with the wrong game's reader." % len(dispatched))

    # Raw-segmented-texture guard (constraint 5 above, #605). Walk every
    # curated DisplayList's own command stream and refuse any that carries a
    # raw segmented G_SETTIMG -- the same walk the crossgame-model CTest row
    # performs at load time, run here at generation time so a bad model is
    # refused at BUILD time with a named reason instead of shipping and
    # rendering wrong.
    raw_segmented = []  # (game, path, index, segment)
    malformed = []  # (game, path, reason)
    for game, path in selected:
        data = payloads[(game, path)]
        if not is_display_list(data):
            continue
        try:
            findings = find_raw_segmented_texture_refs(data)
        except MalformedDisplayList as exc:
            malformed.append((game, path, str(exc)))
            continue
        for index, segment in findings:
            raw_segmented.append((game, path, index, segment))
    if malformed:
        for game, path, reason in malformed:
            print("[redship.o2r] UNPARSABLE DISPLAY LIST: %s-owned %r: %s" % (game, path, reason), file=sys.stderr)
        sys.exit("[redship.o2r] refusing to build: %d curated display list(s) could not be walked far enough to "
                 "prove they carry no raw segmented texture reference; a model this guard cannot verify is refused, "
                 "not shipped." % len(malformed))
    if raw_segmented:
        offenders = sorted({(game, path) for game, path, _index, _segment in raw_segmented})
        for game, path, index, segment in raw_segmented:
            print("[redship.o2r] RAW SEGMENTED TEXTURE: %s-owned %r carries a raw segmented G_SETTIMG at "
                 "instruction %d, segment 0x%02X" % (game, path, index, segment), file=sys.stderr)
        sys.exit("[redship.o2r] refusing to build: %d raw segmented texture reference(s) across %d curated display "
                 "list(s) (%s). These resolve against the HOST game's segment table at draw time, not the "
                 "exporting game's -- the OoTMM kObjectPatches[] hazard (see constraint 5 in this script's "
                 "docstring and src/common/tests/test_crossgame_model.c). Drop the offending resource from the "
                 "manifest, or give it a hand-authored patch before curating it."
                 % (len(raw_segmented), len(offenders), ", ".join("%s:%s" % (g, p) for g, p in offenders)))

    # Array reader-agreement guard (constraint 6 above, #604). The curated
    # archive belongs to neither game's factory registry, so whichever game is
    # RUNNING parses these arrays. Walk each one under both factories' read
    # rules and refuse the first element where they stop consuming the same
    # bytes -- a disagreement there does not just skew one value, it desyncs
    # the reader for the whole rest of the resource.
    array_disagreements = []  # (game, path, index, scalar_type, oot_width, mm_width)
    unwalkable_arrays = []  # (game, path, reason)
    for game, path in selected:
        data = payloads[(game, path)]
        if not is_array(data):
            continue
        try:
            findings = find_array_reader_disagreements(data)
        except MalformedArray as exc:
            unwalkable_arrays.append((game, path, str(exc)))
            continue
        for index, scalar_type, oot_width, mm_width in findings:
            array_disagreements.append((game, path, index, scalar_type, oot_width, mm_width))
    if unwalkable_arrays:
        for game, path, reason in unwalkable_arrays:
            print("[redship.o2r] UNPARSABLE ARRAY: %s-owned %r: %s" % (game, path, reason), file=sys.stderr)
        sys.exit("[redship.o2r] refusing to build: %d curated array resource(s) could not be walked far enough to "
                 "prove both games' Array factories consume them identically; an array this guard cannot verify is "
                 "refused, not shipped." % len(unwalkable_arrays))
    if array_disagreements:
        for game, path, index, scalar_type, oot_width, mm_width in array_disagreements:
            print("[redship.o2r] ARRAY READER DISAGREEMENT: %s-owned %r element %d is %s -- OoT's factory consumes "
                  "%d byte(s) there, MM's consumes %d"
                  % (game, path, index, SCALAR_TYPE_NAMES.get(scalar_type, "scalar type %d" % scalar_type), oot_width,
                     mm_width), file=sys.stderr)
        sys.exit("[redship.o2r] refusing to build: %d curated array resource(s) are parsed DIFFERENTLY by the two "
                 "games' Array factories (%s). The curated archive is in neither game's factory registry, so the "
                 "RUNNING game parses these -- and the factory that does not implement the scalar width reads zero "
                 "bytes for it, desyncing every element after it (see constraint 6 in this script's docstring, "
                 "games/oot/soh/resource/importer/ArrayFactory.cpp vs games/mm/2s2h/resource/importer/"
                 "ArrayFactory.cpp, and #604). Drop the offending resource from the manifest."
                 % (len(array_disagreements),
                    ", ".join("%s:%s" % (g, p) for g, p, _i, _s, _o, _m in array_disagreements)))

    out_dir = os.path.dirname(os.path.abspath(args.out))
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    if os.path.exists(args.out):
        os.remove(args.out)

    total = 0
    with zipfile.ZipFile(args.out, "w", zipfile.ZIP_DEFLATED) as out:
        for game, path in selected:
            data = payloads[(game, path)]
            out.writestr(path, data)
            total += len(data)

    for z in zips.values():
        z.close()

    print("[redship.o2r] wrote %s: %d curated resource(s), %d bytes uncompressed"
          % (args.out, len(selected), total))
    return 0


if __name__ == "__main__":
    sys.exit(main())
