#ifndef SOHMENU_H
#define SOHMENU_H

#include <libultraship/libultraship.h>
#include "Menu.h"
#include <fast/backends/gfx_rendering_api.h>
#include "soh/cvar_prefixes.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include "z64.h"
}

#ifdef __cplusplus
extern "C" {
#endif
void enableBetaQuest();
void disableBetaQuest();
#ifdef __cplusplus
}
#endif

namespace SohGui {
static std::map<int32_t, const char*> languages = {
    { LANGUAGE_ENG, "English" },
    { LANGUAGE_GER, "German" },
    { LANGUAGE_FRA, "French" },
    { LANGUAGE_JPN, "Japanese" },
};

// ============================================================================
// ADR 0004 section 5 capability gating (#497 step 3)
// ============================================================================
// Section 5 is the rule that "an MM entry may only appear enabled when its TU
// links AND its registrar runs AND its hook type has a dispatch placed", and
// that everything else "appears disabled, with a reason string - never
// functional-looking". Before this, the only machinery for that lived inside the
// common-owned MM options pane's own descriptor table
// (ComboMMOptionDesc::liveness/disabledReason), which is correct for that pane
// and is not reusable SohMenu infrastructure - SohMenu's own `disabledMap`
// carried nothing but OoT/platform predicates.
//
// WHY A SIBLING REGISTRY AND NOT MORE `disabledMap` ROWS. `disabledMap` is
// keyed on uint32_t but the only way a widget names one of its rows is
// `WidgetInfo::activeDisables`, a `std::vector<DisableOption>` - and
// `DisableOption` is an unscoped enum with no fixed underlying type, so its
// value range is the smallest bit-field holding its enumerators (0..15 today).
// Casting a capability key into it is not merely ugly, it is out of range, and
// appending capability enumerators to the upstream enum instead would collide
// with every SoH sync. So capabilities live in their own registry of the same
// SHAPE - `disabledInfo`, i.e. a predicate paired with an explanation - and
// compose with `disabledMap` the way the dozens of existing PreFuncs that set
// `options->disabled`/`disabledTooltip` directly already do
// (SohMenuEnhancements.cpp, and the tier-4 rows' ComboRuleApplyDecided). When
// both fire, MenuDrawItem's race-lockout branch overwrites the tooltip, which is
// pre-existing behaviour for every such PreFunc and is why a state that must be
// legible without hovering also says so in the row NAME (see ApplyPresentation).
//
// WHY THE REGISTRY IS PROCESS-WIDE AND NOT A MEMBER. Two reasons, both
// load-bearing: a contributing TU's file-scope registrar runs before any SohMenu
// exists, and a row's PreFunc must resolve its capability in a display-free
// harness that never reaches Ship::Menu::InitElement (which touches window
// backends). GetCapabilityMap() is therefore a function-local static, like
// MenuInit's registries.
//
// WHY THE PREDICATES ARE OBSERVED FACTS, NOT A TRANSCRIBED LIST. #497 asks for
// "a manifest derived from the link, not hand-maintained", and the reason is the
// defect this whole mechanism exists to prevent: a reason string that outlives
// its blocker (twice already - #438's remainder, then #669). So
// SOH_MENU_CAP_MM_HOSTED asks the foreign-item pool registry whether MM's pool
// TU actually registered, which is #640's "observe the effect, never name the
// symbol" applied to MM's half; a test can un-register that pool and watch a
// gated row grey itself, which is the only way to prove the gate is not
// decorative.
typedef enum {
    /**
     * MM's half of the binary is present and its registrars ran. Observed
     * through the foreign-item pool registry: MM's
     * 2s2h/Rando/ForeignItemsSingleExe.cpp hands its static pool to
     * Combo_RegisterForeignItemPool from a file-scope initializer, so a
     * non-empty MM pool is section 5 parts 1 AND 2 for the WHOLE_ARCHIVE'd
     * `2ship_rando`, measured rather than assumed.
     *
     * It does NOT observe section 5 part 3 (a hook type's dispatch point). A row
     * whose behaviour rides a specific hook composes its own reason on top, the
     * way the MM options pane's per-option liveness does - gating on this
     * capability alone would be the "representative file" mistake section 5
     * warns about, one level up.
     */
    SOH_MENU_CAP_MM_HOSTED = 0,
    /**
     * src/common's combo settings surface is reachable in this process, i.e.
     * there is a CVar store to author into at all (a process that never
     * constructed a Ship::Context has none, and libultraship's C bridge
     * dereferences the singleton unconditionally).
     */
    SOH_MENU_CAP_COMBO_HOSTED,
    /** A paired OoT+MM world exists (gComboCtx reports a live pairing). */
    SOH_MENU_CAP_COMBO_PAIRED,
    /** This is the combined RedShipBlueShip executable, not a lone SoH build. */
    SOH_MENU_CAP_SINGLE_EXE,
    SOH_MENU_CAP_BUILTIN_COUNT
} SohMenuCapability;

/**
 * The first capability key an out-of-tree contributor may allocate. The API
 * below is typed on uint32_t rather than on SohMenuCapability precisely so a
 * caller can hold its own keys without casting into an enum whose value range
 * would not admit them.
 */
inline constexpr uint32_t kSohMenuCapabilityExternalFirst = 0x1000u;

// ============================================================================
// ADR 0004 section 4.2's marker and section 6's four presentation states
// (#497 step 5)
// ============================================================================
/**
 * The four states section 6 decides, in the order its table lists them. The ADR
 * is explicit that there are FOUR and that "the distinctions are what each one
 * denies", so this enum is closed: a fifth state would be a change to decided
 * text, not an addition here.
 *
 * The wave brief paraphrases them as "live / frozen-read-only post-creation /
 * partial-with-reason / not-available". The mapping, recorded so the two
 * vocabularies do not drift: "not-available" is SOH_MENU_PRESENT_CAPABILITY,
 * "frozen-read-only post-creation" is SOH_MENU_PRESENT_FROZEN, and
 * "partial-with-reason" is the same CAPABILITY state carrying a reason that
 * names which leg is inert - which is exactly how the common-owned MM options
 * pane already renders COMBO_MM_LIVENESS_PARTIAL (disabled, with the reason).
 */
typedef enum {
    /** Applies now, editable. Denies nothing, and must carry no gate reason. */
    SOH_MENU_PRESENT_LIVE = 0,
    /**
     * Section 6 points 2/3: belongs to the game that is currently suspended.
     * Still READABLE AND EDITABLE - collapsing it into "disabled" would wrongly
     * imply the setting is broken - but labelled with its state so it does not
     * imply immediate effect.
     */
    SOH_MENU_PRESENT_INACTIVE_GAME,
    /** Section 5: the capability is absent. Disabled, with the reason. Denies
     *  "this works". */
    SOH_MENU_PRESENT_CAPABILITY,
    /**
     * Section 6 state 4 (#564): a world built from this key already exists.
     * Disabled, with the FREEZE reason - never the capability reason, because
     * "not yet available" sends a player hunting for a missing feature while
     * "already decided" tells them the choice was made. Denies "this is still a
     * choice".
     */
    SOH_MENU_PRESENT_FROZEN,
    SOH_MENU_PRESENT_COUNT
} SohMenuPresentation;

class SohMenu;

// ============================================================================
// The tier-4 Combo section's extension point (#497 step 6)
// ============================================================================
/** A Combo-section page contributed by another translation unit. */
using ComboPageRegistrar = void (*)(SohMenu& menu, WidgetPath& path);

/** One contributed page: its sidebar label, how many columns it draws, and the
 *  registrar that fills it. */
struct ComboSectionPage {
    std::string sidebarName;
    uint32_t columnCount;
    ComboPageRegistrar registrar;
};

/**
 * THE EXTENSION POINT. A TU that wants its own page in the tier-4 Combo section
 * registers one here instead of editing SohMenuCombo.cpp; AddMenuCombo() walks
 * the registry after its own pages and creates each sidebar, so a contributor
 * touches no file this lane owns. Registration must happen BEFORE
 * AddMenuElements() runs - a file-scope initializer is the intended shape:
 *
 *     static void AddMyPage(SohGui::SohMenu& menu, WidgetPath& path) {
 *         menu.AddWidget(path, "My Toggle", WIDGET_CVAR_CHECKBOX).CVar("gEnhancements.Foo");
 *     }
 *     static SohGui::RegisterComboSectionPage_t myPage("My Page", 1, AddMyPage);
 *
 * TWO THINGS THAT WILL BITE, both measured in this tree rather than imagined:
 *
 *  1. A file-scope registrar in a plain (non-WHOLE_ARCHIVE) archive is DROPPED
 *     by the linker when nothing else references the TU - #516/#640's class.
 *     All three OoT archives (soh_enh, soh_rando, soh_port) are WHOLE_ARCHIVE'd
 *     since #341/#640, so a contributor under games/oot/soh is safe; anything
 *     else must prove its archive is.
 *  2. Register a page only if it will hold at least one widget. An empty
 *     multi-column page is #640's failure mode exactly: Menu::DrawElement's
 *     unconditional SetNextWindowPos goes unconsumed and undocks libultraship's
 *     "Main Game" window.
 *
 * A second registration under the same sidebar name replaces the first and
 * complains on stderr - two registrars claiming one page is the ambiguity this
 * registry would otherwise hide.
 */
void RegisterComboSectionPage(const char* sidebarName, uint32_t columnCount, ComboPageRegistrar registrar);

/** Drop a contributed page again. Returns true if one was removed. Exists so a
 *  test can restore the registry rather than leave process-global state behind
 *  (the Combo_RegisterForeignItemPool(NULL, 0) precedent). */
bool UnregisterComboSectionPage(const char* sidebarName);

/** The contributed pages, in registration order. */
const std::vector<ComboSectionPage>& GetComboSectionPages();

class SohMenu : public Ship::Menu {
  public:
    SohMenu(const std::string& consoleVariable, const std::string& name);

    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override;
    void Draw() override;

    void AddSidebarEntry(std::string sectionName, std::string sidbarName, uint32_t columnCount);
    WidgetInfo& AddWidget(WidgetPath& pathInfo, std::string widgetName, WidgetType widgetType);
    void AddMenuElements();
    void AddMenuSettings();
    void AddMenuEnhancements();
    void AddMenuDevTools();
    void AddMenuRandomizer();
    void AddMenuCombo();
    void AddMenuNetwork();
    static void UpdateLanguageMap(std::map<int32_t, const char*>& languageMap);

    // ---- step 3: capability gating -------------------------------------
    /** The capability registry. Keyed by SohMenuCapability (or a caller's own
     *  key at or above kSohMenuCapabilityExternalFirst). */
    static std::unordered_map<uint32_t, disabledInfo>& GetCapabilityMap();

    /**
     * Publish a capability. @p evaluation must be an OBSERVED fact - a registry
     * lookup, a model accessor - never a transcribed claim, and @p reason must
     * name the issue that tracks the absence (the gating lock refuses a reason
     * with no `#NNN` in it, because a reason that cannot be traced is a reason
     * nobody retires). A re-registration of the same key replaces the previous
     * entry.
     */
    static void RegisterCapability(uint32_t key, DisableInfoFunc evaluation, const char* reason);

    /** Is @p key's capability present right now? An UNREGISTERED key reads
     *  ABSENT: the honest answer for "a row asked for something nothing
     *  publishes" is to grey the row, not to let it look live. */
    static bool CapabilityPresent(uint32_t key);

    /** @p key's reason string, or "" for a key nothing registered. Never NULL. */
    static const char* CapabilityReason(uint32_t key);

    /** Apply @p key's gate to @p info once. Call from a PreFunc: MenuDrawItem
     *  runs ResetDisables() first, so a gate applied anywhere else is cleared
     *  before it is drawn. */
    static void ApplyCapabilityGate(WidgetInfo& info, uint32_t key);

    /** A ready-made PreFunc that gates on @p key. */
    static WidgetFunc CapabilityGate(uint32_t key);

    /** The same, composing with a row's own PreFunc: @p chained runs FIRST (so
     *  it may stage values or hide the row), then the gate, so the gate always
     *  has the last word on `disabled`. */
    static WidgetFunc CapabilityGate(uint32_t key, WidgetFunc chained);

    // ---- step 5: the marker and the four presentation states -----------
    /**
     * ADR 0004 section 4.2's persistent "applies to both games" marker.
     * Delegates to src/common's Combo_ComboSettingSharedMarker() rather than
     * spelling the badge again: the tier-4 rows already carry that string, and
     * two definitions of one requirement is how a marker ends up present on half
     * a menu.
     */
    static const char* SharedIntentMarker();

    /** Is @p cVar a class-(S) shared-intent key (RSBS::kSharedIntentKeys)? An
     *  entry ending in '.' is a shared macro PREFIX and matches by prefix. */
    static bool IsSharedIntentKey(const char* cVar);

    /** @p base with the marker prefixed iff @p cVar is shared-intent, and
     *  unchanged if it already leads with the marker. */
    static std::string MarkRowName(const std::string& base, const char* cVar);

    /**
     * THE MANIFEST-DRIVEN PASS. Walks every registered row of every section and
     * prefixes the marker to the ones bound to a shared-intent key. Section 4.2
     * is "a hard requirement on the widget, not a tooltip nicety", and the key
     * set is already checked in, so the marker is derived from the manifest
     * rather than hand-tagged per widget - which is what makes it impossible for
     * a new shared-intent row to ship unmarked. Idempotent.
     * @return how many rows were marked by this call.
     */
    int ApplySharedIntentMarkers();

    /**
     * Render @p info in section 6 state @p state, from @p baseName.
     *
     * The base name is passed in rather than remembered because the state can
     * change between frames and the suffix must not accumulate - the same reason
     * the tier-4 status row rebuilds its name from scratch every frame.
     *
     * @p reason is the explanation for the two disabled states and the label for
     * SOH_MENU_PRESENT_INACTIVE_GAME; it must be NULL for
     * SOH_MENU_PRESENT_LIVE. Two ADR rules are enforced here rather than
     * trusted: a LIVE row given a reason drops it (a live control that explains
     * why it is unavailable looks broken and is not), and a FROZEN row given a
     * registered CAPABILITY reason drops it for the canonical freeze label -
     * section 6's "a frozen entry's reason is not optional and is not the
     * capability reason". Both refusals log.
     */
    static void ApplyPresentation(WidgetInfo& info, const std::string& baseName, SohMenuPresentation state,
                                  const char* reason);

    /** The canonical label for @p state - "" for LIVE, which is never labelled. */
    static const char* PresentationLabel(SohMenuPresentation state);

  private:
    char mGitCommitHashTruncated[8];
    bool mIsTaggedVersion;
    bool mMenuElementsInitialized = false;
};

/** File-scope helper for RegisterComboSectionPage; see its doc comment. */
struct RegisterComboSectionPage_t {
    RegisterComboSectionPage_t(const char* sidebarName, uint32_t columnCount, ComboPageRegistrar registrar) {
        RegisterComboSectionPage(sidebarName, columnCount, registrar);
    }
};
} // namespace SohGui

#endif // SOHMENU_H
