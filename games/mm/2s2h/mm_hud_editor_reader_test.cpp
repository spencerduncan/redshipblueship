/**
 * @file mm_hud_editor_reader_test.cpp
 * @brief ROM-free lock for W6-HUD-A (#1239): Majora's Mask's HUD draws honour the HUD placement keys the shared menu
 *        writes, and the section's presets write them.
 *
 * CTest row MMHudEditorReader in CMake/SingleExecutable.cmake, dispatch "mm-hud-editor-reader" in
 * src/common/test_runner.cpp.
 *
 * THE DEFECT. 2Ship's HUD editor (2s2h/BenGui/HudEditor.cpp) is excluded from the single exe with the rest of BenGui.
 * Every MM HUD draw (z_parameter.c, z_map_disp.c, the kaleido equip animation) asks HudEditor_ShouldOverrideDraw and
 * the HudEditor_Modify* helpers before it draws, and the linked stand-ins in mm_hudeditor_stubs.c answered "vanilla"
 * to all of it: no MM HUD element could be moved, scaled or hidden, whatever a key held.
 *
 * WHY IT LOCKS ROM-FREE. The reader is pure: it reads the active element's keys and rewrites the rectangle values its
 * caller hands it. The row drives it with plain integers, never a display list. Every mode it drives is "Original
 * position", "Hidden" or "No anchors" (MM's Movable (Align Center)): the two anchored modes ask the render window for
 * its edges (OTRGetRectDimensionFrom*Edge), which the display-free tier does not have.
 *
 * WHAT IT ASSERTS:
 *   1. The B button with its Mode at "No anchors", a position and a scale: ShouldOverrideDraw is true, the element is
 *      not hidden, its scale is the key's, and HudEditor_ModifyDrawValues moves the rectangle to the key's position
 *      (the offset from the vanilla position scaled), scales its size and divides its texture steps.
 *   2. The same element at "Hidden": overridden and hidden. At "Original position" (the key cleared): not overridden.
 *   2b. W6-HUD-B: the hearts at "No anchors" are moved and scaled the same way, and each of W6-HUD-B's ten elements
 *      (carrots, clock, hearts, magic, both timers, the minigame, rupee, key and skulltula counters) hides at "Hidden".
 *   3. HudEditor_OverrideNextElementMode(Vanilla), which Better Song of Double Time uses while it draws the clock,
 *      wins over a non-vanilla key; and no element active draws vanilla.
 *   4. An element with no row in the shared table (none since W6-HUD-B; the leg then checks nothing) draws vanilla
 *      even under a non-vanilla override: the reader never reads a key the shared menu does not write.
 *   5. The presets: Hidden sets every table element Hidden, Widescreen gives each its widescreen mode, Vanilla clears
 *      every mode, and each clears every element's position and scale.
 * RED without the fix: leg 1 (the stand-in answers "vanilla" and leaves the rectangle as it was).
 * RED without W6-HUD-B's table rows: leg 2b (an element with no row draws vanilla whatever its keys hold).
 *
 * Clears every key it set and leaves no element active and no override set, on every exit.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
extern "C" int MM_HudEditorReader_RunHeadless(void) {
    printf("[TEST] mm-hud-editor-reader: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <cmath>

#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/BenGui/HudEditor.h"
#include "cvar_shared_keys.h"       // src/common
#include "mm_hud_editor_elements.h" // src/common

#include <string>

namespace {

constexpr int32_t kUnset = -12345;

bool Near(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

std::string Key(const char* element, const char* field) {
    return std::string(RSBS_CVAR_MM_HUD_EDITOR_PREFIX) + element + field;
}

/** Clear all four keys of every element in the shared table, and leave no element active and no override set. */
void ClearAll() {
#define RSBS_HUD_TEST_CLEAR(id, key, label, x, y, ws) \
    CVarClear(Key(key, ".Mode").c_str());             \
    CVarClear(Key(key, ".Position.X").c_str());       \
    CVarClear(Key(key, ".Position.Y").c_str());       \
    CVarClear(Key(key, ".Scale").c_str());
    RSBS_MM_HUD_EDITOR_ELEMENTS(RSBS_HUD_TEST_CLEAR)
#undef RSBS_HUD_TEST_CLEAR
    HudEditor_OverrideNextElementMode(HUD_EDITOR_ELEMENT_MODE_NONE);
    HudEditor_SetActiveElement(HUD_EDITOR_ELEMENT_NONE);
}

int Fail(const char* what) {
    printf("[TEST] FAIL: %s\n", what);
    return 1;
}

} // namespace

extern "C" int MM_HudEditorReader_RunHeadless(void) {
    printf("[TEST] mm-hud-editor-reader: MM's HUD draws honour the shared menu's HUD placement keys, and its presets "
           "write them (#1239)\n");
    ClearAll();
    int failures = 0;

    // Leg 1: the B button at "No anchors", moved to (100, 50) at twice its size.
    const std::string bMode = Key("B", ".Mode");
    const std::string bX = Key("B", ".Position.X");
    const std::string bY = Key("B", ".Position.Y");
    const std::string bScale = Key("B", ".Scale");
    CVarSetInteger(bMode.c_str(), HUD_EDITOR_ELEMENT_MODE_MOVABLE_43);
    CVarSetInteger(bX.c_str(), 100);
    CVarSetInteger(bY.c_str(), 50);
    CVarSetFloat(bScale.c_str(), 2.0f);
    HudEditor_SetActiveElement(HUD_EDITOR_ELEMENT_B);
    if (!HudEditor_ShouldOverrideDraw()) {
        failures += Fail("with gHudEditor.B.Mode at \"No anchors\" the B button's draw is not overridden: MM's HUD "
                         "ignores the shared menu's placement keys");
    }
    if (HudEditor_IsActiveElementHidden()) {
        failures += Fail("the B button reads as hidden at \"No anchors\"");
    }
    if (!Near(HudEditor_GetActiveElementScale(), 2.0f)) {
        printf("[TEST]       scale %.3f\n", HudEditor_GetActiveElementScale());
        failures += Fail("the B button's scale is not gHudEditor.B.Scale (2.0)");
    }
    // A rectangle 10 px right of and 4 px below the B button's vanilla anchor (167, 17), 16 px square.
    s16 left = 167 + 10;
    s16 top = 17 + 4;
    s16 width = 16;
    s16 height = 16;
    s16 dsdx = 1024;
    s16 dtdy = 1024;
    HudEditor_ModifyDrawValues(&left, &top, &width, &height, &dsdx, &dtdy);
    if (left != 100 + 10 * 2 || top != 50 + 4 * 2 || width != 32 || height != 32 || dsdx != 512 || dtdy != 512) {
        printf("[TEST]       rect (%d, %d) %dx%d steps %d/%d, expected (120, 58) 32x32 steps 512/512\n", left, top,
               width, height, dsdx, dtdy);
        failures += Fail("HudEditor_ModifyDrawValues did not move the B button to its keys' position and scale");
    }
    printf("[TEST] leg 1: B at \"No anchors\" is overridden and drawn at its keys' position and scale\n");

    // Leg 2: Hidden, then Original position.
    CVarSetInteger(bMode.c_str(), HUD_EDITOR_ELEMENT_MODE_HIDDEN);
    if (!HudEditor_ShouldOverrideDraw() || !HudEditor_IsActiveElementHidden()) {
        failures += Fail("with gHudEditor.B.Mode at \"Hidden\" the B button is not overridden and hidden");
    }
    CVarClear(bMode.c_str());
    if (HudEditor_ShouldOverrideDraw() || HudEditor_IsActiveElementHidden()) {
        failures += Fail("with gHudEditor.B.Mode cleared (Original position) the B button is still overridden");
    }
    printf("[TEST] leg 2: \"Hidden\" hides the B button and \"Original position\" draws it vanilla\n");

    // Leg 2b, W6-HUD-B (#1239): the other ten elements. Hearts at "No anchors", moved to (60, 40) at one and a half
    // times its size, is overridden and drawn at its keys' position and scale; then each of the ten, Hearts included,
    // is overridden and hidden at "Hidden", and drawn vanilla again once its Mode key is cleared.
    {
        const std::string hMode = Key("Hearts", ".Mode");
        CVarSetInteger(hMode.c_str(), HUD_EDITOR_ELEMENT_MODE_MOVABLE_43);
        CVarSetInteger(Key("Hearts", ".Position.X").c_str(), 60);
        CVarSetInteger(Key("Hearts", ".Position.Y").c_str(), 40);
        CVarSetFloat(Key("Hearts", ".Scale").c_str(), 1.5f);
        HudEditor_SetActiveElement(HUD_EDITOR_ELEMENT_HEARTS);
        if (!HudEditor_ShouldOverrideDraw() || HudEditor_IsActiveElementHidden()) {
            failures += Fail("with gHudEditor.Hearts.Mode at \"No anchors\" the hearts' draw is not overridden (or "
                             "reads as hidden): MM's hearts ignore the shared menu's placement keys");
        }
        // A rectangle 10 px right of and 4 px below the hearts' vanilla anchor (30, 26), 16 px square.
        s16 hLeft = 30 + 10;
        s16 hTop = 26 + 4;
        s16 hWidth = 16;
        s16 hHeight = 16;
        s16 hDsdx = 1024;
        s16 hDtdy = 1024;
        HudEditor_ModifyDrawValues(&hLeft, &hTop, &hWidth, &hHeight, &hDsdx, &hDtdy);
        if (hLeft != 60 + 15 || hTop != 40 + 6 || hWidth != 24 || hHeight != 24 || hDsdx != 682 || hDtdy != 682) {
            printf("[TEST]       rect (%d, %d) %dx%d steps %d/%d, expected (75, 46) 24x24 steps 682/682\n", hLeft, hTop,
                   hWidth, hHeight, hDsdx, hDtdy);
            failures += Fail("HudEditor_ModifyDrawValues did not move the hearts to their keys' position and scale");
        }
        struct HudBElement {
            HudEditorElementID id;
            const char* key;
        };
        const HudBElement kHudB[] = {
            { HUD_EDITOR_ELEMENT_CARROT, "Carrots" },
            { HUD_EDITOR_ELEMENT_CLOCK, "Clock" },
            { HUD_EDITOR_ELEMENT_HEARTS, "Hearts" },
            { HUD_EDITOR_ELEMENT_MAGIC_METER, "Magic" },
            { HUD_EDITOR_ELEMENT_TIMERS, "Timers" },
            { HUD_EDITOR_ELEMENT_TIMERS_MOON_CRASH, "SkullKidTimer" },
            { HUD_EDITOR_ELEMENT_MINIGAME_COUNTER, "Minigames" },
            { HUD_EDITOR_ELEMENT_RUPEE_COUNTER, "Rupees" },
            { HUD_EDITOR_ELEMENT_KEY_COUNTER, "Keys" },
            { HUD_EDITOR_ELEMENT_SKULLTULA_COUNTER, "Skulltulas" },
        };
        for (const HudBElement& element : kHudB) {
            const std::string mode = Key(element.key, ".Mode");
            CVarSetInteger(mode.c_str(), HUD_EDITOR_ELEMENT_MODE_HIDDEN);
            HudEditor_SetActiveElement(element.id);
            if (!HudEditor_ShouldOverrideDraw() || !HudEditor_IsActiveElementHidden()) {
                printf("[TEST]       %s\n", mode.c_str());
                failures += Fail("an element's Mode key at \"Hidden\" does not hide its draw");
            }
            CVarClear(mode.c_str());
            if (HudEditor_ShouldOverrideDraw()) {
                printf("[TEST]       %s\n", mode.c_str());
                failures += Fail("an element with its Mode key cleared is still overridden");
            }
        }
        // The ten elements' keys are cleared by name: until their table rows exist, ClearAll does not know them.
        for (const HudBElement& element : kHudB) {
            CVarClear(Key(element.key, ".Mode").c_str());
            CVarClear(Key(element.key, ".Position.X").c_str());
            CVarClear(Key(element.key, ".Position.Y").c_str());
            CVarClear(Key(element.key, ".Scale").c_str());
        }
        // Leg 3 continues on the B button.
        HudEditor_SetActiveElement(HUD_EDITOR_ELEMENT_B);
    }
    printf("[TEST] leg 2b: the hearts at \"No anchors\" are drawn at their keys' position and scale, and the carrots, "
           "clock, hearts, magic, timers and counters hide at \"Hidden\"\n");

    // Leg 3: the override wins, and no active element draws vanilla.
    CVarSetInteger(bMode.c_str(), HUD_EDITOR_ELEMENT_MODE_MOVABLE_43);
    HudEditor_OverrideNextElementMode(HUD_EDITOR_ELEMENT_MODE_VANILLA);
    if (HudEditor_ShouldOverrideDraw()) {
        failures += Fail("HudEditor_OverrideNextElementMode(Vanilla) did not win over gHudEditor.B.Mode");
    }
    HudEditor_OverrideNextElementMode(HUD_EDITOR_ELEMENT_MODE_NONE);
    HudEditor_SetActiveElement(HUD_EDITOR_ELEMENT_NONE);
    if (HudEditor_ShouldOverrideDraw() || !Near(HudEditor_GetActiveElementScale(), 1.0f)) {
        failures += Fail("with no element active the draw is overridden or scaled");
    }
    printf("[TEST] leg 3: the next-element override wins, and no active element draws vanilla\n");

    // Leg 4: an element with no row in the shared table draws vanilla, even under a non-vanilla override.
    int rowless = HUD_EDITOR_ELEMENT_NONE;
    for (int i = HUD_EDITOR_ELEMENT_B; i < HUD_EDITOR_ELEMENT_MAX; i++) {
        if (hudEditorElements[i].modeCvar == nullptr) {
            rowless = i;
            break;
        }
    }
    if (rowless != HUD_EDITOR_ELEMENT_NONE) {
        HudEditor_SetActiveElement((HudEditorElementID)rowless);
        HudEditor_OverrideNextElementMode(HUD_EDITOR_ELEMENT_MODE_HIDDEN);
        if (HudEditor_ShouldOverrideDraw() || HudEditor_IsActiveElementHidden()) {
            printf("[TEST]       element %d\n", rowless);
            failures += Fail("an element with no row in the shared table is overridden: its draw sites would read "
                             "keys nothing writes");
        }
        HudEditor_OverrideNextElementMode(HUD_EDITOR_ELEMENT_MODE_NONE);
        HudEditor_SetActiveElement(HUD_EDITOR_ELEMENT_NONE);
        printf("[TEST] leg 4: element %d, which has no row, draws vanilla\n", rowless);
    } else {
        printf("[TEST] leg 4: every element has a row; nothing to check\n");
    }

    // Leg 5: the presets, over every element in the table.
    CVarSetInteger(bX.c_str(), 5);
    CVarSetFloat(bScale.c_str(), 3.0f);
    MM_HudEditor_ApplyPreset(RSBS_MM_HUD_PRESET_HIDDEN);
#define RSBS_HUD_TEST_EXPECT_MODE(id, key, label, x, y, ws, want)                        \
    do {                                                                                 \
        const int32_t mode = CVarGetInteger(Key(key, ".Mode").c_str(), kUnset);          \
        if (mode != (want)) {                                                            \
            printf("[TEST]       %s: mode %d, expected %d\n", label, mode, (int)(want)); \
            failures += Fail("a HUD preset did not set an element's mode");              \
        }                                                                                \
        if (CVarGetInteger(Key(key, ".Position.X").c_str(), kUnset) != kUnset ||         \
            CVarGetInteger(Key(key, ".Position.Y").c_str(), kUnset) != kUnset ||         \
            CVarGetFloat(Key(key, ".Scale").c_str(), -1.0f) != -1.0f) {                  \
            printf("[TEST]       %s\n", label);                                          \
            failures += Fail("a HUD preset left an element's position or scale set");    \
        }                                                                                \
    } while (0);
#define RSBS_HUD_TEST_HIDDEN(id, key, label, x, y, ws) \
    RSBS_HUD_TEST_EXPECT_MODE(id, key, label, x, y, ws, HUD_EDITOR_ELEMENT_MODE_HIDDEN)
#define RSBS_HUD_TEST_WIDESCREEN(id, key, label, x, y, ws) RSBS_HUD_TEST_EXPECT_MODE(id, key, label, x, y, ws, ws)
#define RSBS_HUD_TEST_VANILLA(id, key, label, x, y, ws) RSBS_HUD_TEST_EXPECT_MODE(id, key, label, x, y, ws, kUnset)
    RSBS_MM_HUD_EDITOR_ELEMENTS(RSBS_HUD_TEST_HIDDEN)
    CVarSetInteger(bY.c_str(), 7);
    MM_HudEditor_ApplyPreset(RSBS_MM_HUD_PRESET_WIDESCREEN);
    RSBS_MM_HUD_EDITOR_ELEMENTS(RSBS_HUD_TEST_WIDESCREEN)
    CVarSetFloat(bScale.c_str(), 0.5f);
    MM_HudEditor_ApplyPreset(RSBS_MM_HUD_PRESET_VANILLA);
    RSBS_MM_HUD_EDITOR_ELEMENTS(RSBS_HUD_TEST_VANILLA)
#undef RSBS_HUD_TEST_VANILLA
#undef RSBS_HUD_TEST_WIDESCREEN
#undef RSBS_HUD_TEST_HIDDEN
#undef RSBS_HUD_TEST_EXPECT_MODE
    printf("[TEST] leg 5: the Hidden, Widescreen and Vanilla presets write every element's mode and clear its "
           "position and scale\n");

    ClearAll();
    printf("[TEST] mm-hud-editor-reader: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}

#endif // RSBS_SINGLE_EXECUTABLE
