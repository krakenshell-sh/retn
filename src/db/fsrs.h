#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace retn {

// ── FSRS v5 default weights (19 parameters) ──────────────────────────────────
// Source: open-spaced-repetition/fsrs5 reference implementation
inline constexpr std::array<double, 19> FSRS_DEFAULT_W = {
    0.4072, 1.1829, 3.1262, 15.4722, 7.2102, 0.5316, 1.0651, 0.0589,
    1.5330, 0.1544, 1.0071, 1.9440,  0.1100, 0.2900, 2.2700, 0.1500,
    2.9898, 0.5100, 0.3432
};

inline constexpr double DESIRED_RETENTION = 0.90;

// Learning step durations used for New and Relearning cards.
inline constexpr int64_t AGAIN_STEP_SEC   = 60;   //  1 min  (Again on new card)
inline constexpr int64_t HARD_STEP_SEC    = 600;  // 10 min  (Hard on new card, relearning)
inline constexpr int64_t RELEARN_STEP_SEC = 600;  // 10 min  (Again on review card)

// ── Enumerations ─────────────────────────────────────────────────────────────
enum class Rating    : int { Again = 1, Hard = 2, Good = 3, Easy = 4 };
enum class CardState : int { New = 0, Learning = 1, Review = 2, Relearning = 3 };

// ── Data types ────────────────────────────────────────────────────────────────
struct CardInfo {
    double    stability{0.0};
    double    difficulty{0.0};
    int       reps{0};
    int       lapses{0};
    CardState state{CardState::New};
    int64_t   last_review{0};
};

struct ReviewResult {
    double    stability;
    double    difficulty;
    int       reps;
    int       lapses;
    CardState state;
    int64_t   next_review;
    int       interval_days;  // 0 = learning/relearning step; use next_review for label
};

// String labels ready for display ("1m", "10m", "1d", "14d", "2mo").
// Derived from actual next_review timestamps so Again vs Hard are distinguishable.
struct IntervalPreview {
    std::string again;
    std::string hard;
    std::string good;
    std::string easy;
};

// ── Calculator ────────────────────────────────────────────────────────────────
class FsrsCalculator {
public:
    FsrsCalculator() = default;
    explicit FsrsCalculator(std::array<double, 19> weights);

    ReviewResult    calculate(const CardInfo& card, Rating rating, int64_t now_ts) const;
    IntervalPreview previewIntervals(const CardInfo& card, int64_t now_ts) const;

    /// Human-readable label for interval_days >= 1: "Nd" or "Nmo".
    /// For learning steps (0 days), prefer building the label from step seconds.
    static std::string formatInterval(int days);

    /// Build a display label from the number of seconds until next review.
    static std::string labelFromSeconds(int64_t secs);

private:
    std::array<double, 19> w_{FSRS_DEFAULT_W};

    double initialStability(Rating r) const;
    double initialDifficulty(Rating r) const;
    double retrievability(double s, double elapsed_days) const;
    double nextDifficulty(double d, Rating r) const;
    double nextStabilityRecall(double d, double s, double r_val, Rating rating) const;
    double nextStabilityForgot(double d, double s, double r_val) const;
    int    intervalDays(double stability) const;
};

} // namespace retn
