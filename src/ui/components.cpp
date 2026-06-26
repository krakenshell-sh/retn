#include "ui/components.h"
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace retn::ui {

Element keyHint(const std::string& key, const std::string& desc) {
    return hbox({
        text("[") | color(pal::Muted),
        text(key) | color(pal::Highlight) | bold,
        text("] ") | color(pal::Muted),
        text(desc) | color(pal::Muted),
    });
}

Element statRow(int pad, const std::string& label, const std::string& value,
                 bool highlight) {
    std::string prefix(pad, ' ');
    return hbox({
        text(prefix + label + " ") | color(pal::Muted),
        text(value) | color(highlight ? pal::Highlight : pal::Primary),
    });
}

Element ratingButton(int num, const std::string& label, const std::string& interval) {
    return hbox({
        text("[") | color(pal::Muted),
        text(std::to_string(num)) | color(pal::Highlight) | bold,
        text("] ") | color(pal::Muted),
        text(label) | color(pal::Primary),
        text(" (") | color(pal::Muted),
        text(interval) | color(pal::Highlight),
        text(")") | color(pal::Muted),
    });
}

Element modalBox(const std::string& title, Elements body_lines) {
    Elements lines;
    lines.push_back(text(" " + title + " ") | bold | color(pal::Primary) | hcenter);
    lines.push_back(separator());
    for (auto& l : body_lines) lines.push_back(l);
    return vbox(std::move(lines)) | border | color(pal::Primary) | center | clear_under;
}

Element statusLine(const std::string& msg) {
    if (msg.empty()) return separatorEmpty();
    return hbox({
        text("  "),
        text(msg) | color(pal::Highlight),
    });
}

Element hSep() { return separator(); }

} // namespace retn::ui
