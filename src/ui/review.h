#pragma once
#include "db/database.h"
#include "db/fsrs.h"
#include <ftxui/dom/elements.hpp>
#include <string>

namespace retn::ui {

struct ReviewState {
    const Card&          card;
    size_t               card_idx;    // 0-based
    size_t               total_cards;
    const std::string&   focus_tag;
    // Interval labels for answer screen
    std::string again_label;
    std::string hard_label;
    std::string good_label;
    std::string easy_label;
    // Command bar
    bool               cmd_active;
    const std::string& cmd_text;
};

ftxui::Element renderReviewQuestion(const ReviewState& s);
ftxui::Element renderReviewAnswer(const ReviewState& s);

} // namespace retn::ui
