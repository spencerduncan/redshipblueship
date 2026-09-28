# RedShipBlueShip UI style guide

The Ship of Harkinian (SoH) menus under `games/oot/soh/SohGui/` are the reference. Rule 0 (operator, 2026-09-27): no
unneeded overhaul of an original SoH page. Structure, widgets, spacing, colours, fonts and layout stay as SoH shipped
them. A minor wording change to an original row is allowed only when it is genuinely needed, and the PR must say so.
Everything RedShipBlueShip adds follows the rules below, so that it reads as one more SoH page.

The test is pixels. Render the SoH reference and ours with `redship --test ui-snapshot` and compare the PNGs
(section 12). Each rule cites the SoH line it copies. A rule marked [project rule] is stricter than SoH on purpose.

**Scope (ours):**
- `SohGui/SohMenuCombo.cpp`, `SohGui/SohMenuComboMmEnhancements.cpp`, `SohGui/SohMenuComboMmMods.cpp` and
  `SohGui/SohMenuComboMmRandomizer.cpp`
- `AddCrossGamePointerWidgets` (`SohMenuRandomizer.cpp:823-846`)
- the capability and presentation code in `SohMenu.cpp:199-600`
- `SohGui/CreationProgressOverlay.cpp`
- `src/common/Combo{Spoiler,Tracker}Window.cpp`
- the player-facing strings in `src/common/cvar_shared_keys.h` (`kHostedMmEnhancements`)
- `src/common/combo_settings_view.cpp`, `src/common/combo_mm_*_view.*`, `src/common/combo_mm_options_page.*` and
  `src/common/gen_progress_overlay.c`
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

  Below 800 px the columns collapse to 1 (`Menu.cpp:892`). Sidebar names are Title Case, 1-3 words, and fit the
  200 px sidebar in Montserrat 24: about 16 characters, but measure it in the snapshot PNG, because the budget is
  pixels, not characters. SoH's widest entry, "Entrance Tracker" (16), is about 169 px; a selected entry's highlight
  needs about 10 px of padding each side, so keep a label under about 180 px. Our "Cross-Game Windows" (18) was cut on
  both sides ("ross-Game Window"), and our "MM Enhancements" (15) measured 192 px, the whole child width, so its selected
  highlight lost its padding and rounded corners. Since 2026-09-27 those pages are "Windows" and "Majora's Mask". The selection
  persists BY DISPLAY NAME (`Menu.cpp:849-852`), so never rename an SoH sidebar. One of ours is renamed only together
  with a carry of the persisted value (`ComboSidebarCarryRenamedSelection`, `SohMenuCombo.cpp`) and a lock leg that
  drives it (MenuComboSection leg 6). A page must hold at least one widget (#640, `Menu.cpp:896-904`). Pages under Combo register through
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
    `"\n- <Reason>"` for each reason (`Menu.cpp:284,294-296`). Reasons are short fragments, mostly Title Case ("Save
    Not Loaded"; a few are sentence case, "Disabling VSync not supported").
  - (b) A direct sentence in `disabledTooltip`: "This setting is forcefully enabled because ..."
    (`SohMenuEnhancements.cpp:315-320`), "This is not compatible with ..." (`:219`), "Must be on File Select to ..."
    (`SohMenuRandomizer.cpp:615`).

  Ours use shape (a), written directly into `disabledTooltip` ("This setting is disabled because: \n\n- Already
  Decided"). The reason fragment is the model's own reason string in Title Case, built at runtime, never a literal
  that adds words the model does not own. Do not use `activeDisables` for this: it is a `std::vector<DisableOption>`, and
  `DisableOption` spans 0..13 (`MenuTypes.h:7-22`), so keys outside that enum cannot be represented.
- **R-S3.** A disabled reason lives in the tooltip, never in the label and never inline. A state that must be legible
  WITHOUT hovering (ADR 0004 sections 4.2 and 6) is one gray note row above the group (`SohMenuRandomizer.cpp:627-629`).
  That note also survives a race lockout, which rebuilds the tooltip from `activeDisables` and appends "- Race Lockout
  Active" (`Menu.cpp:294-305`): an SoH disabledMap row keeps its reason, a directly written tooltip (ours) is replaced.
- **R-S3a.** A row gated through SohMenu's capability and presentation API gets both halves from it (ADR 0004's
  2026-09-27 amendment): `SohMenu::ApplyPresentation` / `CapabilityGate` keep the row's name and write shape (a) through
  `SohMenu::DisabledTooltip`; the group's note is a gray `WIDGET_TEXT` row with `.HideInSearch(true)` and
  `.RaceDisable(false)` whose PreFunc is `SohMenu::CapabilityNote(key[, sentence])` or calls
  `SohMenu::ApplyPresentationNote`. A note is a sentence-case sentence, never a Title Case reason fragment pasted into
  one. The editable-but-not-active state is the note alone ("Majora's Mask is suspended; these take effect when you
  return."; that default is Majora's-Mask-specific, so an Ocarina of Time group passes its own sentence). A
  capability's tracking issue is a separate field of its record (`RegisterCapability(key, predicate, text, issue)`),
  never part of the text a player reads. The Majora's Mask page's manifest rows follow the same split: `reason` is the
  player text and prints no number, and `HostedMmEnhancement::issue` records the tracker, nonzero exactly on a non-live
  row (`HostedMmEnhancementsAreHonest()`, `MenuMmEnhancementRows`). A group on that page holding a non-live row gets
  one gray note directly above its rows (under its separator; in a pointer row's group, under the pointer's
  sentence), shown while any of its rows is drawn disabled ("Some of these settings are not available in this build.
  Hover one to see why.", `COMBO_MM_GROUP_NOTE_UNAVAILABLE`, the MM Randomizer page's group note and this project's
  own wording: SoH's menus have no gray capability note to copy); every shipped row is Live, so the shipped page draws
  none, and the harness-only `Combo/MM Row States` page draws both placements from a synthetic table.
- **R-S4.** `RaceDisable` defaults to true (`MenuTypes.h:113`). Mark cosmetic and QoL rows `.RaceDisable(false)`.
- **R-S5.** Destructive buttons confirm through `SohGui::RegisterPopup(title, message, "Reset", "Cancel", cb, nullptr)`
  (`SohMenuSettings.cpp:419-432`).

## 9. Notes and status rows

- **R-X1.** A gray `WIDGET_TEXT` note is one or two sentences (`SohMenuRandomizer.cpp:560-563,627-629`). A warning is
  an Orange line plus plain text (`SohMenuEnhancements.cpp:1816-1822`).
- **R-X2.** A TEXT row whose name changes every frame is fine (`ResolutionEditor.cpp:387-398`), but it stays within
  R-X1's length and gets `.HideInSearch(true)`.

## 10. Page or window (SoH's split)

SoH decides by WHEN a surface is used, not by how big it is. Ours follow the same split.

- **A setting chosen before or between sessions is a page.** Its rows are registered with `AddWidget`. SoH's
  randomizer options are rows on Randomizer > General and the option-group sidebars (`OptionGroup::AddWidgets`,
  `option.cpp:450-473`), and the trick list is a page whose list is one `WIDGET_CUSTOM` row (Randomizer >
  Tricks/Glitches, `SohMenuRandomizer.cpp:723-725`, drawn by `DrawTricksMenu`).
- **A tool used during play is a window.** It is toggled from a page by a "Toggle <Window>" row with
  `.EmbedWindow(false)`. Examples: Item, Entrance and Check Tracker (`SohMenuRandomizer.cpp:739-790`), Timers
  (`SohMenuEnhancements.cpp:1909`) and Input Viewer (`SohMenuSettings.cpp:445`).
- **A large editor still lives in its page.** SoH gives some editors a `GuiWindow`, but `WindowButtonOptions`
  embeds it by default (`EmbedWindow` defaults to true, `UIWidgets.hpp:226-262`), so the editor draws inside its
  own page until the player pops it out with a "Popout <Window>" row. Examples: Cosmetics Editor and Audio Editor
  (`SohMenuEnhancements.cpp:1869,1879`), Plandomizer (`SohMenuRandomizer.cpp:730`) and Mod Menu
  (`SohMenuSettings.cpp:517`).

Ours: MM's randomizer options and tricks freeze at the creation event, so they are pages (Combo > MM Randomizer
and Combo > MM Tricks, `SohMenuComboMmRandomizer.cpp`, since 2026-09-27; ADR 0004's host amendment of that date).
Combo > Windows holds only live-play tools: the Cross-Game Spoiler, the Combo Tracker and MM's trackers. A new
surface that freezes at creation, or that feeds generation, is never a pop-out.

## 10b. Panes (GuiWindow)

- A pane derives from `Ship::GuiWindow`, registers with `AddGuiWindow` and a `CVAR_WINDOW` key, and is opened from a
  WINDOW_BUTTON row. Common-owned panes follow ADR 0008.
- **Chrome** is `GuiWindow::Draw`'s: ImGui's title bar with a close button, because `GuiWindow::Draw` passes the
  window's visibility to `ImGui::Begin` (`libultraship/src/ship/window/gui/GuiWindow.cpp:72`). A pane that overrides
  `Draw` (to read its CVar live) still passes an `open` flag and calls `SetVisibility(false)` when it is closed.
- Inside a pane, use the same helpers with `THEME_COLOR` (`randomizer_check_tracker.cpp:2223-2283`). From `src/common`,
  which cannot include UIWidgets, go through the `combo_ui` seam (`src/common/combo_ui.h`): a C function table
  (Checkbox, Combobox, SliderInt, Button, SeparatorText, NoteText, WarningText, Tooltip, TagChip, Confirm,
  PushTheme/PopTheme, Spacer, RowText) that `SohGui/ComboUiSoh.cpp` implements with those helpers and installs from a file-scope
  initializer. Pass each widget its tooltip and, when disabled, a disabled tooltip from `ComboUi_DisabledTooltip`
  (shape (a) of R-S2). Every widget reports its rectangle and shown tooltip to an optional recorder, which is how the
  snapshot harness finds and hovers a pane row. With no table installed, `ComboUi_Get()` returns a raw-ImGui fallback
  (`combo_ui.cpp`, excluded from the lint); the shipped binary always installs SoH's (the ComboMMOptionsPage lock).
- **Settings groups** in a pane use `SeparatorText` [project rule, matching SoH's randomizer option pages,
  `option.cpp:450-479`]. `CollapsingHeader` is an SoH editor and tracker idiom (`CosmeticsEditor.cpp`,
  `SohInputEditorWindow.cpp`, `randomizer_check_tracker.cpp`). It is allowed in our tracker and spoiler panes, not in
  settings panes or menu pages. Theme it as SoH's tracker panes do (`UIWidgets::PushStyleCombobox(THEME_COLOR)` around
  the header: rounded, 10x6 padding, the theme colour at half alpha, as the Check Tracker Settings pane's section
  headers in `randomizer_check_tracker.cpp`; the seam's PushTheme/PopTheme). `CosmeticsEditor.cpp`'s flat, opaque
  `PushStyleHeader` is SoH's editor-pane variant; the tracker style is the one captured beside our panes.
- **Tables** in a pane take SoH's shape (`randomizer_check_tracker.cpp`'s settings table, `SohMenuRandomizer.cpp`'s
  location tables): `CellPadding` 8x8, `BordersH | BordersV`, `TableSetupColumn` + `TableHeadersRow`, cells that wrap
  rather than clip. No `ScrollY` inside a pane that scrolls as a whole: a scrolling table with no height fills the pane
  with empty bordered rows. The header row stays ImGui's table-header gray, as in the Check Tracker Settings pane:
  the theme's `ImGuiCol_Header*` colours reach a header row only while it is hovered or clicked. A count belongs in
  a gray note under the section header, never in the header (R-N1, R-N4).
- **Status marks** are FontAwesome glyphs (merged into every menu font, `OTRGlobals.cpp`; the macros are
  libultraship's `IconsFontAwesome4.h`), never bracketed ASCII such as `[x]`, `[s]` or `[ ]`. This is a project
  convention, not an SoH idiom: SoH's check tracker marks a check's status by row colour and uses glyphs only on its
  skip and lock buttons (`ICON_FA_PLUS`/`ICON_FA_TIMES`, `ICON_FA_UNLOCK`/`ICON_FA_LOCK`,
  `randomizer_check_tracker.cpp`). A pane uses ONE notation for one state: the crossing tables lead each check name
  with the Checks lists' glyph rather than adding a Yes/No column.
- **Pane size.** The Combo Tracker and the Cross-Game Spoiler open at 480 x 520 and are never taller than what they
  draw (`ComboPaneFit`, `ComboTrackerWindow.h`), so a short state is not a tall empty box; a long one scrolls, as
  SoH's panes do. Table cells wrap in balanced lines, so a long name never leaves its last word alone on a line.
- **Trick lists** follow `DrawTricksMenu` (`SohMenuRandomizer.cpp:171-551`): a filter; "Disable All" and "Enable All"
  250 px buttons; a two-column Disabled/Enabled table of area tree nodes; coloured tag chips (`tricks.cpp:101-110`); and
  the description as a tooltip.
- **Modals** are a centred `BeginPopupModal` with text and themed buttons (`SohModals.cpp:55-83`).
- **Progress dialogs** look like SoH's ROM-extraction modal (`OTRGlobals.cpp`, `RunExtract`): the popup background;
  the title bar in the theme colour (`RunExtract` pushes `ImGuiCol_TitleBgActive` around its whole frame, as SoH's
  graphics loop does around every frame); a border of black at 0.3 alpha; and a bar with rounding 3, padding 10x8, the
  theme colour as the fill and the same colour at 0.6 alpha as the track, 600 x 50 at the default scale (in font
  units). The dim is the one deliberate difference: `RunExtract` also pushes an OPAQUE DarkGray
  `ImGuiCol_ModalWindowDimBg`, because it runs before the game exists and there is nothing behind it. A progress
  dialog over the game or file select dims with the style's own `ImGuiCol_ModalWindowDimBg`, as SoH's in-game modals
  do (`SohModals.cpp`). The creation overlay (`CreationProgressOverlay.cpp`) is a plain window rather than a popup
  for teardown reasons, so it draws that dim itself: a full-viewport, input-less window brought to the display front,
  never the background draw list (that renders behind the game image and the menu). Its frame is pumped outside
  SoH's per-frame title-bar push, so it pushes the theme colour onto its own title bar.
- **Toasts** are `Notification::Emit` with `Notification::Options`' default colours and the player's configured
  duration, as every SoH toast is (`Autosave.cpp`'s "Game autosaved"): an optional short prefix and one short message.
  The overlay draws every field on ONE line at 1.8x and never wraps, so a toast is about 53 characters at most (the
  width of the 832-px window the ui tier renders); the UI snapshot's toast pages fail when a toast leaves the window.
  R-N8 still applies: spell out "Majora's Mask" and "Ocarina of Time" in a toast's sentence. Mute it when the call
  site can run without audio.

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
| Combo > Windows | Randomizer > Item Tracker |
| Combo > Majora's Mask (was MM Enhancements) | Enhancements > Quality of Life (same three-column measure) |
| Combo > MM Row States (harness-only: the Majora's Mask page's builder over a synthetic non-live table, so its disabled rows and group notes are drawn) | Enhancements > Quality of Life |
| Combo > MM Mods | Randomizer > Tricks/Glitches (the two-column Disabled/Enabled table; OoT's Settings > Mod Menu throws in the harness's fresh config) |
| Randomizer > Cross-Game | Randomizer > General (its gray note, at its two-column measure) |
| Combo > MM Randomizer | Randomizer > General (the two-column option page) |
| Combo > MM Tricks | Randomizer > Tricks/Glitches (the Disabled/Enabled trick table) |
| Combo Tracker pane, Cross-Game Spoiler pane | SoH's Check Tracker Settings pane ("window/Check Tracker Settings": pane chrome, its themed section headers and its table), and Randomizer > Item Tracker. The Check Tracker itself shows only "Waiting for file load..." without a save, so it is not captured |
| Creation overlay | SoH's progress modal ("ROM Extraction", a harness copy of `RunExtract`'s modal and frame pushes, held to `RunExtract` by lint rule C1) |
| Creation overlay over the open menu | SoH's modal over the same menu page ("Clear Config@over-menu") |
| Cross-Game Rules Reset confirm, MM Randomizer Reset confirm | the SoH modal ("Clear Config") |
| The creation shortfall, failure and goal-warning toasts | SoH's toast shape ("Game autosaved") |

Compare within the same run, the same profile and the same backend. Check:
- the fonts, rounding, borders and theme tints
- label placement: checkbox labels on the right; combo and slider labels above
- the separator style and the row rhythm
- the tooltip on hover

**What a run writes:**

| Path | Contents |
|---|---|
| `pages/<slug>.png`, `pages/<slug>.txt` | the whole window, and the frame's text: for a menu page every string it submitted (rows scrolled out of view included); for a pane only what was inside a clip rect, i.e. what the PNG shows |
| `compare/<ours>__vs__<ref>[@variant].png` | the SoH reference cropped to its content, over ours |
| `iter/<slug>.png`, `iter/<slug>.diff.png` | before over after, and the difference x4 (only with `RSBS_UI_SNAPSHOT_BASELINE`) |
| `manifest.json` | the run (backend, readback, profile, archives) and, per capture, the hashes, the content rectangle, the settle count and the verdicts |
| `runtime-lint.txt` | the runtime copy lint R1-R7 over our rows |

A `compare/` or `iter/` image that cannot be written is counted, not silent: the summary line reads "N composite(s)
not written" and the manifest's run block holds `compositeWriteFailures`. On Windows the writer retries a path past
MAX_PATH through the extended-length namespace, so a long output directory no longer loses them.
| `soh-names.txt` | SoH's own row names and tooltips (ROM-rich runs only; R8) |

**Variants:**
- STATE: the five Cross-Game Rules states (unpaired, paired-legacy, frozen, corrupt, and empty-oot-classes, the one
  that draws an empty-set note), Majora's Mask's autosave,
  Combo > MM Row States' default (every gated row and both notes hidden), heading-on (the heading group's Dormant row
  drawn disabled under the note right under the heading) and gate-on (the pointer group's Dormant and Partial rows
  drawn disabled under the note below the pointer's sentence; no race-lockout state, because every row that page
  registers is `.RaceDisable(false)`, so a lockout changes nothing there), Combo > MM Randomizer's unpaired, frozen and
  mm-suspended (each shows its own note), and Combo > MM Tricks'
  unpaired and frozen (the trick headline, then the freeze sentence).
  The Cross-Game Spoiler draws paired (no crossings), crossings (crossings both ways authored through the crossing
  store, an MM save in the shadow and a synthetic OoT tracker adapter, so both tables are drawn with their found-state
  glyphs) and unpaired; the Combo Tracker draws paired, unpaired and progress (the same crossings, MM save and OoT
  adapter, with one collected, one skipped and two open OoT checks and its Checks list open, so the status glyphs and
  both crossing tables are drawn).
- SoH PANE: SoH's Check Tracker Settings pane, the reference both of those panes compare with (`compare/`). It is
  opened through its own `GuiWindow::Show` and closed with `Hide`, because an SoH pane latches its visibility CVar at
  construction and never re-reads it (ours read theirs live). A run where the pane is not registered skips it ROM-free
  and fails it ROM-rich.
- SCROLL: `@scrollN`, stepping each column (a menu page) or the pane itself (a window) by one view minus 48 px until
  it reaches its end, at most 9 views.
- HOVER: a pointer injected before ImGui reads input, so the tooltip is captured. Cross-Game Rules hovers its
  direction and goal comboboxes and (frozen) its first slider and its goal row; Majora's Mask hovers its first row and
  Windows its MM Item Tracker toggle (`PageSpec::hoverRows`, a named row, captured in the page's first state unless
  `PageSpec::hoverRowState` names another). Combo > MM Randomizer hovers its first row unpaired and frozen and its
  first capability-blocked row (named rows; the frozen and blocked ones are in `PageSpec::disabledHovers`). Combo > MM
  Tricks hovers a live, an unbound and a reserved trick unpaired and the live one frozen (`PageSpec::paneHovers`,
  found through the `combo_ui` rect recorder, because the trick list is one custom row; the harness scrolls the
  table's own child to reach a row). A disabled row's hover must show SoH's disabled shape with no tracker number.
  The trick hovers are composited against Settings > Graphics' Current FPS hover (`PageSpec::hoverCompareWith`),
  SoH's one captured tooltip: SoH's Tricks page draws its trick names as plain text with no item id, so no hover can
  be injected there.
- MODAL: SoH's "Clear Config" reference, and the Cross-Game Rules and MM Randomizer Reset confirms (each queued
  through its Reset row's own Callback, the call the player's click makes).
- `over-menu` (the creation overlay, and SoH's "Clear Config" as its reference): Combo > Cross-Game Rules left open
  under the box, as a real pumped frame draws it. The page is first settled alone, then with the box over it, and
  `DimOracle` requires three things: "Main Menu" was drawn (and the bare frame is not nearly uniform outside the box);
  the overlay's own dim window sits above the menu and below the box in the display order; and every pixel outside
  the box equals the bare pixel blended with the style's `ImGuiCol_ModalWindowDimBg`, within 2 per channel. SoH's
  modal passes the same pixel check, so "dims the way a modal dims" is measured, not read off the picture.
- TOAST: a page emits one toast through its production emitter (`OoT_Creation_EmitShortfallToast`,
  `OoT_Creation_ReportFailureAtFileSelect`, `OoT_Creation_EmitGoalWarningToast`), captures it, and clears it
  (`OoT_Notification_ClearForTest`). Its oracle finds exactly one `notification#` window, requires it inside the
  window's width, and measures "not blank" inside the toast's own rectangle. ROM-free, the harness registers its own
  Notifications window, so CI draws them too.

**Environment:**

| Variable | Meaning |
|---|---|
| `RSBS_UI_SNAPSHOT_PAGES` | `all` (default), `refs`, `ours`, or a comma list of ids or `*` globs, each optionally with `@variant` (e.g. `Combo/*,window/Cross-Game Spoiler@paired`) |
| `RSBS_UI_SNAPSHOT_OUT` | the output directory (default `./ui-snapshots`) |
| `RSBS_UI_SNAPSHOT_PROFILE` | `auto` (default: the largest that fits the desktop), `desk-1280x800`, `small-960x704` or `min-832x600` (what a hosted Windows runner gets) |
| `RSBS_UI_SNAPSHOT_BACKEND` | `auto` (default: GL, or DX11 on Windows when `CI` is set), `gl` or `dx11` |
| `RSBS_UI_SNAPSHOT_SETTLE` | the minimum and maximum frames per capture (default `4,12`); a capture ends on two identical frames |
| `RSBS_UI_SNAPSHOT_BASELINE` | a previous run's output directory, for the before/after composites and R8 |
| `RSBS_UI_SNAPSHOT_CVARS` | `key=value;...`, applied after the pins |
| `RSBS_UI_SNAPSHOT_SABOTAGE` | break one mechanism so its assert is SEEN going red (below); a sabotaged run must fail |

**Two modes:**
- **ROM-rich** (oot.o2r staged; the workstation) draws every SoH reference page and runs R8.
- **ROM-free** (hosted CI) draws a probe menu holding only the sections that register without oot.o2r: the Randomizer
  pointer page, the whole Combo section and Dev Tools. The rest is recorded as `skip: needs oot.o2r`.

soh.o2r is required in both modes, because the menu fonts live only there. The harness writes its own config
(`rsbs-ui-snapshot.json`) to the path the Context reads it from, `GetPathRelativeToAppDirectory`, so `SHIP_HOME`
(Linux, macOS) and a `NON_PORTABLE` pref path are honoured; hosted Linux CI runs the row with `SHIP_HOME` set to a
fresh directory to keep that path exercised.

**The harness asserts structure only:**
- the drawable is the profile size
- the PNG decodes back to the same hash
- the page is not blank
- the menu window and the page's own section child were drawn, so a selection cannot silently fall back to another
  page
- the text holds a string only the page BODY draws (the manifest's `bodyText`: the first registered section header,
  control or note with no PreFunc that is not part of the header bar or sidebar, which are drawn on every page), and
  that string is absent from a sibling page's capture
- each authored state shows its own text (the manifest's `found`), in some captured view, and that text is absent
  from the state's contrast (the capture the row would produce if the state were not authored)
- each hover shows every authored line of its row's current tooltip (`hoverLines`; `hoverText` is the first), each
  absent from the same state without the pointer. Every line, because SoH's disabled shape opens every disabled
  row's tooltip with the same "This setting is disabled because:", so only the reason line proves which tooltip was
  drawn
- Combo > MM Tricks' trick census, in each of its states (both turn two tricks on first, so both columns draw): for
  one frame the `combo_ui` rect recorder lists every trick name the list drew and the column child that drew it, and
  each of MM's trick descriptors must appear exactly once, in the column its value puts it in, with its row state's
  tooltip, and no other name may appear. The hovers read four rows; the census reads all of them. Observed red
  2026-09-27 with a trick dropped, one drawn twice and one misfiled into the other column: three problems per state,
  each named
- the frame converged
- no popup leaked
- no game framebuffer was composited
- no ImGui frame was left open (`NewFrame` asserts this in a Debug build; the harness checks it in every build type,
  and a page that throws mid-draw fails by name while the frame is unwound and closed)
- the player's `shipofharkinian.json` and `imgui.ini` in the app directory are untouched
- the runtime lint has no hit outside `.github/scripts/ui-runtime-lint-baseline.txt`

It never compares pixels against a stored image, never judges likeness, and never generates a seed or touches
`tests/golden/`.

**Seeing each assert go red.** `RSBS_UI_SNAPSHOT_SABOTAGE` takes a comma list; each value breaks one mechanism and
names what must fail. Run them after changing the harness itself.

| Value | Breaks | Must fail |
|---|---|---|
| `no-state` | EnterState authors nothing | every authored state's text check ("does not show", or "also shown in state") |
| `no-hover` | no pointer injection | every hover ("the hover capture shows no tooltip") |
| `no-scroll` | panes keep their first view | the Combo Tracker's `progress` ("does not show ... in any captured view"; observed 2026-09-27, when the MM options pane's `tricks-open` state left with the pane) |
| `throw` | the first project page's first row throws mid-draw | that capture only, by name; every later capture still passes (not blank) |
| `throw,leave-open` | the exception path leaves the ImGui frame open (the old behaviour) | the open-frame check, on the next pump |
| `keep-imgui-ini` | `imgui.ini` stays armed and ImGui's shutdown save runs | the isolation check on `imgui.ini` |
| `player-config` | the harness names `shipofharkinian.json` as its config | the isolation check on `shipofharkinian.json` |
| `hover-first-line` | a hovered row draws only the first line of its tooltip | every hover whose tooltip has a second line ("does not show its row's tooltip") |
| `no-menu-under` | the overlay's `over-menu` variant leaves the menu hidden | that capture ("the menu under the dim was not drawn") |
| `no-dim` | the overlay's dim is drawn fully transparent | that capture ("... pixels outside the box are not the menu dimmed by ModalWindowDimBg") |

**The original-page guard runs on a ROM-staged workstation only.** Hosted CI is ROM-free: SoH's own menu is not
populated there (only Dev Tools/General registers), R8's `soh-names.txt` is not written, and CI has no base run to
compare against. So rule 0 is protected by the iteration procedure, not by CI: `python tools/ui-refs-diff.py
<base>/manifest.json <new>/manifest.json` on two ROM-rich runs (main, then the branch), plus R8 through
`RSBS_UI_SNAPSHOT_BASELINE`. The refs gate fails (exit 1) when a reference capture's pixels or text moved, and also
when a reference that passed in the base is LOST: absent, failed (a throw, a failed oracle, a blank page) or skipped
in the new run. Settings/General's build-version rows are volatile, so its hashes are reported but not gated; losing
it is still gated.

## 13. Lint

There are two halves, and each compares against a checked-in baseline. On a pull request, CI also fails when
EITHER baseline gained an entry relative to the base branch (`check-ui-parity-lint.py --no-grow-against
origin/<base>`, the "lint baselines may only shrink" step), so a PR cannot add a hit and its baseline line together.
A reworded entry counts as growth: fix the surface instead.

**Static:** `python .github/scripts/check-ui-parity-lint.py`. It runs in CI (static-analysis workflow) with
`--self-test` first.
- Rules S0-S11 are listed in the script's docstring.
- Rule C1 holds each harness COPY of an SoH surface to its original (`REFERENCE_COPIES` in the script): today the
  "ROM Extraction" reference, whose three harness functions must carry `RunExtract`'s style pushes and pops, colour
  locals, modal flags and bar size statement for statement. It is never baselined: a drifted copy is not a reference.
- The files and slices it covers are listed in `.github/scripts/ui-lint-files.txt`: `path::Function` lints one
  function, `path::@from=TOKEN` lints from TOKEN's line to the end of the file. Inside an SoH-shipped file only our
  code is linted: `SohMenuRandomizer.cpp::AddCrossGamePointerWidgets`, and `SohMenu.cpp` from the capability
  reasons down. Rule S0 fails on any ImGui-drawing TU under `src/common` or any `*SingleExe*.cpp` that is not listed.
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
