RedShipBlueShip combines Ocarina of Time (Ship of Harkinian) and Majora's Mask (2Ship2Harkinian) into a single executable with cross-game randomization inspired by OoTMM.

**This is an alpha build.** Expect bugs, missing features, and breaking changes between releases. Windows and Linux only.

## Downloads

- **Windows (x64):** `redshipblueship-{{TAG}}-windows.zip`
- **Linux (x86_64):** `redshipblueship-{{TAG}}-linux.AppImage`

Both builds bundle `soh.o2r` and the asset extraction tools (`assets/extractor/`, `assets/xml/`). You need your own original Ocarina of Time and Majora's Mask ROMs: on first launch the game asks for them and extracts the remaining assets.

## Running

- **Windows:** extract the zip anywhere and run `redship.exe`.
- **Linux:** `chmod +x redshipblueship-{{TAG}}-linux.AppImage`, then run it.

## Before you play: saves from earlier builds

- **Back up first.** Copy your `Save/` and `Randomizer/` folders aside before running this build on them.
- **Saves are version 5.** Files from `v0.3.1-prealpha` load unchanged and are written as version 5 at their next save; from then on older builds refuse them.
- **Old paired files are refused.** A paired randomizer file created before 2026-10-10 is not opened ("Refused: Majora's Mask options differ"): erase it and create a new file.
- **Old seed packages are refused.** This build's seed generator is version 2 (#1264): a seed package made by an earlier build is refused; generate the seed again.
- **Old seeds make new worlds.** A seed and settings from an older build generate a different world on this one.
- **Rollback:** install into a new folder and keep the old one; earlier releases, `v0.1.1-prealpha` among them, stay downloadable from the releases page.

## Known issues and changes

- Known issues: [`docs/known-issues.md`](https://github.com/spencerduncan/redshipblueship/blob/{{TAG}}/docs/known-issues.md)
- What changed in this release: [`docs/release-notes-next.md`](https://github.com/spencerduncan/redshipblueship/blob/{{TAG}}/docs/release-notes-next.md)

Found something else? Report it at https://github.com/spencerduncan/redshipblueship/issues.
