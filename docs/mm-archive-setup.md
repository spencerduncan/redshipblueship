# Setting up Majora's Mask (`mm.o2r`)

RedShipBlueShip needs a game archive for each game it runs:

| Game | Archive | Generated from | First-run extraction in-app? |
|------|---------|----------------|------------------------------|
| Ocarina of Time | `oot.o2r` (or `oot-mq.o2r`) | Your OoT ROM | Yes — on a normal launch (or `--game oot`) the built-in extractor walks you through it |
| Majora's Mask | `mm.o2r` (also accepts `mm.zip` / `mm.otr`) | Your MM ROM | Yes — a normal launch offers it once `oot.o2r` is in place; `--game mm` offers it before any window opens ([#317](https://github.com/spencerduncan/redshipblueship/issues/317), [#944](https://github.com/spencerduncan/redshipblueship/issues/944)) |

## In-app generation (recommended)

A normal launch (no `--game` argument) boots Ocarina of Time, and the same
first-launch flow that sets up `oot.o2r` also sets up `mm.o2r`. Once OoT's
own archive is in place and no Majora's Mask archive is found, it asks
whether to generate one from your Majora's Mask ROM now:

- **Yes** — RedShipBlueShip looks for a supported Majora's Mask ROM
  (`.z64`/`.n64`/`.v64`) next to the executable, in the MM app directory (see
  [Where RedShipBlueShip looks](#where-redshipblueship-looks)) and, on
  AppImage runs, next to the `.AppImage` file. If it finds one it asks
  whether to use it ("Choose another" opens a file picker); otherwise it
  opens the file picker straight away. It then runs the bundled `ZAPD_MM`
  extractor behind the same "ROM Extraction" progress dialog OoT's extraction
  uses, and the boot continues into Ocarina of Time. No restart is needed:
  the Happy Mask Shop door and F10 reach Majora's Mask in the same session.
- **Not now** — Ocarina of Time starts alone. Crossings into Majora's Mask
  are refused with a message until `mm.o2r` exists, and the offer comes back
  on the next launch.

If you cancel the file picker, the ROM is rejected or the extraction fails,
you can try again or continue into Ocarina of Time without Majora's Mask: a
Majora's Mask problem never stops Ocarina of Time from starting.

With both ROMs next to the executable on a fresh install, OoT's own setup
lists them together ("ROMs found in application directory. Would you like
to process them?"). Answering Yes generates `oot.o2r` from the Ocarina of
Time ROM and `mm.o2r` from the Majora's Mask ROM in one pass, so the
separate Majora's Mask question does not appear.

Launching with `--game mm` skips Ocarina of Time's setup, so there the offer
comes before any window opens, as system dialogs: it looks for ROMs next to
the executable (next to the `.AppImage` file on AppImage runs), lets you
browse for one otherwise, and runs the same extractor. If you decline it or
the extraction fails, the app shows a "Majora's Mask archive missing"
message and closes.

Either way the resulting `mm.o2r` is written to the MM app directory — the
working directory for the portable builds RedShipBlueShip makes (see
[Where RedShipBlueShip looks](#where-redshipblueship-looks)).

Generation needs the packaged `assets/` folder next to the executable, with
Majora's Mask's extractor files in it (present in the Windows and Linux
release packages). A plain `cmake --build` tree has no `assets/` folder and
never shows the offer; use Option B below there. When `assets/` is present
but lacks Majora's Mask's extractor files, or `2ship.o2r` is missing, the
launch says so instead of offering, and starts Ocarina of Time alone.

You can also provide the archive yourself:

## Supported ROMs

`mm.o2r` can be generated from either of these Majora's Mask ROMs:

- **US N64 1.0** (CRC `5354631C`)
- **US GameCube** (CRC `B443EB08`)

Other regions/revisions are not supported. You must dump the ROM from your
own cartridge or disc.

## Option A — generate with standalone 2Ship2Harkinian

1. Download [2Ship2Harkinian](https://github.com/HarbourMasters/2ship2harkinian/releases)
   and run it once.
2. When prompted, point it at your Majora's Mask ROM. It generates `mm.o2r`.
3. Find the generated `mm.o2r`:
   - **Windows**: next to `2ship.exe`
   - **Linux**: next to the AppImage, or in `~/.local/share/2s2h/`
4. Copy `mm.o2r` next to the `redship` executable (see
   [Where RedShipBlueShip looks](#where-redshipblueship-looks)).

Note: archives are tied to the resource formats of the 2S2H code they were
generated with. An `mm.o2r` from a current 2S2H release matches the sources
vendored in this repository; if MM assets fail to load after a 2S2H format
change upstream, regenerate with Option B instead.

## Option B — build from this repository

This is the guaranteed-compatible path since it uses the exact exporter
sources vendored here.

```bash
# One-time setup
git submodule update --init
cmake -B build -S .

# Place your MM ROM where the extraction target expects it
cp /path/to/your/mm-rom.z64 OTRExporter/mm.z64

# Generate mm.o2r (also builds the ZAPD_MM extractor; this takes a while)
cmake --build build --target ExtractMMAssets
```

The archive is written to `games/mm/mm.o2r`. Copy it next to your `redship`
executable.

## Where RedShipBlueShip looks

`mm.o2r` is searched for in this order:

1. The MM app directory:
   - `$SHIP_HOME/mm.o2r` if `SHIP_HOME` is set (Linux/macOS)
   - otherwise the current working directory. RedShipBlueShip's builds are
     portable; only a `NON_PORTABLE` build (a libultraship option no
     configuration here enables) would use a per-user data dir such as
     `~/.local/share/2s2h/` instead.
2. The directory containing the `redship` executable
3. The current working directory

Simplest choice: **put `mm.o2r` in the same folder as the `redship`
executable**.

## Troubleshooting

- *"Majora's Mask is not playable yet: no mm.o2r game archive was found"* —
  the file is not in any of the locations above. The message lists the exact
  paths that were checked on your machine. Relaunch to get the generation
  offer again, or copy an `mm.o2r` next to the executable.
- No generation offer at launch — an MM archive (`mm.o2r`, `mm.zip` or
  `mm.otr`) already sits in one of the locations above, or the install has no
  `assets/` folder (see [In-app generation](#in-app-generation-recommended)).
- MM starts but assets look wrong or it crashes while loading — the archive
  was likely generated by an incompatible 2S2H version. Regenerate it with
  Option B.
- *"Majora's Mask port archive missing"* — `2ship.o2r` (MM's port-asset
  archive: fonts and port textures, no ROM data) is required. RedShipBlueShip
  refuses to start Majora's Mask, or to switch into it, without it. It ships
  with the RedShipBlueShip package and cannot be generated from a ROM:
  re-download the package and keep `2ship.o2r` next to the `redship`
  executable.
