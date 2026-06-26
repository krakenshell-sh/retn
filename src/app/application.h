#pragma once
#include "app/config.h"
#include "db/database.h"
#include "db/fsrs.h"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <memory>
#include <string>
#include <vector>

namespace retn {

enum class AppScreen {
    Dashboard,
    ReviewQuestion,
    ReviewAnswer,
    AdderInteractive,
};

class Application {
public:
    explicit Application(Config cfg);
    void run();

private:
    // ── Dependencies ──────────────────────────────────────────────────────────
    Config                    config_;
    std::unique_ptr<Database> db_;
    FsrsCalculator            fsrs_;
    ftxui::ScreenInteractive  screen_;

    // ── Screen state ──────────────────────────────────────────────────────────
    AppScreen current_screen_{AppScreen::Dashboard};

    // ── Dashboard state ───────────────────────────────────────────────────────
    std::string           focus_tag_;
    DeckStats             stats_{};
    std::vector<TagCount> tag_counts_;
    // -1 = "All Decks" mode (no tag highlighted); 0..N-1 = tag index
    int selected_tag_idx_{-1};

    // ── Review state ──────────────────────────────────────────────────────────
    std::vector<Card> due_cards_;
    size_t            current_card_idx_{0};

    struct IntervalLabels { std::string again, hard, good, easy; };
    IntervalLabels interval_labels_;

    // ── Command bar (manual string input) ─────────────────────────────────────
    bool        cmd_active_{false};
    std::string cmd_text_;

    // ── Adder state ───────────────────────────────────────────────────────────
    std::string          adder_q_;
    std::string          adder_a_;
    std::string          adder_tags_;
    // Full CatchEvent-wrapped container — used for event routing
    ftxui::Component     adder_component_;
    // Individual input components — used for per-field rendering with labels
    ftxui::Component     adder_q_comp_;
    ftxui::Component     adder_a_comp_;
    ftxui::Component     adder_t_comp_;

    // ── Overlay / dialog state ────────────────────────────────────────────────
    bool    show_help_{false};
    bool    show_quit_confirm_{false};
    bool    show_delete_confirm_{false};
    int64_t delete_target_id_{-1};
    int     remaining_cards_on_quit_{0};

    // ── Status message ────────────────────────────────────────────────────────
    std::string status_msg_;

    // ── Internal methods ──────────────────────────────────────────────────────
    void refreshData();
    void startReview();
    void applyRating(Rating rating);
    void saveAdderCard();
    void clearAdder();
    void executeCommand(const std::string& input);
    void updateIntervalLabels();

    ftxui::Element buildLayout();

    bool handleEvent(const ftxui::Event& e);
    bool handleDashboardEvent(const ftxui::Event& e);
    bool handleReviewQEvent(const ftxui::Event& e);
    bool handleReviewAEvent(const ftxui::Event& e);
    bool handleCmdBarEvent(const ftxui::Event& e);
};

} // namespace retn
