#pragma once
#include <ftxui/dom/elements.hpp>
#include <string>

namespace retn::ui {

struct CmdBarState {
    bool               active;
    const std::string& text;
};

/// Pure overlay element when help dialog is visible.
ftxui::Element renderHelpDialog();

/// Delete-confirm dialog element.
ftxui::Element renderDeleteDialog(int64_t card_id);

/// Quit-confirm dialog element.
ftxui::Element renderQuitDialog(int due_count);

} // namespace retn::ui
