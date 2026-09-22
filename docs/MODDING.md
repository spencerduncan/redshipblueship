# Modding

There are two unrelated kinds of "mod" here, and this document covers both:

- **Asset mods** — a `.o2r` archive that replaces textures, models, sequences or
  text at runtime. HD texture packs, retextures and custom music are these. No
  compiler needed; you drop a file in a folder. See
  [Asset mods and where they live](#asset-mods-and-where-they-live) below.
- **Code mods** — a fork of the source with your own C/C++ changes. That is the
  rest of this document, starting at [Preface](#preface).

## Asset mods and where they live

RedShipBlueShip is ONE executable running BOTH games, and the two games share one
libultraship resource manager. That has two consequences you need to know before
you install anything.

### The folders

```
<install dir>/
  mods/                      <- Ocarina of Time asset mods  (*.o2r)
    my-oot-texture-pack.o2r
    mods-in-subfolders-are-fine/
      another-oot-mod.o2r
    mm/                      <- Majora's Mask asset mods    (*.o2r)
      my-mm-texture-pack.o2r
      mods-in-subfolders-are-fine/
        another-mm-mod.o2r
```

**`mods/` is OoT's. `mods/mm/` is MM's.** Both are searched recursively, so a mod
distributed as its own folder works in either.

Why the two games do not simply get one folder each: both ports ask libultraship
for their mods directory by app name — `"soh"` for OoT, `"2s2h"` for MM — but in a
**portable** build (which is what every RedShipBlueShip release is)
`Context::GetAppDirectoryPath` ignores that app name and returns the install
directory. Both lookups therefore land on the same `mods/`. OoT keeps the root so
that existing installs and every upstream Ship of Harkinian mod keep working
unchanged; MM gets the reserved `mm` subfolder. The folder name is matched
case-insensitively, so `mods/MM/` works too. `mods/mm/` is created for you on
first run, with a `majoras_mask_mod_files_go_here.txt` marker inside.

Nothing else in the tree is reserved: `mods/anything-else/` is OoT's, like the
root. `mm` is reserved only as the **first** folder under `mods/` —
`mods/my-pack/mm/` is OoT's.

**`mods/mm/` is reserved from OoT only while the two games really do share one
folder.** The sharing comes from that portable-build lookup, not from the source, so
the combo checks it at runtime instead of assuming it: if a build resolves the two
mods folders to different directories (a `NON_PORTABLE` build uses a per-app-name
preferences folder; `SHIP_HOME` on Linux collapses them again even then), then
nothing is globbing OoT's `mods/mm/` but OoT, and OoT keeps it — exactly as before
this feature existed. You only ever lose a folder to MM when MM is actually reading
it.

Both halves of the tree are also **walked** the same way: recursively, following
directory symlinks (so you can keep one library of mods and link it into an
install), and skipping a subfolder the OS will not let the game read rather than
abandoning the whole walk.

> **Upgrading: if you already have a `mods/mm/` folder, its archives change
> owner.** OoT's mods folder has always been searched recursively, so anything you
> had at `mods/mm/*.o2r` — a mod that happened to ship inside a folder called `mm`,
> a Majora-themed OoT retexture pack — was an **OoT** mod and was listed in OoT's
> mod menu. From this version that folder is MM's: those archives are mounted for
> MM and no longer for OoT. Move them up into `mods/` (or into
> `mods/some-other-name/`) to keep them as OoT mods. If one of them is still listed
> in OoT's enabled mods, OoT prints a one-line `[OoT] NOTE:` about it on startup.

Both games accept the same archive types: `.o2r`, and `.otr` for older mods.
A `.zip` is **not** mounted for either game, because a mod is usually *distributed*
as a zip that contains the `.o2r` — unpack it. (Standalone 2Ship does mount a
`.zip`; the combo deliberately does not, so that one shared folder tree does not
accept different file types on its two sides.)

Loose (unpacked) asset files are **not** supported for either game — neither port
mounts a directory as an archive, so assets have to be inside an archive.

### Which mod wins

libultraship resolves a resource path by **last archive mounted wins**; there is no
priority field. Each game mounts its base archives first (`oot.o2r`/`oot-mq.o2r` +
`soh.o2r`, or `mm.o2r` + `2ship.o2r`) and then its mods, which is exactly why a mod
overrides a base asset at all.

- **Between two mods of the same game:** later wins. OoT's order is the one you
  set in its in-game mod menu (Enhancements → Mods), where you can enable,
  disable and drag to reorder. MM has no such menu yet: **every** archive under
  `mods/mm/` is mounted, sorted by file name ignoring the extension, so `10-base`
  loses to `20-override`. Rename to reorder; move the file out of `mods/mm/` to
  disable it. (Precisely, MM compares the whole path with the extension removed,
  which is upstream 2Ship's own comparator — so a subfolder name participates
  too: `mods/mm/aaa/z.o2r` loses to `mods/mm/bbb/a.o2r`.)

  **This is a known asymmetry inside one game, and it is not the intended end
  state.** OoT's half of the tree has enable/disable/reorder and MM's half does
  not; an MM mod menu at parity is tracked as a follow-up issue. It is listed here
  rather than papered over, because the alternative available today — making MM
  read OoT's `EnabledMods` setting — would let a stale OoT list silently disable an
  MM mod, which is a worse asymmetry, not a smaller one.

  Two related differences that were **not** worth keeping have been aligned
  instead: which file types count as a mod archive (both sides use OoT's rule, see
  above), and how the folder is walked (both sides follow directory symlinks and
  skip unreadable subfolders — for a while MM's half did not follow symlinks, so a
  linked mod folder worked under `mods/` and silently did nothing under
  `mods/mm/`).

  One more difference is bookkeeping rather than behaviour you can see: OoT mounts
  only the archives its enabled list names and identifies them by file name with
  the extension removed, so two archives with the same name in different
  subfolders count as one. MM mounts everything it finds.
- **Between the two games:** OoT and MM already ship many colliding resource
  paths of their own — 151 object names, 14 actor overlays and all three
  `gameplay_*_keep` archives (`docs/resource-namespace-audit.md`), plus 595 paths
  shared by `soh.o2r` and `2ship.o2r` (`docs/asset-collision-analysis.md`). The
  combo resolves that by re-mounting the **arriving** game's base archives and
  then its mods on every cross-game switch, so whichever game you are playing owns
  every path it ships.

  A mod is resolved by that same mechanism and introduces no new kind of
  collision. If an MM mod happens to ship a path OoT also uses, it owns that path
  only while you are in MM; the switch back to OoT hands it to
  `oot.o2r`/`oot-mq.o2r`/`soh.o2r` again, and vice versa. This is why the
  `mods/mm/` split matters: a mod placed in the wrong folder gets registered as
  the other game's, and *that* mis-registration would survive the switch and
  shadow the game you are actually playing.

### Troubleshooting

MM logs every mount to stderr. `[MM] Loaded mod archive: <path>` means it was
mounted; `[MM] Mounted N mod archive(s) from ...` is the total. A
`[MM] WARNING: could not mount mod archive` line means the file is not a readable
archive. If you see no lines at all, the archives are not under `mods/mm/`.

---

> So you would like to create a code mod? _BUCKLE UP_

## Preface

Git is required to be installed. Knowing how to use git is going to help, I will list out commands that should set you on the right but without a general understanding you will likely get stuck if you deviate from the happy path.

General coding knowledge is also usually a requirement. You might be able to get by without, but the more knowledgeable the better, as it will allow you to find what you are looking for and troubleshoot much more easily.

## Making a fork in the road

Your first step is to fork the repository. To do this, you will need a github account. Assuming you have a github account you can navigate to the Shipwright repo [here](https://github.com/HarbourMasters/Shipwright) and press the `Fork` button in the top right of the screen. When that process is complete you should land on a page that looks similar to the original repo, but under your username (eg: `https://github.com/<GITHUB USERNAME>/Shipwright`).

On this page you should see a green `Code` button, click this and copy the URL within. (You may use the github desktop app here as well, but I will not provide instructions for it). Then in your terminal/command prompt you will git clone to a local development folder using the copied URL

```bash
cd <path to where you'll clone (eg, ~/Documents)>
git clone https://github.com/<GITHUB USERNAME>/Shipwright.git
```

At this point, I will now direct you to our [BUILDING](building.md) guide, it will have the most up to date documentation on getting the Ship running on your local machine. Once you have successfully built and launched the executable you may return here.

## Look at all those branches!

Congrats, if you have made it this far! Before we start making changes, you will need to create a new branch. It's recommended that you cut all your branches from the `develop` branch, as that tends to be the most up-to-date. Before cutting a branch make sure you are on the `develop` branch with the following command:

```bash
git checkout develop
```

Then cut your branch with the following command:

```bash
git checkout -b <BRANCH NAME>
```

You can name your branch whatever you want, but it's recommended to name it something that describes the feature you are working on. For example, if you are adding a new feature to the ship, you might name your branch `feature/ship-new-feature`. If you are fixing a bug, you might name your branch `bugfix/ship-bugfix`. If you are adding a new mod, you might name your branch `mod/ship-new-mod`. The important thing is to be descriptive and consistent.

## Making the changes

The limit is your imagination. You can add new features, fix bugs, add new mods, or even change the way the game works. We will demonstrate this by creating a mod that changes the speed of the day/night cycle.

Let's begin by finding where the time is updated. Thankfully in the save editor we have a slider already hooked up to the time of day so we can check there for reference. The save editor file is at `soh/soh/Enhancements/debugger/debugSaveEditor.cpp`, if we do a quick search within that file for time we will find the following at around line 217:

```cpp
    SliderInt("Time", (int32_t*)&gSaveContext.dayTime, intSliderOptionsBase.Min(0).Max(0xFFFF).Tooltip("Time of day"));
    if (Button("Dawn", buttonOptionsBase)) {
        gSaveContext.dayTime = 0x4000;
    }
    ImGui::SameLine();
    if (Button("Noon", buttonOptionsBase)) {
        gSaveContext.dayTime = 0x8000;
    }
    ImGui::SameLine();
    if (Button("Sunset", buttonOptionsBase)) {
        gSaveContext.dayTime = 0xC001;
    }
    ImGui::SameLine();
    if (Button("Midnight", buttonOptionsBase)) {
        gSaveContext.dayTime = 0;
    }
```

So this tells us that `gSaveContext.dayTime` is what we're looking for. Let's now do a global search for this to see if we can find where it is updated. We find the following in `soh/src/code/z_kankyo.c` around line 925:

```cpp
if (IS_DAY || gTimeIncrement >= 0x190) {
    gSaveContext.dayTime += gTimeIncrement;
} else {
    gSaveContext.dayTime += gTimeIncrement * 2; // time moves twice as fast at night
}
```

We can make a quick change to this code to verify this is indeed what we are looking for, lets multiply the gTimeIncrement by 10:

```diff
if (IS_DAY || gTimeIncrement >= 0x190) {
-    gSaveContext.dayTime += gTimeIncrement;
+    gSaveContext.dayTime += gTimeIncrement * 10;
} else {
-    gSaveContext.dayTime += gTimeIncrement * 2; // time moves twice as fast at night
+    gSaveContext.dayTime += gTimeIncrement * 2 * 10; // time moves twice as fast at night
}
```

Rebuild the game and launch it, then load a save file. You should see that the time of day is now moving much faster. Terrific! While we could wrap this up and call it a day, we could make this user configurable by making a few more changes. I think a slider would be good for this, there's a slider in the cheat menu that we can use as a reference. Let's find it in `soh/soh/SohGui/SohMenuEnhancements.cpp` around line 1565:

```cpp
    AddWidget(path, "Hookshot Reach Multiplier: %.2fx", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_CHEAT("HookshotReachMultiplier"))
        .Options(FloatSliderOptions().Format("%.2f").Min(1.0f).Max(5.0f));
```
This adds a `Widget` which sets a CVar, which then sets the options of the slider. We'll make our minimum 0.2 to allow it to move slower, and our maximum 5.0 to allow it to move up to 5x faster. We'll also set the default to 1.0 so that it doesn't change the behavior by default. Copy this line and paste it below, then make the relevant changes:

```cpp
    AddWidget(path, "Time Multiplier: %.2fx", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_CHEAT("TimeOfDayMultiplier"))
        .Options(FloatSliderOptions().Format("%.2f").Min(0.2f).Max(5.0f).DefaultValue(1.0f));
```

Now we need to replace our hard coded values with the new variable. We can do this by replacing the `10` with a cvar call

```diff
if (IS_DAY || gTimeIncrement >= 0x190) {
-    gSaveContext.dayTime += gTimeIncrement * 10;
+    gSaveContext.dayTime += gTimeIncrement * CVarGetFloat(CVAR_CHEAT("TimeOfDayMultiplier"),1.0f);
} else {
-    gSaveContext.dayTime += gTimeIncrement * 2 * 10;
+   gSaveContext.dayTime += gTimeIncrement * 2 * CVarGetFloat(CVAR_CHEAT("TimeOfDayMultiplier"),1.0f);
}
```

After rebuilding and launching the game, you should now see a new slider in the cheat menu that allows you to change the speed of the day/night cycle. Nice!
If you're ever not sure about how something would be implemented, you can always look at external resources like the [Cloudmodding Wiki](https://wiki.cloudmodding.com/oot/Main_Page) to get a better perspective.
## Are you committed?

Now that we have made our changes, we need to commit them. First we need to add the files we changed to the staging area. We can do this with the following command:

```bash
git add .
```

This will add all the files we changed to the staging area. If you want to add specific files you can do so by replacing the `.` with the file path. For example, if we only wanted to add the `soh/soh/SohMenuBar.cpp` file we would do the following:

```bash
git add soh/soh/SohMenuBar.cpp
```

Now that we have added our files to the staging area, we need to commit them. We can do this with the following command:

```bash
git commit -m "Add time multiplier cheat"
```

You can verify everything was committed correctly by running the following command:

```bash
git status
```

Now push your changes to your fork with the following command:

```bash
git push origin <BRANCH NAME>
```

## Sharing the treasure

Now that you have made your changes, you can share them with the world! You can do this by creating a pull request to your own fork. You can navigate around in the Github UI to find this, or you can use the following replacing the relevant info:
```
https://github.com/<GITHUB USERNAME>/Shipwright/compare/develop...<BRANCH NAME>
```

From there you should see all of your changes listed, and a big green `Create pull request` button. You can fill out relevant information and a title and create the pull request. Once you have done this the CI will begin building distributables for your changes, and when they are ready they will be added to the bottom of your Pull request description! (See other PRs for examples)

Note: DO NOT MERGE THIS PULL REQUEST. You will want your fork's develop branch to stay in sync with the upstream develop branch. We will go over this in the next section, but all changes should stay on their own branches, with open PR's to continue generating distributables.

## Maintaining your fork'in mod

A month has passed, and new features have been added upstream that you want included in your distribution. You can do this by rebasing your branch on top of the upstream develop branch. You can do this with the following commands:

```bash
# This command will add the upstream repository as a remote, only needs to be done once
git remote add upstream https://github.com/HarbourMasters/Shipwright.git

git checkout develop
git pull upstream develop
git checkout <BRANCH NAME>
git pull origin develop --rebase
```

If you happen to run into merge conflicts, it is outside the scope of this tutorial to explain how to resolve them. If you want to abort the rebase you can run the following command:

```bash
git rebase --abort
```

Assuming all went well, you can now push your changes to your fork with the following command:

```bash
git push origin <BRANCH NAME> --force
```

## Combining multiple mods

You have been working on your mod for a while, and you want to combine it with another mod. You can do this by adding the other mod as a remote, and then merging it into your branch. You can do this with the following commands:

```bash
# This command will add the other repository as a remote, only needs to be done once
git remote add <MOD AUTHOR NAME> https://github.com/<MOD AUTHOR>/Shipwright.git

git checkout <YOUR BRANCH NAME>
git pull <MOD AUTHOR NAME> <MOD AUTHOR BRANCH NAME>
```

If you happen to run into merge conflicts, it is outside the scope of this tutorial to explain how to resolve them. If you want to abort the merge you can run the following command:

```bash
git merge --abort
```

Assuming all went well, you can now push your changes to your fork with the following command:

```bash
git push origin <YOUR BRANCH NAME>
```
