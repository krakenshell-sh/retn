#include "ui/dashboard.h"
#include "ui/components.h"
#include <ftxui/dom/elements.hpp>
#include <cstdio>

using namespace ftxui;

namespace retn::ui {

// ── Tag grid: 2 columns, natural-height rows ──────────────────────────────────
// BUG FIX: was `row | flex` (stretches rows vertically in vbox — wrong).
//          Correct: rows take natural height; columns take flex width in hbox.
static Element buildTagGrid(const std::vector<TagCount>& tags, int sel) {
    if (tags.empty()) {
        return hbox({
            text("  No tags yet.  Add cards with --tag to organize them.") | color(pal::Muted),
        });
    }

    Elements left_col, right_col;
    for (int i = 0; i < static_cast<int>(tags.size()); ++i) {
        const auto& tc  = tags[i];
        bool        focused = (sel >= 0 && i == sel);

        std::string num_str  = "[" + std::to_string(i + 1) + "] ";
        std::string name_str = "#" + tc.name;
        char due_buf[32];
        std::snprintf(due_buf, sizeof(due_buf), "  [%2d due]", tc.due_count);

        Element row = hbox({
            text("  " + num_str) | color(pal::Muted),
            text(name_str) | color(focused ? pal::Highlight : pal::Primary) |
                (focused ? bold : nothing),
            text(due_buf) | color(focused ? pal::Highlight : pal::Muted),
        });
        // No | flex on rows: natural single-line height is correct.

        if (i % 2 == 0) left_col.push_back(row);
        else             right_col.push_back(row);
    }

    // BUG FIX: padding was `text("") | flex` which expanded the column vertically.
    //          Plain text("") gives correct fixed-height padding.
    while (left_col.size() > right_col.size())  right_col.push_back(text(""));
    while (right_col.size() > left_col.size())  left_col.push_back(text(""));

    if (left_col.empty()) return text("");
    // Columns carry | flex so they share available horizontal space equally.
    return hbox({vbox(std::move(left_col)) | flex, vbox(std::move(right_col)) | flex});
}

// ── Command bar footer ────────────────────────────────────────────────────────
// BUG FIX: previously the hint row disappeared when cmd was active.
//          Now both rows are always present:
//          Row 1 — hint text (dimmed when cmd is active)
//          Row 2 — ">> [typed text]▌" or ">> _"
static Element buildCmdFooter(bool active, const std::string& cmd_text) {
    // Row 1: key hints
    Element hint_row;
    if (active) {
        // Mute the hint while the user is typing
        hint_row = hbox({
            text("  COMMAND BAR") | color(pal::Muted) | bold,
            text("  (Enter to execute, Esc to cancel)") | color(pal::Muted),
            filler(),
        });
    } else {
        hint_row = hbox({
            text("  COMMAND BAR") | color(pal::Muted) | bold,
            text("  (") | color(pal::Muted),
            text(":") | color(pal::Highlight),
            text(" commands  ") | color(pal::Muted),
            text("Space") | color(pal::Highlight),
            text(" review  ") | color(pal::Muted),
            text("j/k") | color(pal::Highlight),
            text(" navigate  ") | color(pal::Muted),
            text("0") | color(pal::Highlight),
            text(" all decks  ") | color(pal::Muted),
            text("q") | color(pal::Highlight),
            text(" quit)") | color(pal::Muted),
            filler(),
        });
    }

    // Row 2: input prompt — always visible
    Element input_row = hbox({
        text("  >> ") | color(pal::Muted),
        text(active ? cmd_text : std::string{}) | color(pal::Primary),
        text("▌") | color(active ? pal::Highlight : pal::Muted),
        filler(),
    });

    return vbox({hint_row, input_row});
}

// ── Main render ───────────────────────────────────────────────────────────────
Element renderDashboard(const DashboardState& s) {
    // Header
    auto header = hbox({
        text("  COLLECTION OVERVIEW") | bold | color(pal::Primary),
        filler(),
        text("[STUDY MODE]") | color(pal::Muted),
        text("  "),
    });

    // Stats panel (left half)
    auto stats_panel = vbox({
        text("  Card Statistics") | bold | color(pal::Primary),
        separatorEmpty(),
        statRow(4, "• Total Active :", std::to_string(s.stats.total)),
        statRow(4, "• New / Learn  :",
                std::to_string(s.stats.new_cards) + " / " +
                std::to_string(s.stats.learning)),
        statRow(4, "• Mature       :", std::to_string(s.stats.mature)),
        filler(),
    }) | flex;

    // Due status panel (right half)
    std::string focus_label = s.focus_tag.empty() ? "All Decks" : "#" + s.focus_tag;

    auto due_panel = vbox({
        text("  Due Status") | bold | color(pal::Primary),
        separatorEmpty(),
        statRow(4, "• Focus    :", focus_label),
        statRow(4, "• Due Now  :", std::to_string(s.stats.due_now) + " Cards",  true),
        statRow(4, "• Next 7d  :", std::to_string(s.stats.due_next7d) + " Cards"),
        filler(),
    }) | flex;

    auto mid_row = hbox({stats_panel, separator(), due_panel});

    // Tags section
    // sel = -1 means "All Decks" mode → no tag highlighted
    auto tag_grid = buildTagGrid(s.tag_counts, s.selected_tag_idx);

    return vbox({
        header,
        separator(),
        mid_row,
        separator(),
        text("  ACTIVE TAGS LOAD") | bold | color(pal::Muted),
        separatorEmpty(),
        tag_grid,
        separatorEmpty(),
        separator(),
        statusLine(s.status_msg),
        separator(),
        buildCmdFooter(s.cmd_active, s.cmd_text),
    }) | border | flex;
}

} // namespace retn::ui
