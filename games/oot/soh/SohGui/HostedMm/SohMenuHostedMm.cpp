/**
 * @file SohMenuHostedMm.cpp
 * @brief The hosted-row seam (see SohMenuHostedMm.h for the convention every page lane follows).
 */

#include "SohMenuHostedMm.h"

namespace SohGui {

namespace {

std::vector<HostedMmRow>& Registry() {
    static std::vector<HostedMmRow> rows;
    return rows;
}

} // namespace

WidgetInfo& HostedMmWidget::CVar(const char* key) {
    HostedMmRow& row = Registry().at(mIndex);
    row.key = key;
    row.accessor = nullptr;
    return mInfo.CVar(key);
}

WidgetInfo& HostedMmWidget::Accessor(const char* accessorName) {
    HostedMmRow& row = Registry().at(mIndex);
    row.key = nullptr;
    row.accessor = accessorName;
    return mInfo;
}

HostedMmWidget AddHostedMmWidget(SohMenu& menu, WidgetPath& path, const char* section, const std::string& name,
                                 WidgetType type) {
    WidgetInfo& info = menu.AddWidget(path, name, type);
    std::vector<HostedMmRow>& rows = Registry();
    // One entry per row: a second build of the same page (the live menu, then a test's probe of SoH's builders)
    // refreshes the entry instead of adding another.
    std::size_t index = rows.size();
    for (std::size_t i = 0; i < rows.size(); i++) {
        if (rows[i].header == path.sectionName && rows[i].sidebar == path.sidebarName && rows[i].name == name) {
            index = i;
            break;
        }
    }
    if (index == rows.size()) {
        rows.push_back({});
    }
    HostedMmRow& row = rows[index];
    row.header = path.sectionName;
    row.sidebar = path.sidebarName;
    row.section = section != nullptr ? section : "";
    row.name = name;
    row.key = nullptr;
    row.accessor = nullptr;
    return HostedMmWidget(info, index);
}

WidgetInfo& AddHostedMmBlock(SohMenu& menu, WidgetPath& path) {
    return menu.AddWidget(path, kHostedMmBlockLabel, WIDGET_SEPARATOR_TEXT);
}

const std::vector<HostedMmRow>& HostedMmRows() {
    return Registry();
}

bool IsHostedMmRow(const std::string& header, const std::string& sidebar, const std::string& name) {
    for (const HostedMmRow& row : Registry()) {
        if (row.header == header && row.sidebar == sidebar && row.name == name) {
            return true;
        }
    }
    return false;
}

} // namespace SohGui
