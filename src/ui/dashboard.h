#pragma once
#include "db/database.h"
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

namespace retn::ui {

struct DashboardState {
    const DeckStats&            stats;
    const std::vector<TagCount>& tag_counts;
    const std::string&          focus_tag;
    int                         selected_tag_idx;
    const std::string&          status_msg;
    bool                        cmd_active;
    const std::string&          cmd_text;
};

ftxui::Element renderDashboard(const DashboardState& s);

} // namespace retn::ui
