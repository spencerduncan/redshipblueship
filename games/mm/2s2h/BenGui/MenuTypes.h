#ifndef S2H_MENUTYPES_H
#define S2H_MENUTYPES_H
// Guard renamed from MENUTYPES_H: OoT's soh/SohGui/MenuTypes.h uses that exact
// guard, so a TU including both headers would silently drop whichever came
// second (#446).

#include "UIWidgets.hpp"

#ifdef RSBS_SINGLE_EXECUTABLE
// Single-exe symbol split for MM's menu type family (#446).
//
// OoT (games/oot/soh/SohGui/MenuTypes.h) defines identically-named
// global-scope types with DIVERGENT layouts: OoT's WidgetFunc/DisableInfoFunc
// are std::function (~32 bytes) where MM's are raw function pointers, OoT's
// WidgetInfo carries an extra `raceDisable` field, OptionsVariant has 9 vs 6
// alternatives, and the DisableOption/WidgetType enumerators sit at different
// values. MenuInit's inline registries COMDAT-fold across the two games into
// ONE shared registry, so an MM RegisterMenuInitFunc entering the link would
// inject MM menu-builder code into the registry OoT's SohMenu executes. This
// is the #383/FlagTable incident class: latent while MM's menu TUs stay
// link-elided, armed by any link-set change that pulls one in.
//
// Fix, per the ShipInit.hpp / UIWidgets.hpp (#434) recipe: move MM's types
// into namespace S2H so they stop sharing mangled names with OoT's, with
// using-declarations below so unqualified MM callers compile unchanged. A
// deliberate side effect: re-declaring any of these names at global scope in
// an MM TU that includes this header is a compile error in single-exe builds,
// so the split cannot silently regress.
namespace S2H {
#endif

typedef enum {
    DISABLE_FOR_CAMERAS_OFF,
    DISABLE_FOR_DEBUG_CAM_ON,
    DISABLE_FOR_DEBUG_CAM_OFF,
    DISABLE_FOR_FREE_LOOK_ON,
    DISABLE_FOR_FREE_LOOK_OFF,
    DISABLE_FOR_GYRO_OFF,
    DISABLE_FOR_GYRO_ON,
    DISABLE_FOR_RIGHT_STICK_OFF,
    DISABLE_FOR_AUTO_SAVE_OFF,
    DISABLE_FOR_NULL_PLAY_STATE,
    DISABLE_FOR_DEBUG_MODE_OFF,
    DISABLE_FOR_NO_VSYNC,
    DISABLE_FOR_NO_WINDOWED_FULLSCREEN,
    DISABLE_FOR_NO_MULTI_VIEWPORT,
    DISABLE_FOR_NOT_DIRECTX,
    DISABLE_FOR_DIRECTX,
    DISABLE_FOR_MATCH_REFRESH_RATE_ON,
    DISABLE_FOR_MOTION_BLUR_MODE,
    DISABLE_FOR_MOTION_BLUR_OFF,
    DISABLE_FOR_FRAME_ADVANCE_OFF,
    DISABLE_FOR_INTRO_SKIP_OFF,
    DISABLE_FOR_ADVANCED_RESOLUTION_ON,
    DISABLE_FOR_VERTICAL_RES_TOGGLE_ON,
    DISABLE_FOR_LOW_RES_MODE_ON,
    DISABLE_FOR_ADVANCED_RESOLUTION_OFF,
    DISABLE_FOR_VERTICAL_RESOLUTION_OFF,
    DISABLE_FOR_LINKS_VOICE_PITCH_MULTIPLIER_OFF,
    DISABLE_FOR_KOUME_INVINCIBLE,
} DisableOption;

struct WidgetInfo;
struct disabledInfo;
using VoidFunc = void (*)();
using DisableInfoFunc = bool (*)(disabledInfo&);
using DisableVec = std::vector<DisableOption>;
using WidgetFunc = void (*)(WidgetInfo&);

typedef enum {
    WIDGET_CHECKBOX,
    WIDGET_COMBOBOX,
    WIDGET_SLIDER_INT,
    WIDGET_SLIDER_FLOAT,
    WIDGET_CVAR_CHECKBOX,
    WIDGET_CVAR_COMBOBOX,
    WIDGET_CVAR_SLIDER_INT,
    WIDGET_CVAR_SLIDER_FLOAT,
    WIDGET_BUTTON,
    WIDGET_COLOR_24, // color picker without alpha
    WIDGET_COLOR_32, // color picker with alpha
    WIDGET_SEARCH,
    WIDGET_SEPARATOR,
    WIDGET_SEPARATOR_TEXT,
    WIDGET_TEXT,
    WIDGET_WINDOW_BUTTON,
    WIDGET_AUDIO_BACKEND, // needed for special operations that can't be handled easily with the normal combobox widget
    WIDGET_VIDEO_BACKEND, // same as above
    WIDGET_CUSTOM,
} WidgetType;

typedef enum {
    SECTION_COLUMN_1,
    SECTION_COLUMN_2,
    SECTION_COLUMN_3,
} SectionColumns;

typedef enum {
    MOTION_BLUR_DYNAMIC,
    MOTION_BLUR_ALWAYS_OFF,
    MOTION_BLUR_ALWAYS_ON,
} MotionBlurOption;

typedef enum {
    DEBUG_LOG_TRACE,
    DEBUG_LOG_DEBUG,
    DEBUG_LOG_INFO,
    DEBUG_LOG_WARN,
    DEBUG_LOG_ERROR,
    DEBUG_LOG_CRITICAL,
    DEBUG_LOG_OFF,
} DebugLogOption;

// holds the widget values for a widget, contains all CVar types available from LUS. int32_t is used for boolean
// evaluation
using CVarVariant = std::variant<int32_t, const char*, float, Color_RGBA8, Color_RGB8>;
using OptionsVariant =
    std::variant<UIWidgets::ButtonOptions, UIWidgets::CheckboxOptions, UIWidgets::ComboboxOptions,
                 UIWidgets::FloatSliderOptions, UIWidgets::IntSliderOptions, UIWidgets::WidgetOptions>;

// All the info needed for display and search of all widgets in the menu.
// `name` is the label displayed,
// `cVar` is the string representation of the CVar used to store the widget value
// `tooltip` is what is displayed when hovering (except when disabled, more on that later)
// `type` is the WidgetType for the widget, which is what determines how the information is used in the draw func
// `options` is a variant that holds the UIWidgetsOptions struct for the widget type
// blank objects need to be initialized with specific typing matching the expected Options struct for the widget
// `callback` is a lambda used for running code on widget change. may need `BenGui::GetMenu()` for specific menu actions
// `preFunc` is a lambda called before drawing code starts. It can be used to determine a widget's status,
// whether disabled or hidden, as well as update pointers for non-CVar widget types.
// `postFunc` is a lambda called after all drawing code is finished, for reacting to states other than
// widgets having been changed, like holding buttons.
// All three lambdas accept a `widgetInfo` reference in case it needs information on the widget for these operations
// `activeDisables` is a vector of DisableOptions for specifying what reasons a widget is disabled, which are displayed
// in the disabledTooltip for the widget. Can display multiple reasons. Handling the reasons is done in `preFunc`.
// It is recommended to utilize `disabledInfo`/`DisableReason` to list out all reasons for disabling and isHidden so
// the info can be shown.
// `windowName` is what is displayed and searched for `windowButton` type and window interactions
// `isHidden` just prevents the widget from being drawn under whatever circumstances you specify in the `preFunc`
// `sameLine` allows for specifying that the widget should be on the same line as the previous widget
struct WidgetInfo {
    std::string name; // Used by all widgets
    const char* cVar; // Used by all widgets except
    WidgetType type;
    std::shared_ptr<UIWidgets::WidgetOptions> options;
    std::variant<bool*, int32_t*, float*> valuePointer;
    WidgetFunc callback = nullptr;
    WidgetFunc preFunc = nullptr;
    WidgetFunc postFunc = nullptr;
    WidgetFunc customFunction = nullptr;
    DisableVec activeDisables = {};
    const char* windowName = "";
    bool isHidden = false;
    bool sameLine = false;
    bool hideInSearch = false;

    WidgetInfo& CVar(const char* cVar_) {
        cVar = cVar_;
        return *this;
    }
    WidgetInfo& Options(OptionsVariant options_) {
        switch (type) {
            case WIDGET_AUDIO_BACKEND:
            case WIDGET_VIDEO_BACKEND:
            case WIDGET_COMBOBOX:
            case WIDGET_CVAR_COMBOBOX:
                options = std::make_shared<UIWidgets::ComboboxOptions>(std::get<UIWidgets::ComboboxOptions>(options_));
                break;
            case WIDGET_CHECKBOX:
            case WIDGET_CVAR_CHECKBOX:
                options = std::make_shared<UIWidgets::CheckboxOptions>(std::get<UIWidgets::CheckboxOptions>(options_));
                break;
            case WIDGET_SLIDER_FLOAT:
            case WIDGET_CVAR_SLIDER_FLOAT:
                options =
                    std::make_shared<UIWidgets::FloatSliderOptions>(std::get<UIWidgets::FloatSliderOptions>(options_));
                break;
            case WIDGET_SLIDER_INT:
            case WIDGET_CVAR_SLIDER_INT:
                options =
                    std::make_shared<UIWidgets::IntSliderOptions>(std::get<UIWidgets::IntSliderOptions>(options_));
                break;
            case WIDGET_BUTTON:
            case WIDGET_WINDOW_BUTTON:
                options = std::make_shared<UIWidgets::ButtonOptions>(std::get<UIWidgets::ButtonOptions>(options_));
                break;
            case WIDGET_TEXT:
            case WIDGET_SEPARATOR_TEXT:
            case WIDGET_SEPARATOR:
            default:
                options = std::make_shared<UIWidgets::WidgetOptions>(std::get<UIWidgets::WidgetOptions>(options_));
        }
        return *this;
    }
    void ResetDisables() {
        isHidden = false;
        options->disabled = false;
        options->disabledTooltip = "";
        activeDisables.clear();
    }
    WidgetInfo& Options(std::shared_ptr<UIWidgets::WidgetOptions> options_) {
        options = options_;
        return *this;
    }
    WidgetInfo& Callback(WidgetFunc callback_) {
        callback = callback_;
        return *this;
    }
    WidgetInfo& PreFunc(WidgetFunc preFunc_) {
        preFunc = preFunc_;
        return *this;
    }
    WidgetInfo& PostFunc(WidgetFunc postFunc_) {
        postFunc = postFunc_;
        return *this;
    }
    WidgetInfo& WindowName(const char* windowName_) {
        windowName = windowName_;
        return *this;
    }
    WidgetInfo& ValuePointer(std::variant<bool*, int32_t*, float*> valuePointer_) {
        valuePointer = valuePointer_;
        return *this;
    }
    WidgetInfo& SameLine(bool sameLine_) {
        sameLine = sameLine_;
        return *this;
    }
    WidgetInfo& CustomFunction(WidgetFunc customFunction_) {
        customFunction = customFunction_;
        return *this;
    }

    WidgetInfo& HideInSearch(bool hide) {
        hideInSearch = hide;
        return *this;
    }
};

struct WidgetPath {
    std::string sectionName;
    std::string sidebarName;
    SectionColumns column;
};

// `disabledInfo` holds information on reasons for hiding or disabling a widget, as well as an evaluation lambda that
// is run once per frame to update its status (this is done to prevent dozens of redundant CVarGets in each frame loop)
// `evaluation` returns a bool which can be determined by whatever code you want that changes its status
// `reason` is the text displayed in the disabledTooltip when a widget is disabled by a particular DisableReason
// `active` is what's referenced when determining disabled status for a widget that uses this This can also be used to
// hold reasons to hide widgets so that their evaluations are also only run once per frame
struct disabledInfo {
    DisableInfoFunc evaluation;
    const char* reason;
    bool active = false;
    int32_t value = 0;
};

// struct Sidebar {
//     //std::unordered_map<std::string, SidebarEntry> entries;
//     uint32_t columnCount;
//     std::vector<std::vector<WidgetInfo>> columnWidgets;
//
//     void Insert(std::string entryName, WidgetInfo& entry, int32_t index = -1) {
//         if (index == -1 || index >= entryOrder.size()) {
//             entryOrder.push_back(entryName);
//         } else {
//             entryOrder.insert(entryOrder.begin() + index, entryName);
//         }
//         columnWidgets[entryName].push_back({entry});
//     }
//
//     void Erase(std::string entryName) {
//         if (columnWidgets.contains(entryName)) {
//             columnWidgets.erase(entryName);
//         }
//         std::erase_if(entryOrder, [entryName](std::string name) { return name == entryName; });
//     }
// };

// Contains the name displayed in the sidebar (label), the number of columns to use in drawing (columnCount; for visual
// separation, 1-3), and nested vectors of the widgets, grouped by column (columnWidgets). The number of widget vectors
// added to the column groups does not need to match the specified columnCount, e.g. you can have one vector added to
// the sidebar, but still separate the window into 3 columns and display only in the first column
struct SidebarEntry {
    uint32_t columnCount;
    std::vector<std::vector<WidgetInfo>> columnWidgets;
};

struct SearchWidget {
    // First four required
    WidgetInfo& info;
    std::string menuName;
    std::string sidebarName;
    std::string location;
    std::string extraTerms = "";
};

// Contains entries for what's listed in the header at the top, including the name displayed on the top bar (label),
// a vector of the SidebarEntries for that header entry, and the name of the cvar used to track what sidebar entry is
// the last viewed for that header.
struct MainMenuEntry {
    std::string label;
    const char* sidebarCvar;
    std::unordered_map<std::string, SidebarEntry> sidebars = {};
    std::vector<std::string> sidebarOrder = {};
};

static const std::unordered_map<Ship::AudioBackend, const char*> audioBackendsMap = {
    { Ship::AudioBackend::WASAPI, "Windows Audio Session API" },
    { Ship::AudioBackend::SDL, "SDL" },
    { Ship::AudioBackend::NUL, "Null" },
};

static const std::unordered_map<Ship::WindowBackend, const char*> windowBackendsMap = {
    { Ship::WindowBackend::FAST3D_DXGI_DX11, "DirectX" },
    { Ship::WindowBackend::FAST3D_SDL_OPENGL, "OpenGL" },
    { Ship::WindowBackend::FAST3D_SDL_METAL, "Metal" },
};

struct MenuInit {
    static std::vector<std::function<void()>>& GetInitFuncs() {
        static std::vector<std::function<void()>> menuInitFuncs;
        return menuInitFuncs;
    }

    static std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::function<void()>>>>&
    GetUpdateFuncs() {
        static std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::function<void()>>>>
            menuUpdateFuncs;
        return menuUpdateFuncs;
    }

    static void InitAll() {
        auto& menuInitFuncs = MenuInit::GetInitFuncs();
        for (const auto& initFunc : menuInitFuncs) {
            initFunc();
        }
    }
};

struct RegisterMenuInitFunc {
    RegisterMenuInitFunc(std::function<void()> initFunc) {
        auto& menuInitFuncs = MenuInit::GetInitFuncs();

        menuInitFuncs.push_back(initFunc);
    }
};

struct RegisterMenuUpdateFunc {
    RegisterMenuUpdateFunc(std::function<void()> updateFunc, std::string sectionName, std::string sidebarName) {
        auto& menuUpdateFuncs = MenuInit::GetUpdateFuncs();

        menuUpdateFuncs[sectionName][sidebarName].push_back(updateFunc);
    }
};

#ifdef RSBS_SINGLE_EXECUTABLE
} // namespace S2H

// Let unqualified upstream MM callers resolve to the S2H versions unchanged.
// The unscoped enums are typedef'd anonymous enums, so their enumerators are
// re-exposed one by one (`using enum` on a typedef-name of an unnamed enum is
// not portable across our CI compilers).
using S2H::DebugLogOption;
using S2H::DisableOption;
using S2H::MotionBlurOption;
using S2H::SectionColumns;
using S2H::WidgetType;

// DisableOption enumerators
using S2H::DISABLE_FOR_ADVANCED_RESOLUTION_OFF;
using S2H::DISABLE_FOR_ADVANCED_RESOLUTION_ON;
using S2H::DISABLE_FOR_AUTO_SAVE_OFF;
using S2H::DISABLE_FOR_CAMERAS_OFF;
using S2H::DISABLE_FOR_DEBUG_CAM_OFF;
using S2H::DISABLE_FOR_DEBUG_CAM_ON;
using S2H::DISABLE_FOR_DEBUG_MODE_OFF;
using S2H::DISABLE_FOR_DIRECTX;
using S2H::DISABLE_FOR_FRAME_ADVANCE_OFF;
using S2H::DISABLE_FOR_FREE_LOOK_OFF;
using S2H::DISABLE_FOR_FREE_LOOK_ON;
using S2H::DISABLE_FOR_GYRO_OFF;
using S2H::DISABLE_FOR_GYRO_ON;
using S2H::DISABLE_FOR_INTRO_SKIP_OFF;
using S2H::DISABLE_FOR_KOUME_INVINCIBLE;
using S2H::DISABLE_FOR_LINKS_VOICE_PITCH_MULTIPLIER_OFF;
using S2H::DISABLE_FOR_LOW_RES_MODE_ON;
using S2H::DISABLE_FOR_MATCH_REFRESH_RATE_ON;
using S2H::DISABLE_FOR_MOTION_BLUR_MODE;
using S2H::DISABLE_FOR_MOTION_BLUR_OFF;
using S2H::DISABLE_FOR_NO_MULTI_VIEWPORT;
using S2H::DISABLE_FOR_NO_VSYNC;
using S2H::DISABLE_FOR_NO_WINDOWED_FULLSCREEN;
using S2H::DISABLE_FOR_NOT_DIRECTX;
using S2H::DISABLE_FOR_NULL_PLAY_STATE;
using S2H::DISABLE_FOR_RIGHT_STICK_OFF;
using S2H::DISABLE_FOR_VERTICAL_RES_TOGGLE_ON;
using S2H::DISABLE_FOR_VERTICAL_RESOLUTION_OFF;

// WidgetType enumerators
using S2H::WIDGET_AUDIO_BACKEND;
using S2H::WIDGET_BUTTON;
using S2H::WIDGET_CHECKBOX;
using S2H::WIDGET_COLOR_24;
using S2H::WIDGET_COLOR_32;
using S2H::WIDGET_COMBOBOX;
using S2H::WIDGET_CUSTOM;
using S2H::WIDGET_CVAR_CHECKBOX;
using S2H::WIDGET_CVAR_COMBOBOX;
using S2H::WIDGET_CVAR_SLIDER_FLOAT;
using S2H::WIDGET_CVAR_SLIDER_INT;
using S2H::WIDGET_SEARCH;
using S2H::WIDGET_SEPARATOR;
using S2H::WIDGET_SEPARATOR_TEXT;
using S2H::WIDGET_SLIDER_FLOAT;
using S2H::WIDGET_SLIDER_INT;
using S2H::WIDGET_TEXT;
using S2H::WIDGET_VIDEO_BACKEND;
using S2H::WIDGET_WINDOW_BUTTON;

// SectionColumns enumerators
using S2H::SECTION_COLUMN_1;
using S2H::SECTION_COLUMN_2;
using S2H::SECTION_COLUMN_3;

// MotionBlurOption enumerators
using S2H::MOTION_BLUR_ALWAYS_OFF;
using S2H::MOTION_BLUR_ALWAYS_ON;
using S2H::MOTION_BLUR_DYNAMIC;

// DebugLogOption enumerators
using S2H::DEBUG_LOG_CRITICAL;
using S2H::DEBUG_LOG_DEBUG;
using S2H::DEBUG_LOG_ERROR;
using S2H::DEBUG_LOG_INFO;
using S2H::DEBUG_LOG_OFF;
using S2H::DEBUG_LOG_TRACE;
using S2H::DEBUG_LOG_WARN;

using S2H::CVarVariant;
using S2H::DisableInfoFunc;
using S2H::DisableVec;
using S2H::OptionsVariant;
using S2H::VoidFunc;
using S2H::WidgetFunc;

using S2H::disabledInfo;
using S2H::MainMenuEntry;
using S2H::MenuInit;
using S2H::RegisterMenuInitFunc;
using S2H::RegisterMenuUpdateFunc;
using S2H::SearchWidget;
using S2H::SidebarEntry;
using S2H::WidgetInfo;
using S2H::WidgetPath;

using S2H::audioBackendsMap;
using S2H::windowBackendsMap;
#endif

#endif // S2H_MENUTYPES_H
