RedShipBlueShip combines Ocarina of Time (Ship of Harkinian) and Majora's Mask (2Ship2Harkinian) into a single executable with cross-game randomization inspired by OoTMM.

**This is a pre-alpha build.** Expect bugs, missing features, and breaking changes between releases. Windows and Linux only.

## Downloads

- **Windows (x64):** `redshipblueship-{{TAG}}-windows.zip`
- **Linux (x86_64):** `redshipblueship-{{TAG}}-linux.AppImage`

Both builds bundle `soh.o2r` and the asset extraction tools (`assets/extractor/`, `assets/xml/`). You need your own original Ocarina of Time and Majora's Mask ROMs: on first launch the game asks for them and extracts the remaining assets.

## Running

- **Windows:** extract the zip anywhere and run `redship.exe`.
- **Linux:** `chmod +x redshipblueship-{{TAG}}-linux.AppImage`, then run it.

## Known Issues

- **No macOS build.** The macOS CI build is disabled due to a libultraship linking issue; this pre-alpha supports Windows and Linux only.
- **Stubbed settings tabs.** Several settings/enhancements tabs in the menu bar are placeholders and have no effect yet.
- **MM enhancement hooks disabled.** The Majora's Mask C++ enhancement layer is stubbed out in single-executable builds, so MM enhancements are unavailable.
- **Config file name.** Settings are saved to `shipofharkinian.json` (legacy name inherited from upstream Ship of Harkinian).

Found something else? Report it at https://github.com/spencerduncan/redshipblueship/issues.
