# RedShipBlueShip UI style guide

The Ship of Harkinian (SoH) menus under `games/oot/soh/SohGui/` are the reference. Rule 0 (operator, 2026-09-27): no
unneeded overhaul of an original SoH page. Structure, widgets, spacing, colours, fonts and layout stay as SoH shipped
them. A minor wording change to an original row is allowed only when it is genuinely needed, and the PR must say so.
Everything RedShipBlueShip adds follows the rules below, so that it reads as one more SoH page.

The test is pixels. Render the SoH reference and ours with `redship --test ui-snapshot` and compare the PNGs
(section 12). Each rule cites the SoH line it copies. A rule marked [project rule] is stricter than SoH on purpose.

**Scope (ours):**
- `SohGui/SohMenuCombo.cpp` and `SohGui/SohMenuComboMmEnhancements.cpp`
- `AddCrossGamePointerWidgets` (`SohMenuRandomizer.cpp:823-846`)
- the capability and presentation code in `SohMenu.cpp:199-600`
- `SohGui/CreationProgressOverlay.cpp`
- `src/common/Combo{MmOptions,Spoiler,Tracker}Window.cpp`
- the player-facing strings in `src/common/cvar_shared_keys.h` (`kHostedMmEnhancements`)
- `src/common/combo_settings_view.cpp`, `src/common/combo_mm_*_view.*` and `src/common/gen_progress_overlay.c`
- `games/mm/2s2h/Rando/OptionsUiSingleExe.cpp`
- the toasts in the `*ForeignItemsSingleExe.cpp` TUs
- any new ImGui-drawing TU (the lint tripwire lists them)

**Excluded:**
- `ComboSettingsWindow.cpp`: retired from the menu, console-only.
- `ComboMenuBar.cpp`: never constructed.
- MM's tracker windows: vendored 2S2H UI.

Inside an SoH-shipped file, only the functions this project added are ours.

The "[Both Games]" prefix on shared-intent rows is ruled (operator, 2026-09-27): it stays, on the SoH rows it marks
and on ours (R-N4).

## 1. Structure

- **Header.** Added with `Menu::AddMenuEntry` (`Menu.cpp:270-273`). SoH's headers are one or two words. Ours is
  "Combo", placed after Randomizer (`SohMenu.cpp:94-104`). A new header needs operator approval.
- **Page.** Added with `SohMenu::AddSidebarEntry(header, name, columns)` (`SohMenu.cpp:29-34`). A page has 1-3
  columns (`MenuTypes.h:55-59`):
  - 3 for dense toggle pages (`SohMenuEnhancements.cpp:152`)
  - 2 for mixed pages (`SohMenuSettings.cpp:128`)
  - 1 for window-button and tracker pages (`SohMenuRandomizer.cpp:739`)

  Below 800 px the columns collapse to 1 (`Menu.cpp:892`). Sidebar names are Title Case, 1-3 words, at most 20
  characters. The selection persists BY DISPLAY NAME (`Menu.cpp:849-852`), so never rename a shipped sidebar,
  including ours. A page must hold at least one widget (#640, `Menu.cpp:896-904`). Pages under Combo register through
  `RegisterComboSectionPage` (`SohMenu.h:176-212`).
- **Column.** Set `path.column` explicitly before each column's first row (`SohMenuEnhancements.cpp:235,284`).
- **Section.** `WIDGET_SEPARATOR_TEXT` (`Menu.cpp:370-379`). Most SoH columns open with one
  (`SohMenuEnhancements.cpp:155`, `SohMenuSettings.cpp:132`). Some open with a row or a gray note instead
  (`SohMenuDevTools.cpp:41`, `SohMenuRandomizer.cpp:560-563`). Ours: open each column with a separator, optionally
  preceded by ONE gray note.
- **Row.** `SohMenu::AddWidget(path, name, type)` plus chained setters (`MenuTypes.h:116-214`), drawn by
  `Menu::MenuDrawItem` (`Menu.cpp:283-574`).

## 2. Widgets (`MenuTypes.h:31-53`, `UIWidgets.hpp:108-603`)

| Row | Helper | Look (defaults) |
|---|---|---|
| CHECKBOX / CVAR_CHECKBOX | `UIWidgets::Checkbox` / `CVarCheckbox` | label right of the box, padding 10x8 (`UIWidgets.hpp:264-305`) |
| COMBOBOX / CVAR_COMBOBOX | `Combobox` / `CVarCombobox` | label ABOVE, box as wide as the longest value (`UIWidgets.cpp:435-448`) |
| SLIDER_INT / CVAR_SLIDER_INT | `SliderInt` / `CVarSliderInt` | label ABOVE, with `-`/`+` buttons, full width (`UIWidgets.hpp:346-419`) |
| BUTTON | `Button` | `Sizes::Fill`. Primary actions may be 250 px (`SohMenuRandomizer.cpp:282,612-613`). Inline buttons use `Sizes::Inline` (`SohMenuSettings.cpp:432`) |
| WINDOW_BUTTON | `WindowButton` | the icon prefix is added automatically; `EmbedWindow` defaults to true (`UIWidgets.hpp:226-262`) |
| TEXT / SEPARATOR_TEXT | `TextWrapped` / `SeparatorText` | `TextOptions().Color(...)` (`Menu.cpp:370-390`) |
| CUSTOM | your function | only for compound rows |

- **R-W1.** Menu rows are registered with `AddWidget`, never drawn by hand. `SohMenuSettings.cpp` and
  `SohMenuEnhancements.cpp` contain zero `ImGui::` calls. SoH does use raw ImGui inside a few CUSTOM rows
  (`SohMenuRandomizer.cpp:568-571,600`). In our code a raw widget is allowed only where no UIWidgets helper exists,
  and it must be themed.
- **R-W2.** A raw `ImGui::Button`, `ArrowButton` or `SmallButton` must sit between
  `UIWidgets::PushStyleButton(THEME_COLOR)` and `PopStyleButton()` (`SohMenuRandomizer.cpp:90-104,403-418`;
  `SohModals.cpp:61-69`).
- **R-W3.** `MenuDrawItem` forces the theme colour onto every interactive row
  (`Menu.cpp:322,332,399,423,448,466,486`). Never set `.Color()` on one.
- **R-W4.** Dependent rows sit directly under their parent. SohMenu files contain no `Indent`.
- **R-W5.** For two controls on one line, put `.SameLine(true)` on the second (`SohMenuDevTools.cpp:106`).

## 3. Typography

- **Fonts.** Montserrat 16/20/24 and Inconsolata 14-24, with FontAwesome merged in (`OTRGlobals.cpp:551-559,2440-2446`).
  Body text is Montserrat 20. The header and sidebar are Montserrat 24 (`Menu.cpp:671`). The scale comes from "ImGui
  Menu Scaling" (`OTRGlobals.cpp:155-160`).
- **R-T1.** Never `PushFont` in a page or a pane.
- **R-T2.** Custom widths are in font units (`ImGui::GetFontSize() * N`, `SohMenuNetwork.cpp:63,73`). The only fixed
  width is 250 px.
- **R-T3.** Tooltips wrap at 80 characters automatically (`UIWidgets.cpp:16-39`, `UIWidgets.hpp:35-36`). Use `\n`
  only between items or paragraphs (`SohMenuSettings.cpp:198-203`).

## 4. Colour

- **Theme.** One of 14 menu-safe colours, LightBlue by default (`SohMenuSettings.cpp:29-44,133-139`;
  `SohMenu.cpp:92`), read through `THEME_COLOR`. Derived tints come from UIWidgets: buttons (`UIWidgets.cpp:114-122`),
  combos (`:450-464`), the checkmark (`:210`) and the slider grab (`:503-504`). Never hand-pick them.
- **Semantic colours.** Use `TextOptions().Color()` or the palette `UIWidgets::ColorValues`:
  - Gray for notes (`SohMenuRandomizer.cpp:560-563`)
  - Orange for warnings and experimental sections (`SohMenuSettings.cpp:240`, `SohMenuEnhancements.cpp:1816-1817`)
  - white at 0.4 alpha for placeholder hints (`Menu.cpp:931`)
- **R-C1.** Our files contain no all-literal `ImVec4(r,g,b,a)`, no `IM_COL32` and no literal toast colour arrays. These
  are fine: style colours (`ImGui::GetColorU32(ImGuiCol_*)`), the palette, `THEME_COLOR`, and CVar-driven alphas
  (`Menu.cpp:637-638`).
- `src/common` code has no SoH header (ADR 0002). It reaches the palette and the helpers only through a seam (section
  10).

## 5. Spacing and sizing (from UIWidgets; never restate)

- **Frame padding.** 10x8 on buttons, checkboxes and sliders; 10x6 on combos and inputs.
- **Rounding** is 3.
- **Border.** 5 on buttons, checkboxes and inputs; 0 on sliders (`UIWidgets.cpp:119-121,141-143,211-213,460-464,506-508`).
- **Vertical space.** Use `UIWidgets::Spacer`, `Separator` and `PaddedSeparator` (`UIWidgets.cpp:45-53,225-237`).
  SohMenu files contain no `ImGui::Spacing`, `Dummy` or `Indent`.
- **Tables.** `CellPadding` {8,8}, `BordersH|BordersV`, `TableSetupColumn` plus `TableHeadersRow`
  (`SohMenuRandomizer.cpp:38-60`).

## 6. Copy

- **R-N1.** Section headers are Title Case, 1-3 words, with no colon ("Saving", "Item Tracker Settings"). 79 of SoH's
  80 headers follow this.
- **R-N2.** Row labels are Title Case feature names: median 21 characters, never over 61 (363 of 390 SoH labels). Small
  words may stay lowercase. No trailing period.
- **R-N3.** Sliders read "Name: %d unit" (`SohMenuEnhancements.cpp:253-258`, `.Format("%d frames")`), or "Name: %d %%"
  with `.Format("")` (`SohMenuSettings.cpp:272-275`).
- **R-N4.** Labels never carry state, issue numbers or explanations. SoH changes a name at runtime only on TEXT rows, or
  to flip a verb: "Connecting..." / "Connected" (`SohMenuNetwork.cpp:99-106`) and "Viewport dimensions: {} x {}"
  (`ResolutionEditor.cpp:387-398`). The one project exception is the ruled `"[Both Games] "` prefix on shared-intent
  rows (ADR 0004 section 4.2; `combo_settings_view.cpp:252`). It is applied by the marker pass
  (`SohMenu.cpp:113-117`) and never hand-typed.
- **R-N5.** Names are unique IDs and search keys (`UIWidgets.cpp:301`, `Menu.cpp:212-216`). Disambiguate with a
  `##suffix` ("Enable##CrowdControl", `SohMenuNetwork.cpp:143`), never with a visible prefix.
- **R-N6.** Combobox values are short Title Case, at most 30 characters. A parenthetical is only a qualifier
  ("(Seeded)", `SohMenuEnhancements.cpp:38-144`). Explanations go in the tooltip as "Value: effect" lines
  (`SohMenuSettings.cpp:198-203`).
- **R-N7.** Window buttons are named "Toggle <Window>" (with `.EmbedWindow(false)`) or "Popout <Window> Settings". The
  tooltip is "Toggles the <Window>." or "Enables the separate <Window> Window.". Always set `.CVar`, `.WindowName` and
  `.HideInSearch(true)` (`SohMenuRandomizer.cpp:740-754`).
- **R-N8.** Labels may abbreviate "MM" and "OoT". Tooltips spell out "Majora's Mask" and "Ocarina of Time".
- **R-N9.** CVar keys go through macros or the `src/common` constants, not literals. SoH does this on 367 of 368 rows.

## 7. Tooltips

- **R-TT1 [project rule].** Every interactive row we add has a tooltip. SoH leaves some rows without one
  (`SohMenuEnhancements.cpp:1706-1711`, `SohMenuRandomizer.cpp:563`).
- **R-TT2.** Present tense, verb first ("Makes...", "Allows...", "Toggles..."). "You" is fine.
- **R-TT3.** End with a period (298 of 321 SoH tooltips).
- **R-TT4.** One or two sentences. SoH's median is 72 characters and its p90 is 173. Over 200 is a smell; over 600 is an
  error.
- **R-TT5.** No issue numbers, ADR or section references, file names, or implementation reasoning (SoH: 0 of 321).
- **R-TT6.** Caveats trail the main sentence: "Requires a scene reload to take effect." (`SohMenuEnhancements.cpp:218`).
  Ours end with "Majora's Mask only." rather than starting with it.
- **R-TT7.** The search indexes tooltips (`Menu.cpp:212-216`), so use the words a player would search for.
- Outside the menu, use `UIWidgets::Tooltip(text)` (`UIWidgets.cpp:55-58`), never a raw `ImGui::SetTooltip`.

## 8. Unavailable, dependent and locked rows

- **R-S1. Hidden** means the parent feature is off. Set `info.isHidden` in a PreFunc (`SohMenuEnhancements.cpp:164-168`).
- **R-S2. Disabled** means the row exists but something outside it forces it. SoH has two shapes:
  - (a) The disabledMap shape, built by MenuDrawItem as `"This setting is disabled because: \n"` followed by
    `"\n- <Reason>"` for each reason (`Menu.cpp:284,294-296`). Reasons are short Title Case fragments ("Save Not
    Loaded").
  - (b) A direct sentence in `disabledTooltip`: "This setting is forcefully enabled because ..."
    (`SohMenuEnhancements.cpp:315-320`), "This is not compatible with ..." (`:219`), "Must be on File Select to ..."
    (`SohMenuRandomizer.cpp:615`).

  Ours use shape (a), written directly into `disabledTooltip` ("This setting is disabled because: \n\n- Already Decided
  When This World Was Created"). Do not use `activeDisables` for this: it is a `std::vector<DisableOption>`, and
  `DisableOption` spans 0..13 (`MenuTypes.h:7-22`), so keys outside that enum cannot be represented.
- **R-S3.** A disabled reason lives in the tooltip, never in the label and never inline. A state that must be legible
  WITHOUT hovering (ADR 0004 sections 4.2 and 6) is one gray note row above the group (`SohMenuRandomizer.cpp:627-629`).
  That note also survives a race lockout, which replaces the tooltip (`Menu.cpp:301-305`).
- **R-S4.** `RaceDisable` defaults to true (`MenuTypes.h:113`). Mark cosmetic and QoL rows `.RaceDisable(false)`.
- **R-S5.** Destructive buttons confirm through `SohGui::RegisterPopup(title, message, "Reset", "Cancel", cb, nullptr)`
  (`SohMenuSettings.cpp:419-432`).

## 9. Notes and status rows

- **R-X1.** A gray `WIDGET_TEXT` note is one or two sentences (`SohMenuRandomizer.cpp:560-563,627-629`). A warning is
  an Orange line plus plain text (`SohMenuEnhancements.cpp:1816-1822`).
- **R-X2.** A TEXT row whose name changes every frame is fine (`ResolutionEditor.cpp:387-398`), but it stays within
  R-X1's length and gets `.HideInSearch(true)`.

## 10. Panes (GuiWindow)

- A pane derives from `Ship::GuiWindow`, registers with `AddGuiWindow` and a `CVAR_WINDOW` key, and is opened from a
  WINDOW_BUTTON row. Common-owned panes follow ADR 0008.
- Inside a pane, use the same helpers with `THEME_COLOR` (`randomizer_check_tracker.cpp:2223-2283`). From `src/common`,
  which cannot include UIWidgets, go through a C function-table seam that an OoT TU implements with those helpers (the
  plan's `ComboUi` seam, M6; it does not exist yet).
- **Settings groups** in a pane use `SeparatorText` [project rule, matching SoH's randomizer option pages,
  `option.cpp:450-479`]. `CollapsingHeader` is an SoH editor and tracker idiom (`CosmeticsEditor.cpp`,
  `SohInputEditorWindow.cpp`, `randomizer_check_tracker.cpp`). It is allowed in our tracker and spoiler panes, not in
  settings panes or menu pages.
- **Trick lists** follow `DrawTricksMenu` (`SohMenuRandomizer.cpp:171-551`): a filter; "Disable All" and "Enable All"
  250 px buttons; a two-column Disabled/Enabled table of area tree nodes; coloured tag chips (`tricks.cpp:101-110`); and
  the description as a tooltip.
- **Modals** are a centred `BeginPopupModal` with text and themed buttons (`SohModals.cpp:55-83`).

## 11. Anti-patterns

- raw player-facing widgets
- literal colours
- `TextDisabled` used for meaning (there are zero in `games/oot/soh` outside ours)
- hand spacing
- a CollapsingHeader on a settings surface
- state or issue numbers in labels
- inline reasons
- paragraph-length labels, tooltips or notes
- `.Color()` on menu rows
- literal CVars
- `PushFont`
- destructive buttons without a confirm

## 12. Verification by pixels

`redship --test ui-snapshot` (CTest row `UiSnapshot`, label `ui`) renders every page below into
`<build>/ui-snapshots/`. It uses one process, one window, one backend and pinned settings: a fresh config file, default
theme, scale and background opacity, multi-viewports off, and MSAA 1.

| Ours | SoH reference |
|---|---|
| Combo > Cross-Game Rules | Randomizer > General |
| Combo > Cross-Game Windows | Randomizer > Item Tracker |
| Combo > MM Enhancements | Enhancements > Quality of Life |
| Randomizer > Cross-Game | Randomizer > General (its gray note) |
| MM Randomizer Options pane / Tricks | Randomizer > Logic/Access / Tricks/Glitches |
| Creation overlay (and, once it exists, the Reset confirm) | the SoH modal ("Clear Config") |

Compare within the same run, the same profile and the same backend. Check:
- the fonts, rounding, borders and theme tints
- label placement: checkbox labels on the right; combo and slider labels above
- the separator style and the row rhythm
- the tooltip on hover

**What a run writes:**

| Path | Contents |
|---|---|
| `pages/<slug>.png`, `pages/<slug>.txt` | the whole window, and every string the frame submitted (rows scrolled out of view included) |
| `compare/<ours>__vs__<ref>[@variant].png` | the SoH reference cropped to its content, over ours |
| `iter/<slug>.png`, `iter/<slug>.diff.png` | before over after, and the difference x4 (only with `RSBS_UI_SNAPSHOT_BASELINE`) |
| `manifest.json` | the run (backend, readback, profile, archives) and, per capture, the hashes, the content rectangle, the settle count and the verdicts |
| `runtime-lint.txt` | the runtime copy lint R1-R7 over our rows |
| `soh-names.txt` | SoH's own row names and tooltips (ROM-rich runs only; R8) |

**Variants:**
- STATE: the four Cross-Game Rules states (unpaired, paired-legacy, frozen, corrupt), and the MM options pane's
  unpaired, frozen, mm-suspended and tricks-open.
- SCROLL: `@scrollN`, stepping each column by one view minus 48 px until the column reaches its end.
- HOVER: a pointer injected before ImGui reads input, so the tooltip is captured.
- MODAL.

**Environment:**

| Variable | Meaning |
|---|---|
| `RSBS_UI_SNAPSHOT_PAGES` | `all` (default), `refs`, `ours`, or a comma list of ids or `*` globs, each optionally with `@variant` (e.g. `Combo/*,window/Cross-Game Spoiler@paired`) |
| `RSBS_UI_SNAPSHOT_OUT` | the output directory (default `./ui-snapshots`) |
| `RSBS_UI_SNAPSHOT_PROFILE` | `auto` (default), `desk-1280x800` or `small-960x704` |
| `RSBS_UI_SNAPSHOT_BACKEND` | `auto` (default: GL, or DX11 on Windows when `CI` is set), `gl` or `dx11` |
| `RSBS_UI_SNAPSHOT_SETTLE` | the minimum and maximum frames per capture (default `4,12`); a capture ends on two identical frames |
| `RSBS_UI_SNAPSHOT_BASELINE` | a previous run's output directory, for the before/after composites and R8 |
| `RSBS_UI_SNAPSHOT_CVARS` | `key=value;...`, applied after the pins |

**Two modes:**
- **ROM-rich** (oot.o2r staged; the workstation) draws every SoH reference page and runs R8.
- **ROM-free** (hosted CI) draws a probe menu holding only the sections that register without oot.o2r: the Randomizer
  pointer page, the whole Combo section and Dev Tools. The rest is recorded as `skip: needs oot.o2r`.

soh.o2r is required in both modes, because the menu fonts live only there.

**The harness asserts structure only:**
- the drawable is the profile size
- the PNG decodes back to the same hash
- the page is not blank
- the menu window and the page's own section child were drawn, so a selection cannot silently fall back to another
  page
- the text log names the page
- the frame converged
- no popup leaked
- no game framebuffer was composited
- the config and `imgui.ini` in the run directory are untouched
- the runtime lint has no hit outside `.github/scripts/ui-runtime-lint-baseline.txt`

It never compares pixels against a stored image, never judges likeness, and never generates a seed or touches
`tests/golden/`.

The references gate is `python tools/ui-refs-diff.py <base>/manifest.json <new>/manifest.json`. It checks that every
SoH reference capture hashes the same as in the baseline run. Settings/General's build-version rows are
volatile, so that page is reported but not gated.

## 13. Lint

There are two halves, and both compare against a baseline that may only shrink.

**Static:** `python .github/scripts/check-ui-parity-lint.py`. It runs in CI (static-analysis workflow) with
`--self-test` first.
- Rules S0-S11 are listed in the script's docstring.
- The files and function slices it covers are listed in `.github/scripts/ui-lint-files.txt`. Rule S0 fails on any
  ImGui-drawing TU under `src/common` or any `*SingleExe*.cpp` that is not listed.
- Accepted hits live in `.github/scripts/ui-lint-baseline.txt`, keyed without line numbers. The check fails on a new
  hit and on a stale entry.
- `--report` prints everything. `--write-baseline` regenerates the baseline; review the diff, which must only lose
  lines.

**Runtime:** inside `redship --test ui-snapshot`. It is authoritative for names a PreFunc computes. It checks:
- R1: every interactive row has a tooltip
- R2: the tooltip ends a sentence and is at most 200 characters (a warning) or 600 (an error)
- R3: no issue number, ADR or section sign in any name, tooltip or disabled reason
- R4: labels are Title Case and at most 40 characters (a warning) or 61 (an error)
- R5: an interactive row's name is never rewritten by its PreFunc
- R6: TEXT notes are at most two sentences and 200 characters
- R7: names are unique per page

The baseline is `.github/scripts/ui-runtime-lint-baseline.txt`, enforced on complete runs only.
