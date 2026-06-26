#include "ui/review.h"
#include "ui/components.h"
#include <ftxui/dom/elements.hpp>
#include <sstream>

using namespace ftxui;

namespace retn::ui {

// ── Shared review header ──────────────────────────────────────────────────────
static Element reviewHeader(const ReviewState& s) {
    std::string tag_label = s.focus_tag.empty() ? "All Decks" : "#" + s.focus_tag;
    std::string card_pos  = "Card " + std::to_string(s.card_idx + 1) +
                            " of " + std::to_string(s.total_cards);

    return hbox({
        text("  REVIEWING: ") | color(pal::Muted),
        text(tag_label) | color(pal::Highlight) | bold,
        text("  (" + card_pos + ")") | color(pal::Muted),
        filler(),
    });
}

// ── Wrap long text into paragraphs ───────────────────────────────────────────
static Element wrapText(const std::string& txt) {
    std::istringstream ss(txt);
    std::string line;
    Elements lines;
    while (std::getline(ss, line)) {
        lines.push_back(paragraph(line));
    }
    if (lines.empty()) lines.push_back(text(""));
    return vbox(std::move(lines));
}

// ── Command bar footer (same pattern as dashboard) ────────────────────────────
static Element reviewCmdBar(bool active, const std::string& text_in,
                              const std::string& left_hints,
                              const std::string& right_hints) {
    if (active) {
        return hbox({
            text("  >> ") | color(pal::Muted),
            text(text_in) | color(pal::Primary),
            text("▌") | color(pal::Highlight),
            filler(),
        });
    }
    return hbox({
        text("  " + left_hints) | color(pal::Muted),
        filler(),
        text(right_hints + "  ") | color(pal::Muted),
    });
}

// ── Screen 2: Question ────────────────────────────────────────────────────────
Element renderReviewQuestion(const ReviewState& s) {
    auto hdr = reviewHeader(s);

    auto q_block = vbox({
        separatorEmpty(),
        hbox({text("  Question:") | bold | color(pal::Primary), filler()}),
        separatorEmpty(),
        hbox({text("  "), wrapText(s.card.question) | color(pal::Primary)}),
        separatorEmpty(),
    }) | flex;

    auto footer = reviewCmdBar(s.cmd_active, s.cmd_text,
        "[Space] Show Answer",
        "[Esc] Main Menu   [:] Command");

    return vbox({
        hdr,
        separator(),
        q_block,
        separator(),
        footer,
    }) | border | flex;
}

// ── Screen 3: Answer ──────────────────────────────────────────────────────────
Element renderReviewAnswer(const ReviewState& s) {
    auto hdr = reviewHeader(s);

    auto q_block = vbox({
        separatorEmpty(),
        hbox({text("  Question:") | bold | color(pal::Muted), filler()}),
        separatorEmpty(),
        hbox({text("  "), wrapText(s.card.question) | color(pal::Muted)}),
        separatorEmpty(),
    });

    auto a_block = vbox({
        separatorEmpty(),
        hbox({text("  Answer:") | bold | color(pal::Primary), filler()}),
        separatorEmpty(),
        hbox({text("  "), wrapText(s.card.answer) | color(pal::Primary)}),
        separatorEmpty(),
    }) | flex;

    // Rating buttons row
    Element rating_row;
    if (s.cmd_active) {
        rating_row = hbox({
            text("  >> ") | color(pal::Muted),
            text(s.cmd_text) | color(pal::Primary),
            text("▌") | color(pal::Highlight),
            filler(),
        });
    } else {
        rating_row = hbox({
            text("  "),
            ratingButton(1, "Again", s.again_label),
            text("    "),
            ratingButton(2, "Hard",  s.hard_label),
            text("    "),
            ratingButton(3, "Good",  s.good_label),
            text("    "),
            ratingButton(4, "Easy",  s.easy_label),
            filler(),
            text("[Esc] Back  ") | color(pal::Muted),
        });
    }

    return vbox({
        hdr,
        separator(),
        q_block,
        separator(),
        a_block,
        separator(),
        rating_row,
    }) | border | flex;
}

} // namespace retn::ui
