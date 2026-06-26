#include "ui/command_bar.h"
#include "ui/components.h"
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace retn::ui {

Element renderHelpDialog() {
    Elements lines = {
        text("") ,
        text("  Commands:") | bold | color(pal::Primary),
        text("    add \"Q\" \"A\" [--tag tag]    Add a card inline") | color(pal::Muted),
        text("    add --interactive            Open interactive form") | color(pal::Muted),
        text("    delete <id>                 Remove a card") | color(pal::Muted),
        text("    filter <tag>                Study a specific tag") | color(pal::Muted),
        text("    filter all                  Study all decks") | color(pal::Muted),
        text("    help                        Show this dialog") | color(pal::Muted),
        text("") ,
        text("  Navigation:") | bold | color(pal::Primary),
        text("    j / k  or  ↓ / ↑           Move tag selection") | color(pal::Muted),
        text("    1 … 9                       Jump to tag by number") | color(pal::Muted),
        text("    0                           Clear filter (all decks)") | color(pal::Muted),
        text("    Enter                       Select focused tag") | color(pal::Muted),
        text("    Space                       Start review session") | color(pal::Muted),
        text("    :                           Open command bar") | color(pal::Muted),
        text("    q                           Quit (with confirm if reviewing)") | color(pal::Muted),
        text("") ,
        text("  Press any key to close") | color(pal::Highlight) | hcenter,
        text(""),
    };
    return modalBox("  HELP  ", std::move(lines));
}

Element renderDeleteDialog(int64_t card_id) {
    Elements lines = {
        text("") ,
        text("  Delete card #" + std::to_string(card_id) + "?") | color(pal::Primary),
        text("  This action cannot be undone.") | color(pal::Muted),
        text("") ,
        hbox({
            text("  "),
            text("[Y]") | color(pal::Err) | bold,
            text(" Delete   ") | color(pal::Muted),
            text("[N]") | color(pal::Highlight) | bold,
            text(" Cancel") | color(pal::Muted),
        }),
        text(""),
    };
    return modalBox("  CONFIRM DELETE  ", std::move(lines));
}

Element renderQuitDialog(int due_count) {
    std::string due_msg = due_count > 0
        ? "  " + std::to_string(due_count) + " card(s) remain in this session."
        : "  No active review session.";

    Elements lines = {
        text(""),
        text("  Quit retn?") | color(pal::Primary),
        text(due_msg) | color(pal::Muted),
        text(""),
        hbox({
            text("  "),
            text("[Y]") | color(pal::Err) | bold,
            text(" Quit   ") | color(pal::Muted),
            text("[N]") | color(pal::Highlight) | bold,
            text(" Cancel") | color(pal::Muted),
        }),
        text(""),
    };
    return modalBox("  QUIT  ", std::move(lines));
}

} // namespace retn::ui
