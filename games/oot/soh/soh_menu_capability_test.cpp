/**
 * @file soh_menu_capability_test.cpp
 * @brief ROM-free, display-free lock for #497 steps 3 and 5: SohMenu's
 *        capability gating (ADR 0004 §5) and the §4.2 shared-intent marker with
 *        §6's four presentation states.
 *
 * CTest row MenuCapabilityGating in CMake/SingleExecutable.cmake, dispatch
 * "menu-capability-gating" in src/common/test_runner.cpp. This is the lock #497
 * named `menu-capability-gating` and recorded as not existing.
 *
 * WHAT WOULD MAKE THIS ROW WORTHLESS, and what each leg does about it.
 *
 *  1. A GATE WHOSE PREDICATE IS A CONSTANT. ADR 0004 §5 exists to stop "a
 *     control that flips a CVar and changes nothing"; a gate that can only ever
 *     report PRESENT is the same defect one level up, and it passes every test
 *     that checks only the enabled case. So leg 2 drives a REAL capability
 *     through both answers: SOH_MENU_CAP_MM_HOSTED observes whether MM's item
 *     pool registered, and the leg un-registers that pool
 *     (Combo_RegisterForeignItemPool(GAME_MM, NULL, 0), the documented
 *     test-only inverse) to watch a row grey ITSELF, then puts the pool back and
 *     watches the row recover. Neither half is asserted without the other:
 *     red-then-green in one process, over production code, with no synthetic
 *     predicate involved.
 *
 *  2. A STALE REASON. Twice now a disabled-with-reason row outlived its blocker
 *     (#438's remainder, then #669, re-measured by PR #677). A reason nobody can
 *     trace back to a tracker is a reason nobody retires, so leg 1 refuses a
 *     built-in capability whose reason carries no `#NNN`.
 *
 *  3. A MARKER THE PASS DOES NOT ACTUALLY REACH. §4.2 is "a hard requirement on
 *     the widget, not a tooltip nicety", and the failure mode of a
 *     manifest-driven pass is that it works on a synthetic row set and misses
 *     the real menu. So leg 6 runs the production pass over a production
 *     section: `AddMenuDevTools()`, the one SohMenu section that registers
 *     ROM-free (every CVar read and all four `OoT_gPlayState` touches in it are
 *     inside PreFunc lambdas) and that binds genuine `kSharedIntentKeys`
 *     entries. The assertion is exhaustive over that section rather than a spot
 *     check: every row bound to a shared-intent key leads with the marker, and
 *     every row that is not, does not.
 *
 *  4. A PRESENTATION THAT ACCUMULATES. `ApplyPresentation` writes the state into
 *     the row NAME (§4.2's "legible without hovering", and because MenuDrawItem's
 *     race-lockout branch overwrites `disabledTooltip` outright). A PreFunc runs
 *     every frame and `WidgetInfo::ResetDisables()` does not clear `name`, so a
 *     naive version compounds — "Row - not yet available: ..." becomes that twice,
 *     then three times — and a row that recovers keeps its stale label forever.
 *     Leg 5 applies the same state repeatedly and asserts exactly one suffix,
 *     then walks every transition between the four states and asserts the name
 *     ends up as if only the last one had been applied.
 *
 * HOW IT OBSERVES. A derived probe reaches `Ship::Menu`'s protected
 * `menuEntries` (no production header grows a test-only reader), constructed with
 * an EMPTY visibility CVar so `GuiWindow`'s ctor never reaches the Ship::Context
 * latch. Rows are driven the way `Menu::MenuDrawItem` does — `ResetDisables()`,
 * then `preFunc` — which is possible because a gate touches no ImGui. Row
 * pointers are resolved by NAME after every registration is done: `AddWidget`
 * push_backs into a `std::vector<WidgetInfo>`, so a reference taken before a
 * later AddWidget can be left dangling by a reallocation.
 *
 * WHAT IS NOT COVERED, named rather than implied. That `AddMenuElements()` runs
 * the marker pass over EVERY section is not asserted here, because
 * `AddMenuRandomizer()` cannot run ROM-free at all (its five `Rando::Settings`
 * option groups reach `Option::AddWidget`, which runs each option's callback at
 * registration time and dereferences a null `gRandoContext`). Leg 6 proves the
 * pass works on a real section; that the call site covers the rest is a one-line
 * fact in `AddMenuElements()` and an operator nightly check. Appearance —
 * whether the badge reads well, whether a greyed row is legible — is not
 * testable headless and is not claimed.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/SohGui/SohMenu.h"

#include <cstdio>
#include <string>
#include <vector>

#include "combo_settings_view.h"
#include "cvar_shared_keys.h"
#include "foreign_items.h"
#include "game.h"

namespace {

int gFailures = 0;

#define CAP_CHECK(cond, ...)                                               \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            printf("[TEST]       ");                                       \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
            gFailures++;                                                   \
        }                                                                  \
    } while (0)

/** The probe: a real SohMenu whose pages this file can read. */
class CapabilityMenuProbe final : public SohGui::SohMenu {
  public:
    CapabilityMenuProbe() : SohGui::SohMenu("", "Menu Capability Probe") {
    }

    std::unordered_map<std::string, MainMenuEntry>& Entries() {
        return menuEntries;
    }
};

/** Drive one row the way Menu::MenuDrawItem does, minus the drawing. */
void RunPreFunc(WidgetInfo& row) {
    row.ResetDisables();
    if (row.preFunc != nullptr) {
        row.preFunc(row);
    }
}

/** How many times @p needle occurs in @p haystack. */
int CountOf(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return 0;
    }
    int n = 0;
    for (std::size_t at = haystack.find(needle); at != std::string::npos; at = haystack.find(needle, at + 1)) {
        n++;
    }
    return n;
}

const char* Tooltip(const WidgetInfo& row) {
    if (row.options == nullptr || row.options->disabledTooltip == nullptr) {
        return "";
    }
    return row.options->disabledTooltip;
}

/** The row named @p name on @p page, or nullptr. */
WidgetInfo* FindRow(SidebarEntry& page, const std::string& name) {
    for (auto& column : page.columnWidgets) {
        for (WidgetInfo& row : column) {
            if (row.name == name) {
                return &row;
            }
        }
    }
    return nullptr;
}

const char* PresentLabel(SohGui::SohMenuPresentation state) {
    return SohGui::SohMenu::PresentationLabel(state);
}

// Test-only capability keys, above kSohMenuCapabilityExternalFirst -- which is
// exactly what that constant is for, so they can never collide with a built-in.
constexpr uint32_t kSyntheticKey = SohGui::kSohMenuCapabilityExternalFirst + 0x101u;
constexpr uint32_t kUnregisteredKey = SohGui::kSohMenuCapabilityExternalFirst + 0x7770u;
constexpr uint32_t kRefusedKey = SohGui::kSohMenuCapabilityExternalFirst + 0x7771u;

bool gSyntheticAbsent = false;

bool SyntheticAbsent(disabledInfo& info) {
    (void)info;
    return gSyntheticAbsent;
}

constexpr const char* kSyntheticReason = "Not yet available: a synthetic capability owned by this test (#497)";

int gChainedRan = 0;
int gChainedSawDisabled = 0;

bool gMmGatedValue = false;
bool gSyntheticGatedValue = false;
bool gUnregisteredGatedValue = false;
bool gChainedValue = false;
bool gHiddenValue = false;
bool gStateValue = false;

} // namespace

extern "C" int OoT_MenuCapabilityGating_RunHeadless(void) {
    printf("[TEST] menu-capability-gating: SohMenu capability gating (ADR 0004 §5) and the §4.2 marker with §6's "
           "four presentation states (#497 steps 3 and 5)\n");

    gFailures = 0;

    // ---- Leg 1: every built-in capability is published and traceable --------
    // An UNREGISTERED key reads ABSENT by design, so a capability that failed to
    // install would grey its rows rather than crash -- honest, and invisible to
    // any test that does not look. This is the look.
    for (uint32_t key = 0; key < (uint32_t)SohGui::SOH_MENU_CAP_BUILTIN_COUNT; key++) {
        const std::string reason = SohGui::SohMenu::CapabilityReason(key);
        CAP_CHECK(!reason.empty(), "built-in capability %u publishes no reason; a row gated on it would grey itself "
                                   "with an empty tooltip, which reads as a bug in the menu",
                  key);
        bool namesAnIssue = false;
        for (std::size_t i = 0; i + 1 < reason.size(); i++) {
            if (reason[i] == '#' && reason[i + 1] >= '0' && reason[i + 1] <= '9') {
                namesAnIssue = true;
                break;
            }
        }
        CAP_CHECK(namesAnIssue,
                  "built-in capability %u's reason names no issue (#NNN): \"%s\". Two disabled-with-reason rows have "
                  "already outlived their blocker (#438's remainder, then #669); a reason nobody can trace is a "
                  "reason nobody retires",
                  key, reason.c_str());
    }
    CAP_CHECK(SohGui::SohMenu::GetCapabilityMap().size() >= (std::size_t)SohGui::SOH_MENU_CAP_BUILTIN_COUNT,
              "the capability registry holds %zu entries, fewer than the %d built-ins",
              SohGui::SohMenu::GetCapabilityMap().size(), (int)SohGui::SOH_MENU_CAP_BUILTIN_COUNT);
    printf("[TEST] leg 1: all %d built-in capabilities publish a reason that names an issue\n",
           (int)SohGui::SOH_MENU_CAP_BUILTIN_COUNT);

    // ---- The synthetic page every later leg draws on ------------------------
    // The synthetic capability is registered BEFORE the rows, because a
    // CapabilityGate PreFunc resolves its key every frame and an unregistered key
    // reads ABSENT -- registering afterwards would work, but the order here is
    // the one a contributing TU's file-scope registrar actually has.
    SohGui::SohMenu::RegisterCapability(kSyntheticKey, SyntheticAbsent, kSyntheticReason);

    const std::string kMmRowBase = "Needs Majora's Mask";
    const std::string kSynRowBase = "Needs The Synthetic Capability";
    const std::string kUnregRowBase = "Needs Something Nothing Publishes";
    const std::string kChainedRowBase = "Chained Gate";
    const std::string kHiddenRowBase = "Hidden Then Gated";
    const std::string kStateRowBase = "Presentation States";

    CapabilityMenuProbe probe;
    probe.AddMenuEntry("CapProbe", "gSettings.Menu.CapProbeSidebarSection");
    probe.AddSidebarEntry("CapProbe", "Gates", 1);
    WidgetPath path = { "CapProbe", "Gates", SECTION_COLUMN_1 };

    probe.AddWidget(path, kMmRowBase, WIDGET_CHECKBOX)
        .ValuePointer(&gMmGatedValue)
        .PreFunc(SohGui::SohMenu::CapabilityGate(SohGui::SOH_MENU_CAP_MM_HOSTED));
    probe.AddWidget(path, kSynRowBase, WIDGET_CHECKBOX)
        .ValuePointer(&gSyntheticGatedValue)
        .PreFunc(SohGui::SohMenu::CapabilityGate(kSyntheticKey));
    probe.AddWidget(path, kUnregRowBase, WIDGET_CHECKBOX)
        .ValuePointer(&gUnregisteredGatedValue)
        .PreFunc(SohGui::SohMenu::CapabilityGate(kUnregisteredKey));
    // Chained: the row's own PreFunc must run FIRST (it may stage a value), the
    // gate LAST (it must have the final word on `disabled`).
    probe.AddWidget(path, kChainedRowBase, WIDGET_CHECKBOX)
        .ValuePointer(&gChainedValue)
        .PreFunc(SohGui::SohMenu::CapabilityGate(kSyntheticKey, [](WidgetInfo& info) {
            gChainedRan++;
            // Observable proof of ORDER: the chained function must see `disabled`
            // as ResetDisables() left it, never as the gate will leave it.
            gChainedSawDisabled = (info.options != nullptr && info.options->disabled) ? 1 : 0;
        }));
    probe.AddWidget(path, kHiddenRowBase, WIDGET_CHECKBOX)
        .ValuePointer(&gHiddenValue)
        .PreFunc(SohGui::SohMenu::CapabilityGate(kSyntheticKey, [](WidgetInfo& info) { info.isHidden = true; }));
    probe.AddWidget(path, kStateRowBase, WIDGET_CHECKBOX).ValuePointer(&gStateValue);

    // Resolved AFTER every registration: AddWidget push_backs, so a reference
    // taken earlier can be dangling.
    SidebarEntry& gatesPage = probe.Entries().at("CapProbe").sidebars.at("Gates");
    WidgetInfo* mmRow = FindRow(gatesPage, kMmRowBase);
    WidgetInfo* synRow = FindRow(gatesPage, kSynRowBase);
    WidgetInfo* unregRow = FindRow(gatesPage, kUnregRowBase);
    WidgetInfo* chainedRow = FindRow(gatesPage, kChainedRowBase);
    WidgetInfo* hiddenRow = FindRow(gatesPage, kHiddenRowBase);
    WidgetInfo* stateRow = FindRow(gatesPage, kStateRowBase);
    if (mmRow == nullptr || synRow == nullptr || unregRow == nullptr || chainedRow == nullptr || hiddenRow == nullptr ||
        stateRow == nullptr) {
        printf("[TEST] FAIL: the probe page did not register all six rows; nothing below can run\n");
        return 1;
    }

    // ---- Leg 2: a REAL capability, driven through both answers --------------
    // SOH_MENU_CAP_MM_HOSTED observes the foreign-item pool registry rather than
    // naming a symbol in MM's TU -- taking that address would BE the reference
    // that keeps the TU in the link, and the gate would pass vacuously (#640's
    // rule). Which is also why this leg can flip it: the registry has a
    // documented un-register.
    const ComboForeignItemDef* mmPool = nullptr;
    const int mmPoolCount = Combo_GetForeignItemPoolFor((uint8_t)GAME_MM, &mmPool);
    CAP_CHECK(mmPoolCount > 0 && mmPool != nullptr,
              "MM's item pool is not registered in this build, so leg 2's PRESENT half would be vacuous and its "
              "ABSENT half would prove nothing (pool count %d)",
              mmPoolCount);

    if (mmPoolCount > 0 && mmPool != nullptr) {
        // PRESENT.
        RunPreFunc(*mmRow);
        CAP_CHECK(!mmRow->options->disabled, "'%s' is disabled while MM's pool IS registered -- the capability is "
                                             "present and the row must be live",
                  kMmRowBase.c_str());
        CAP_CHECK(mmRow->name == kMmRowBase, "'%s' carries a state label while its capability is present: '%s'",
                  kMmRowBase.c_str(), mmRow->name.c_str());
        CAP_CHECK(SohGui::SohMenu::CapabilityPresent(SohGui::SOH_MENU_CAP_MM_HOSTED),
                  "CapabilityPresent(MM_HOSTED) is false while MM's pool holds %d entries", mmPoolCount);

        // ABSENT. Three frames, because a per-frame PreFunc is what compounds.
        Combo_RegisterForeignItemPool((uint8_t)GAME_MM, NULL, 0);
        CAP_CHECK(!SohGui::SohMenu::CapabilityPresent(SohGui::SOH_MENU_CAP_MM_HOSTED),
                  "CapabilityPresent(MM_HOSTED) is still true with MM's pool un-registered -- the predicate is not "
                  "observing the registry");
        const std::string mmReason = SohGui::SohMenu::CapabilityReason(SohGui::SOH_MENU_CAP_MM_HOSTED);
        for (int frame = 0; frame < 3; frame++) {
            RunPreFunc(*mmRow);
        }
        CAP_CHECK(mmRow->options->disabled, "'%s' is still enabled with its capability absent -- ADR 0004 §5's one "
                                            "forbidden outcome, a functional-looking control over nothing",
                  kMmRowBase.c_str());
        CAP_CHECK(std::string(Tooltip(*mmRow)) == mmReason,
                  "'%s' greyed itself with '%s', not the capability reason '%s'", kMmRowBase.c_str(), Tooltip(*mmRow),
                  mmReason.c_str());
        CAP_CHECK(mmRow->name.rfind(kMmRowBase, 0) == 0, "'%s' lost its base name: '%s'", kMmRowBase.c_str(),
                  mmRow->name.c_str());
        CAP_CHECK(mmRow->name.find(PresentLabel(SohGui::SOH_MENU_PRESENT_CAPABILITY)) != std::string::npos,
                  "'%s' does not state its state in the NAME: '%s'. §4.2 wants it legible without hovering, and "
                  "MenuDrawItem's race-lockout branch overwrites disabledTooltip outright",
                  kMmRowBase.c_str(), mmRow->name.c_str());
        CAP_CHECK(CountOf(mmRow->name, PresentLabel(SohGui::SOH_MENU_PRESENT_CAPABILITY)) == 1,
                  "'%s' accumulated its state label over 3 frames: '%s'", kMmRowBase.c_str(), mmRow->name.c_str());

        // PRESENT again: the row must recover, NAME included.
        Combo_RegisterForeignItemPool((uint8_t)GAME_MM, mmPool, mmPoolCount);
        RunPreFunc(*mmRow);
        CAP_CHECK(!mmRow->options->disabled, "'%s' stayed disabled after its capability came back", kMmRowBase.c_str());
        CAP_CHECK(mmRow->name == kMmRowBase,
                  "'%s' kept a stale state label after its capability came back: '%s'. ResetDisables() clears "
                  "`disabled` but not `name`, so the gate has to restore it",
                  kMmRowBase.c_str(), mmRow->name.c_str());
        CAP_CHECK(Combo_GetForeignItemPoolFor((uint8_t)GAME_MM, NULL) == mmPoolCount,
                  "MM's pool was not restored to its %d entries; AllTests runs every row in ONE process", mmPoolCount);
        printf("[TEST] leg 2: MM_HOSTED greys a row when MM's pool is un-registered and releases it when the pool "
               "comes back (%d entries)\n",
               mmPoolCount);
    }

    // ---- Leg 3: the registration API, including its refusals ---------------
    CAP_CHECK(std::string(SohGui::SohMenu::CapabilityReason(kSyntheticKey)) == kSyntheticReason,
              "RegisterCapability did not publish the synthetic reason (got '%s')",
              SohGui::SohMenu::CapabilityReason(kSyntheticKey));

    gSyntheticAbsent = false;
    RunPreFunc(*synRow);
    CAP_CHECK(!synRow->options->disabled, "'%s' is disabled while its capability reports PRESENT", kSynRowBase.c_str());
    gSyntheticAbsent = true;
    RunPreFunc(*synRow);
    CAP_CHECK(synRow->options->disabled, "'%s' is enabled while its capability reports ABSENT", kSynRowBase.c_str());
    CAP_CHECK(std::string(Tooltip(*synRow)) == kSyntheticReason, "'%s' carries '%s', expected the registered reason",
              kSynRowBase.c_str(), Tooltip(*synRow));

    // Refusals. A capability with no predicate or no reason must not install --
    // a half-registered key would read PRESENT by accident, or grey rows with an
    // empty tooltip.
    SohGui::SohMenu::RegisterCapability(kRefusedKey, nullptr, "Not yet available: has no predicate (#497)");
    CAP_CHECK(std::string(SohGui::SohMenu::CapabilityReason(kRefusedKey)).empty(),
              "RegisterCapability accepted a capability with no predicate");
    SohGui::SohMenu::RegisterCapability(kRefusedKey, SyntheticAbsent, "");
    CAP_CHECK(std::string(SohGui::SohMenu::CapabilityReason(kRefusedKey)).empty(),
              "RegisterCapability accepted a capability with an empty reason");

    // An unregistered key: ABSENT, with a reason that still names a tracker.
    RunPreFunc(*unregRow);
    CAP_CHECK(unregRow->options->disabled,
              "'%s' is enabled although nothing in this build publishes the capability it asked for. Letting it look "
              "live is the vacuous-gate class in UI form",
              kUnregRowBase.c_str());
    CAP_CHECK(std::string(Tooltip(*unregRow)).find("#497") != std::string::npos,
              "the unregistered-key fallback reason names no tracker: '%s'", Tooltip(*unregRow));
    printf("[TEST] leg 3: RegisterCapability round-trips, refuses a predicate-less and a reason-less capability, and "
           "an unregistered key reads ABSENT\n");

    // ---- Leg 4: the chained form's ordering and its isHidden short-circuit --
    gSyntheticAbsent = true;
    gChainedRan = 0;
    gChainedSawDisabled = -1;
    RunPreFunc(*chainedRow);
    CAP_CHECK(gChainedRan == 1, "the chained PreFunc ran %d times, expected once", gChainedRan);
    CAP_CHECK(gChainedSawDisabled == 0, "the chained PreFunc saw `disabled` already set -- the gate ran FIRST, so a "
                                        "row whose own PreFunc stages a value would be gated against a stale one");
    CAP_CHECK(chainedRow->options->disabled, "the gate did not get the last word on '%s'", kChainedRowBase.c_str());

    RunPreFunc(*hiddenRow);
    CAP_CHECK(hiddenRow->isHidden, "the chained PreFunc's isHidden was cleared");
    CAP_CHECK(!hiddenRow->options->disabled,
              "a hidden row was gated anyway; MenuDrawItem returns on isHidden, so that only spends the predicate");
    CAP_CHECK(hiddenRow->name == kHiddenRowBase, "a hidden row was relabelled: '%s'", hiddenRow->name.c_str());
    printf("[TEST] leg 4: the chained gate runs the row's own PreFunc first, keeps the last word on `disabled`, and "
           "skips a hidden row\n");

    // ---- Leg 5: §6's four presentations, and no accumulation ---------------
    // §6's table is what each state DENIES: live denies nothing; inactive denies
    // "this takes effect now" and stays EDITABLE; capability denies "this works";
    // frozen denies "this is still a choice".
    struct StateCase {
        SohGui::SohMenuPresentation state;
        const char* reason;
        bool expectDisabled;
    };
    const StateCase kCases[] = {
        { SohGui::SOH_MENU_PRESENT_LIVE, nullptr, false },
        { SohGui::SOH_MENU_PRESENT_INACTIVE_GAME, "Majora's Mask is suspended", false },
        { SohGui::SOH_MENU_PRESENT_CAPABILITY, "Not yet available: a reason with a tracker (#497)", true },
        { SohGui::SOH_MENU_PRESENT_FROZEN, "Already decided: frozen into this world's identity", true },
    };
    for (const StateCase& c : kCases) {
        stateRow->name = kStateRowBase;
        // Three applications of the SAME state, from `info.name` each time,
        // exactly as a PreFunc would.
        for (int frame = 0; frame < 3; frame++) {
            stateRow->ResetDisables();
            SohGui::SohMenu::ApplyPresentation(*stateRow, stateRow->name, c.state, c.reason);
        }
        const char* label = PresentLabel(c.state);
        CAP_CHECK(stateRow->options->disabled == c.expectDisabled,
                  "state %d left disabled=%d, expected %d -- ADR 0004 §6's table is what each presentation must deny",
                  (int)c.state, (int)stateRow->options->disabled, (int)c.expectDisabled);
        CAP_CHECK(stateRow->name.rfind(kStateRowBase, 0) == 0, "state %d lost the base name: '%s'", (int)c.state,
                  stateRow->name.c_str());
        if (c.state == SohGui::SOH_MENU_PRESENT_LIVE) {
            CAP_CHECK(stateRow->name == kStateRowBase, "a LIVE row is labelled: '%s'", stateRow->name.c_str());
            CAP_CHECK(std::string(Tooltip(*stateRow)).empty(), "a LIVE row carries a disabled tooltip: '%s'",
                      Tooltip(*stateRow));
        } else {
            CAP_CHECK(CountOf(stateRow->name, label) == 1, "state %d accumulated its label over 3 frames: '%s'",
                      (int)c.state, stateRow->name.c_str());
            CAP_CHECK(stateRow->name.find(c.reason) != std::string::npos,
                      "state %d does not carry its reason in the NAME: '%s'", (int)c.state, stateRow->name.c_str());
        }
        if (c.expectDisabled) {
            CAP_CHECK(std::string(Tooltip(*stateRow)) == c.reason, "state %d's tooltip is '%s', expected '%s'",
                      (int)c.state, Tooltip(*stateRow), c.reason);
        }
    }

    // A LIVE application must DROP a reason it is handed: a control that works
    // and explains why it does not is the same lie as one that does not work and
    // says nothing.
    stateRow->name = kStateRowBase;
    stateRow->ResetDisables();
    SohGui::SohMenu::ApplyPresentation(*stateRow, stateRow->name, SohGui::SOH_MENU_PRESENT_LIVE,
                                       "Not yet available: should be dropped (#497)");
    CAP_CHECK(stateRow->name == kStateRowBase, "a LIVE row given a reason kept it in the name: '%s'",
              stateRow->name.c_str());
    CAP_CHECK(std::string(Tooltip(*stateRow)).empty(), "a LIVE row given a reason kept it as a tooltip: '%s'",
              Tooltip(*stateRow));

    // Every transition between the four states, from a name the previous state
    // already wrote. The name must read as if only the last state had applied.
    for (int from = 0; from < (int)SohGui::SOH_MENU_PRESENT_COUNT; from++) {
        for (int to = 0; to < (int)SohGui::SOH_MENU_PRESENT_COUNT; to++) {
            stateRow->name = kStateRowBase;
            stateRow->ResetDisables();
            SohGui::SohMenu::ApplyPresentation(*stateRow, stateRow->name, (SohGui::SohMenuPresentation)from,
                                               from == (int)SohGui::SOH_MENU_PRESENT_LIVE ? nullptr : "Reason A (#497)");
            stateRow->ResetDisables();
            SohGui::SohMenu::ApplyPresentation(*stateRow, stateRow->name, (SohGui::SohMenuPresentation)to,
                                               to == (int)SohGui::SOH_MENU_PRESENT_LIVE ? nullptr : "Reason B (#497)");

            std::string expected = kStateRowBase;
            if (to != (int)SohGui::SOH_MENU_PRESENT_LIVE) {
                expected += std::string(" - ") + PresentLabel((SohGui::SohMenuPresentation)to) + ": Reason B (#497)";
            }
            CAP_CHECK(stateRow->name == expected, "state %d -> %d left the name '%s', expected '%s'", from, to,
                      stateRow->name.c_str(), expected.c_str());
        }
    }

    // §6's one explicit confusion: the two disabled states may not borrow each
    // other's wording. "Not yet available" sends a player hunting for a missing
    // feature; "already decided" tells them the choice was made.
    stateRow->name = kStateRowBase;
    stateRow->ResetDisables();
    SohGui::SohMenu::ApplyPresentation(*stateRow, stateRow->name, SohGui::SOH_MENU_PRESENT_CAPABILITY,
                                       "already decided: borrowed from the freeze");
    CAP_CHECK(std::string(Tooltip(*stateRow)) == std::string(PresentLabel(SohGui::SOH_MENU_PRESENT_CAPABILITY)),
              "a CAPABILITY gate kept a borrowed freeze reason: '%s'", Tooltip(*stateRow));
    stateRow->name = kStateRowBase;
    stateRow->ResetDisables();
    SohGui::SohMenu::ApplyPresentation(*stateRow, stateRow->name, SohGui::SOH_MENU_PRESENT_FROZEN,
                                       SohGui::SohMenu::CapabilityReason(SohGui::SOH_MENU_CAP_MM_HOSTED));
    CAP_CHECK(std::string(Tooltip(*stateRow)) == std::string(PresentLabel(SohGui::SOH_MENU_PRESENT_FROZEN)),
              "a FROZEN row kept a registered capability reason: '%s'", Tooltip(*stateRow));
    printf("[TEST] leg 5: all four §6 presentations render per the ADR, every transition between them is idempotent, "
           "and the two disabled states cannot borrow each other's wording\n");

    // ---- Leg 6: §4.2's marker, manifest-driven, over a PRODUCTION section ---
    CAP_CHECK(std::string(SohGui::SohMenu::SharedIntentMarker()) == std::string(Combo_ComboSettingSharedMarker()),
              "SohMenu's marker ('%s') and src/common's ('%s') disagree; a requirement with two spellings is one "
              "half the menu will fail",
              SohGui::SohMenu::SharedIntentMarker(), Combo_ComboSettingSharedMarker());

    // The manifest, by construction: every checked-in entry must be recognised, a
    // prefix entry must match by prefix, and a key in no entry must not match.
    CAP_CHECK(RSBS::kSharedIntentKeyCount > 0, "kSharedIntentKeys is empty; the pass below would be vacuous");
    for (std::size_t i = 0; i < RSBS::kSharedIntentKeyCount; i++) {
        const std::string entry(RSBS::kSharedIntentKeys[i]);
        const std::string probeKey = (!entry.empty() && entry.back() == '.') ? (entry + "SomeLeaf") : entry;
        CAP_CHECK(SohGui::SohMenu::IsSharedIntentKey(probeKey.c_str()),
                  "IsSharedIntentKey does not recognise manifest entry '%s' (probed as '%s')", entry.c_str(),
                  probeKey.c_str());
    }
    CAP_CHECK(!SohGui::SohMenu::IsSharedIntentKey("gDeveloperTools.RegEditEnabled"),
              "IsSharedIntentKey claimed a key that is in no manifest entry");
    CAP_CHECK(!SohGui::SohMenu::IsSharedIntentKey(nullptr), "IsSharedIntentKey(nullptr) claimed shared intent");
    CAP_CHECK(!SohGui::SohMenu::IsSharedIntentKey(""), "IsSharedIntentKey(\"\") claimed shared intent");
    CAP_CHECK(SohGui::SohMenu::MarkRowName("Row", "gCheats.InfiniteHealth") ==
                  (std::string(SohGui::SohMenu::SharedIntentMarker()) + " Row"),
              "MarkRowName did not prefix the marker for a shared-intent key (got '%s')",
              SohGui::SohMenu::MarkRowName("Row", "gCheats.InfiniteHealth").c_str());
    {
        const std::string once = SohGui::SohMenu::MarkRowName("Row", "gCheats.InfiniteHealth");
        CAP_CHECK(SohGui::SohMenu::MarkRowName(once, "gCheats.InfiniteHealth") == once,
                  "MarkRowName is not idempotent: '%s'",
                  SohGui::SohMenu::MarkRowName(once, "gCheats.InfiniteHealth").c_str());
    }

    // Now the PASS, over a real section. Dev Tools is the one SohMenu section
    // that registers ROM-free and binds genuine kSharedIntentKeys entries
    // ("gSettings.Menu.Popout", "gDeveloperTools.DebugEnabled", ...), so this is
    // the production code path over production rows, not a synthetic set.
    CapabilityMenuProbe markerProbe;
    markerProbe.AddMenuDevTools();
    auto& devEntries = markerProbe.Entries();
    if (!devEntries.contains("Dev Tools")) {
        printf("[TEST] FAIL: AddMenuDevTools registered no \"Dev Tools\" entry; leg 6's marker pass has nothing to "
               "run over\n");
        gFailures++;
    } else {
        int sharedRows = 0;
        int plainRows = 0;
        const int marked = markerProbe.ApplySharedIntentMarkers();
        const std::string marker(SohGui::SohMenu::SharedIntentMarker());
        for (auto& [sidebarName, sidebar] : devEntries.at("Dev Tools").sidebars) {
            for (auto& column : sidebar.columnWidgets) {
                for (WidgetInfo& row : column) {
                    const bool leads = row.name.rfind(marker, 0) == 0;
                    if (SohGui::SohMenu::IsSharedIntentKey(row.cVar)) {
                        sharedRows++;
                        CAP_CHECK(leads,
                                  "Dev Tools / %s row '%s' is bound to the shared-intent key '%s' and carries no "
                                  "marker. §4.2 is a hard requirement on the widget: a player who toggles this under "
                                  "Ocarina of Time and later finds Majora's Mask changed reads correct behaviour as "
                                  "a bug",
                                  sidebarName.c_str(), row.name.c_str(), row.cVar);
                    } else {
                        plainRows++;
                        CAP_CHECK(!leads,
                                  "Dev Tools / %s row '%s' carries the \"applies to both games\" marker but its key "
                                  "('%s') is in no kSharedIntentKeys entry -- the marker would be claiming something "
                                  "false",
                                  sidebarName.c_str(), row.name.c_str(), row.cVar != nullptr ? row.cVar : "(none)");
                    }
                }
            }
        }
        CAP_CHECK(sharedRows > 0, "no Dev Tools row is bound to a shared-intent key, so the pass proved nothing");
        CAP_CHECK(plainRows > 0, "every Dev Tools row is shared-intent, so the negative half proved nothing");
        CAP_CHECK(marked == sharedRows, "the pass reported %d rows marked but %d rows are shared-intent", marked,
                  sharedRows);
        CAP_CHECK(markerProbe.ApplySharedIntentMarkers() == 0,
                  "a second marker pass marked rows again; the pass runs once per menu build but must be idempotent "
                  "because a half-marked menu is worse than an unmarked one");
        printf("[TEST] leg 6: the marker pass marked %d of Dev Tools' rows -- every shared-intent one, no other -- "
               "and is idempotent\n",
               marked);
    }

    // Leave the process clean: the capability registry is process-global and
    // AllTests runs every dispatch entry in ONE process.
    SohGui::SohMenu::UnregisterCapability(kSyntheticKey);
    SohGui::SohMenu::UnregisterCapability(kRefusedKey);

    if (gFailures == 0) {
        printf("[TEST] menu-capability-gating: PASS\n");
    } else {
        printf("[TEST] menu-capability-gating: %d failure(s)\n", gFailures);
    }
    return gFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
