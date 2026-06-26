#include "app/application.h"
#include "cli/cli_parser.h"
#include "db/fsrs.h"
#include "ui/adder.h"
#include "ui/command_bar.h"
#include "ui/components.h"
#include "ui/dashboard.h"
#include "ui/review.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <chrono>
#include <sstream>

using namespace ftxui;

namespace retn {

// ── Constructor ───────────────────────────────────────────────────────────────
Application::Application(Config cfg)
    : config_(std::move(cfg)),
      db_(std::make_unique<Database>(config_.db_path)),
      fsrs_(),
      screen_(ScreenInteractive::Fullscreen()) {}

// ── run() ─────────────────────────────────────────────────────────────────────
void Application::run() {
    refreshData();

    // Build adder components. Individual q/a/t pointers kept for per-field
    // rendering with labels; container used for event routing.
    auto parts = ui::makeAdderComponent(
        &adder_q_, &adder_a_, &adder_tags_,
        [this] { saveAdderCard(); },
        [this] { clearAdder(); current_screen_ = AppScreen::Dashboard; });

    adder_component_ = parts.container;
    adder_q_comp_    = parts.q_input;
    adder_a_comp_    = parts.a_input;
    adder_t_comp_    = parts.t_input;

    // Root component:
    //   Renderer wraps adder_component_ so unhandled events (when on adder
    //   screen) flow through to the FTXUI Input components automatically.
    //   CatchEvent intercepts ALL events first and routes them.
    auto renderer = Renderer(adder_component_, [this] { return buildLayout(); });
    auto root     = CatchEvent(renderer, [this](Event e) -> bool { return handleEvent(e); });

    screen_.Loop(root);
}

// ── Data refresh ──────────────────────────────────────────────────────────────
void Application::refreshData() {
    stats_      = db_->getStats(focus_tag_);
    tag_counts_ = db_->getTagCounts();

    // BUG FIX: -1 is the sentinel for "All Decks / no tag selected".
    // Only clamp non-negative indices.
    if (selected_tag_idx_ >= 0 && !tag_counts_.empty()) {
        selected_tag_idx_ =
            std::min(selected_tag_idx_, static_cast<int>(tag_counts_.size()) - 1);
    } else if (tag_counts_.empty()) {
        selected_tag_idx_ = -1;  // No tags → stay in all-decks mode
    }
}

// ── Review helpers ────────────────────────────────────────────────────────────
static int64_t nowSec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void Application::updateIntervalLabels() {
    if (current_card_idx_ >= due_cards_.size()) return;
    const auto& c = due_cards_[current_card_idx_];

    CardInfo info{c.stability, c.difficulty, c.reps, c.lapses,
                  static_cast<CardState>(c.state), c.last_review};

    auto pv = fsrs_.previewIntervals(info, nowSec());
    interval_labels_ = {
        pv.again,
        pv.hard,
        pv.good,
        pv.easy,
    };
}

void Application::startReview() {
    due_cards_ = db_->getDueCards(focus_tag_, 50);
    if (due_cards_.empty()) {
        std::string lbl = focus_tag_.empty() ? "all decks" : "#" + focus_tag_;
        status_msg_ = "No cards due for " + lbl + "!  Come back later.";
        return;
    }
    current_card_idx_ = 0;
    updateIntervalLabels();
    current_screen_ = AppScreen::ReviewQuestion;
    status_msg_.clear();
}

void Application::applyRating(Rating rating) {
    if (current_card_idx_ >= due_cards_.size()) return;

    const auto& card = due_cards_[current_card_idx_];
    int64_t     now  = nowSec();

    CardInfo info{card.stability, card.difficulty, card.reps, card.lapses,
                  static_cast<CardState>(card.state), card.last_review};

    auto result = fsrs_.calculate(info, rating, now);

    Database::FsrsUpdate upd{
        result.stability, result.difficulty,
        result.reps,      result.lapses,
        static_cast<int>(result.state),
        now, result.next_review,
    };
    db_->updateFsrsState(card.id, upd);

    ++current_card_idx_;

    if (current_card_idx_ >= due_cards_.size()) {
        status_msg_     = "Session complete!  Reviewed " +
                          std::to_string(due_cards_.size()) + " card(s).";
        current_screen_ = AppScreen::Dashboard;
        refreshData();
    } else {
        current_screen_ = AppScreen::ReviewQuestion;
        updateIntervalLabels();
    }
}

// ── Adder helpers ─────────────────────────────────────────────────────────────
void Application::clearAdder() {
    adder_q_.clear();
    adder_a_.clear();
    adder_tags_.clear();
}

void Application::saveAdderCard() {
    if (adder_q_.empty()) { status_msg_ = "Question cannot be empty."; return; }
    if (adder_a_.empty()) { status_msg_ = "Answer cannot be empty.";   return; }

    std::vector<std::string> tags;
    std::istringstream       ss(adder_tags_);
    std::string              tok;
    while (ss >> tok) {
        if (!tok.empty() && tok[0] == '#') tok = tok.substr(1);
        if (!tok.empty()) tags.push_back(tok);
    }

    db_->addCard(adder_q_, adder_a_, tags);
    clearAdder();
    current_screen_ = AppScreen::Dashboard;
    status_msg_     = "Card added successfully!";
    refreshData();
}

// ── Command execution ─────────────────────────────────────────────────────────
void Application::executeCommand(const std::string& input) {
    CLIParser     parser;
    ParsedCommand cmd = parser.parse(input);

    if (cmd.error) { status_msg_ = cmd.error_msg; return; }

    switch (cmd.type) {
        case ParsedCommand::Type::Add: {
            if (cmd.question.empty()) { status_msg_ = "add: question required"; break; }
            if (cmd.answer.empty())   { status_msg_ = "add: answer required";   break; }
            std::vector<std::string> tags;
            if (!cmd.tag.empty()) tags.push_back(cmd.tag);
            db_->addCard(cmd.question, cmd.answer, tags);
            status_msg_ = "Card added!";
            refreshData();
            break;
        }
        case ParsedCommand::Type::AddInteractive:
            clearAdder();
            current_screen_ = AppScreen::AdderInteractive;
            break;

        case ParsedCommand::Type::Delete:
            if (cmd.card_id <= 0) { status_msg_ = "delete: invalid id"; break; }
            delete_target_id_    = cmd.card_id;
            show_delete_confirm_ = true;
            break;

        case ParsedCommand::Type::Filter:
            focus_tag_        = cmd.tag;
            selected_tag_idx_ = -1;   // Reset to all-decks visual mode
            refreshData();
            // Find the matching tag in the list and select it visually
            for (int i = 0; i < static_cast<int>(tag_counts_.size()); ++i) {
                if (tag_counts_[i].name == cmd.tag) { selected_tag_idx_ = i; break; }
            }
            status_msg_     = "Filter set to #" + cmd.tag;
            current_screen_ = AppScreen::Dashboard;
            break;

        case ParsedCommand::Type::FilterAll:
            focus_tag_.clear();
            selected_tag_idx_ = -1;   // BUG FIX: was 0, now -1 (all-decks sentinel)
            refreshData();
            status_msg_     = "Showing all decks";
            current_screen_ = AppScreen::Dashboard;
            break;

        case ParsedCommand::Type::Help:
            show_help_ = true;
            break;

        default:
            status_msg_ = "Unknown command. Type 'help'.";
    }
}

// ── Layout builder ────────────────────────────────────────────────────────────
Element Application::buildLayout() {
    Element base;

    switch (current_screen_) {
        case AppScreen::Dashboard: {
            ui::DashboardState ds{stats_, tag_counts_, focus_tag_,
                                   selected_tag_idx_, status_msg_,
                                   cmd_active_, cmd_text_};
            base = ui::renderDashboard(ds);
            break;
        }
        case AppScreen::ReviewQuestion:
        case AppScreen::ReviewAnswer: {
            if (current_card_idx_ < due_cards_.size()) {
                ui::ReviewState rs{due_cards_[current_card_idx_],
                                   current_card_idx_,
                                   due_cards_.size(),
                                   focus_tag_,
                                   interval_labels_.again,
                                   interval_labels_.hard,
                                   interval_labels_.good,
                                   interval_labels_.easy,
                                   cmd_active_,
                                   cmd_text_};
                base = (current_screen_ == AppScreen::ReviewQuestion)
                           ? ui::renderReviewQuestion(rs)
                           : ui::renderReviewAnswer(rs);
            } else {
                // Shouldn't happen normally — fall back to dashboard
                ui::DashboardState ds{stats_, tag_counts_, focus_tag_,
                                       selected_tag_idx_, status_msg_,
                                       cmd_active_, cmd_text_};
                base = ui::renderDashboard(ds);
            }
            break;
        }
        case AppScreen::AdderInteractive:
            // BUG FIX: render each field separately with its own label
            base = ui::renderAdder(adder_q_comp_->Render(),
                                    adder_a_comp_->Render(),
                                    adder_t_comp_->Render(),
                                    status_msg_, cmd_active_, cmd_text_);
            break;
    }

    // Overlay dialogs (dbox layers on top of base)
    if (show_quit_confirm_)
        base = dbox({base, ui::renderQuitDialog(remaining_cards_on_quit_)});
    else if (show_delete_confirm_)
        base = dbox({base, ui::renderDeleteDialog(delete_target_id_)});
    else if (show_help_)
        base = dbox({base, ui::renderHelpDialog()});

    return base;
}

// ── Event handler ─────────────────────────────────────────────────────────────
bool Application::handleEvent(const Event& e) {
    // ── Quit dialog ───────────────────────────────────────────────────────────
    if (show_quit_confirm_) {
        if (e == Event::Character('y') || e == Event::Character('Y'))
            screen_.ExitLoopClosure()();
        else
            show_quit_confirm_ = false;
        return true;
    }

    // ── Delete confirm dialog ─────────────────────────────────────────────────
    if (show_delete_confirm_) {
        if (e == Event::Character('y') || e == Event::Character('Y')) {
            db_->deleteCard(delete_target_id_);
            status_msg_          = "Card #" + std::to_string(delete_target_id_) + " deleted.";
            delete_target_id_    = -1;
            show_delete_confirm_ = false;
            refreshData();
        } else {
            show_delete_confirm_ = false;
        }
        return true;
    }

    // ── Help dialog ───────────────────────────────────────────────────────────
    if (show_help_) {
        show_help_ = false;
        return true;
    }

    // ── Command bar active ────────────────────────────────────────────────────
    if (cmd_active_) return handleCmdBarEvent(e);

    // ── Global keys (all screens) ─────────────────────────────────────────────
    if (e == Event::Character(':')) {
        cmd_active_ = true;
        cmd_text_.clear();
        return true;
    }

    // ── Adder screen: forward events to FTXUI Input components ───────────────
    if (current_screen_ == AppScreen::AdderInteractive) {
        if (e == Event::Special(std::string{'\x04'})) {
            saveAdderCard();
            return true;
        }
        if (e == Event::Escape) {
            clearAdder();
            current_screen_ = AppScreen::Dashboard;
            return true;
        }
        return false;  // Let adder_component_ handle Tab, chars, arrows, etc.
    }

    // ── Screen-specific routing ───────────────────────────────────────────────
    switch (current_screen_) {
        case AppScreen::Dashboard:      return handleDashboardEvent(e);
        case AppScreen::ReviewQuestion: return handleReviewQEvent(e);
        case AppScreen::ReviewAnswer:   return handleReviewAEvent(e);
        default:                        break;
    }

    // Consume all remaining key events so adder never receives stray input
    return e.is_character() || e == Event::Return || e == Event::Backspace ||
           e == Event::Tab   || e == Event::Escape ||
           e == Event::ArrowUp || e == Event::ArrowDown ||
           e == Event::ArrowLeft || e == Event::ArrowRight;
}

bool Application::handleDashboardEvent(const Event& e) {
    if (e == Event::Character('q') || e == Event::Character('Q')) {
        remaining_cards_on_quit_ = 0;
        show_quit_confirm_       = true;
        return true;
    }

    // Space → start review with current focus_tag_
    if (e == Event::Character(' ')) {
        startReview();
        return true;
    }

    // Enter → select the currently highlighted tag as filter
    if (e == Event::Return) {
        if (selected_tag_idx_ >= 0 &&
            selected_tag_idx_ < static_cast<int>(tag_counts_.size())) {
            focus_tag_ = tag_counts_[selected_tag_idx_].name;
            refreshData();
        }
        return true;
    }

    // j / ↓ — navigate down
    if (e == Event::Character('j') || e == Event::ArrowDown) {
        if (!tag_counts_.empty()) {
            int next = (selected_tag_idx_ < 0) ? 0 : selected_tag_idx_ + 1;
            selected_tag_idx_ = std::min(next, static_cast<int>(tag_counts_.size()) - 1);
        }
        return true;
    }

    // k / ↑ — navigate up
    if (e == Event::Character('k') || e == Event::ArrowUp) {
        if (!tag_counts_.empty()) {
            int prev = (selected_tag_idx_ <= 0) ? 0 : selected_tag_idx_ - 1;
            selected_tag_idx_ = prev;
        }
        return true;
    }

    // Number keys 1–9 → jump to tag N and set filter immediately
    if (e.is_character() && !e.character().empty()) {
        char c = e.character()[0];
        if (c >= '1' && c <= '9') {
            int idx = c - '1';
            if (idx < static_cast<int>(tag_counts_.size())) {
                selected_tag_idx_ = idx;
                focus_tag_        = tag_counts_[idx].name;
                refreshData();
            }
            return true;
        }
        // BUG FIX: was `selected_tag_idx_ = 0` — now -1 so no tag is highlighted
        if (c == '0') {
            focus_tag_.clear();
            selected_tag_idx_ = -1;
            refreshData();
            status_msg_ = "Showing all decks";
            return true;
        }
    }

    return true;  // Consume all unknowns on dashboard
}

bool Application::handleReviewQEvent(const Event& e) {
    if (e == Event::Escape) {
        current_screen_ = AppScreen::Dashboard;
        refreshData();
        return true;
    }
    if (e == Event::Character(' ')) {
        current_screen_ = AppScreen::ReviewAnswer;
        return true;
    }
    if (e == Event::Character('q') || e == Event::Character('Q')) {
        remaining_cards_on_quit_ =
            static_cast<int>(due_cards_.size() - current_card_idx_);
        show_quit_confirm_ = true;
        return true;
    }
    return true;
}

bool Application::handleReviewAEvent(const Event& e) {
    if (e == Event::Escape) {
        current_screen_ = AppScreen::Dashboard;
        refreshData();
        return true;
    }
    if (e == Event::Character('q') || e == Event::Character('Q')) {
        remaining_cards_on_quit_ =
            static_cast<int>(due_cards_.size() - current_card_idx_);
        show_quit_confirm_ = true;
        return true;
    }

    Rating rating;
    if      (e == Event::Character('1')) rating = Rating::Again;
    else if (e == Event::Character('2')) rating = Rating::Hard;
    else if (e == Event::Character('3')) rating = Rating::Good;
    else if (e == Event::Character('4')) rating = Rating::Easy;
    else return true;  // consume unknowns

    applyRating(rating);
    return true;
}

bool Application::handleCmdBarEvent(const Event& e) {
    if (e == Event::Escape) {
        cmd_active_ = false;
        cmd_text_.clear();
        return true;
    }
    if (e == Event::Return) {
        std::string cmd = cmd_text_;
        cmd_active_     = false;
        cmd_text_.clear();
        executeCommand(cmd);
        return true;
    }
    if (e == Event::Backspace) {
        if (!cmd_text_.empty()) cmd_text_.pop_back();
        return true;
    }
    if (e.is_character()) {
        cmd_text_ += e.character();
        return true;
    }
    return false;
}

} // namespace retn
