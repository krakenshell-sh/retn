#include "db/fsrs.h"
#include <algorithm>
#include <cmath>

namespace retn {

// ── Constructor ───────────────────────────────────────────────────────────────
FsrsCalculator::FsrsCalculator(std::array<double, 19> weights) : w_(weights) {}

// ── Private helpers ───────────────────────────────────────────────────────────

double FsrsCalculator::initialStability(Rating r) const {
    return w_[static_cast<int>(r) - 1];
}

double FsrsCalculator::initialDifficulty(Rating r) const {
    double d = w_[4] - std::exp(w_[5] * (static_cast<int>(r) - 1)) + 1.0;
    return std::clamp(d, 1.0, 10.0);
}

// R(t, S) = exp(ln(Rd) * t / S)   where Rd = DESIRED_RETENTION
double FsrsCalculator::retrievability(double s, double elapsed_days) const {
    if (s <= 0.0) return 0.0;
    return std::exp(std::log(DESIRED_RETENTION) * elapsed_days / s);
}

double FsrsCalculator::nextDifficulty(double d, Rating r) const {
    double d0_easy = std::clamp(w_[4] - std::exp(w_[5] * 3.0) + 1.0, 1.0, 10.0);
    double d_prime  = d - w_[6] * (static_cast<int>(r) - 3);
    return std::clamp(w_[7] * d0_easy + (1.0 - w_[7]) * d_prime, 1.0, 10.0);
}

double FsrsCalculator::nextStabilityRecall(double d, double s, double r_val,
                                            Rating rating) const {
    double hard_penalty = (rating == Rating::Hard) ? w_[15] : 1.0;
    double easy_bonus   = (rating == Rating::Easy) ? w_[16] : 1.0;
    double factor       = std::exp(w_[8]) *
                          (11.0 - d) *
                          std::pow(s, -w_[9]) *
                          (std::exp(w_[10] * (1.0 - r_val)) - 1.0) *
                          hard_penalty * easy_bonus;
    return s * (factor + 1.0);
}

double FsrsCalculator::nextStabilityForgot(double d, double s, double r_val) const {
    return w_[11] *
           std::pow(d, -w_[12]) *
           (std::pow(s + 1.0, w_[13]) - 1.0) *
           std::exp(w_[14] * (1.0 - r_val));
}

// FIX: use full FSRS interval formula so DESIRED_RETENTION is respected.
// When DESIRED_RETENTION == 0.9 the formula simplifies to round(stability),
// but the explicit form is correct for any target retention.
int FsrsCalculator::intervalDays(double stability) const {
    double factor = std::log(DESIRED_RETENTION) / std::log(0.9);
    return std::max(1, static_cast<int>(std::round(stability * factor)));
}

// ── Static label helpers ──────────────────────────────────────────────────────

std::string FsrsCalculator::labelFromSeconds(int64_t secs) {
    if (secs <= 0)          return "now";
    if (secs < 3600)        return std::to_string(secs / 60) + "m";
    if (secs < 86400)       return std::to_string(secs / 3600) + "h";
    int days = static_cast<int>(secs / 86400);
    return formatInterval(days);
}

std::string FsrsCalculator::formatInterval(int days) {
    if (days <= 0)          return "0d";
    if (days < 30)          return std::to_string(days) + "d";
    return std::to_string((days + 14) / 30) + "mo";
}

// ── Public API ────────────────────────────────────────────────────────────────

ReviewResult FsrsCalculator::calculate(const CardInfo& card, Rating rating,
                                        int64_t now_ts) const {
    static constexpr int64_t SEC_PER_DAY = 86400;

    ReviewResult res{};
    res.reps   = card.reps;
    res.lapses = card.lapses;

    if (card.state == CardState::New) {
        // ── First review — align with Py-FSRS v5 reference ───────────────────
        res.stability  = initialStability(rating);
        res.difficulty = initialDifficulty(rating);
        res.reps       = 1;

        switch (rating) {
            case Rating::Again:
                // FIX: was 10 min; Py-FSRS uses 1 min for new+Again (step 1 of [1m,10m])
                res.state         = CardState::Learning;
                res.interval_days = 0;
                res.next_review   = now_ts + AGAIN_STEP_SEC;
                break;

            case Rating::Hard:
                // FIX: was Review/~1d; Py-FSRS keeps Hard in Learning with ~5-10 min step
                res.state         = CardState::Learning;
                res.interval_days = 0;
                res.next_review   = now_ts + HARD_STEP_SEC;
                break;

            case Rating::Good:
                // FIX: was ~3d from S₀; Py-FSRS graduates to Review with fixed 1 day
                res.state         = CardState::Review;
                res.interval_days = 1;
                res.next_review   = now_ts + 1 * SEC_PER_DAY;
                break;

            case Rating::Easy:
                // Consistent with Py-FSRS: graduate to Review using S₀-based interval
                res.state         = CardState::Review;
                res.interval_days = intervalDays(res.stability);
                res.next_review   = now_ts + res.interval_days * SEC_PER_DAY;
                break;
        }

    } else {
        // ── Subsequent review (Learning / Review / Relearning) ────────────────
        double elapsed_days = 0.0;
        if (card.last_review > 0) {
            elapsed_days = std::max(0.0,
                static_cast<double>(now_ts - card.last_review) / SEC_PER_DAY);
        }

        double r_val = retrievability(card.stability, elapsed_days);
        double new_d = nextDifficulty(card.difficulty, rating);

        if (rating == Rating::Again) {
            res.lapses++;
            double new_s = std::max(0.1, nextStabilityForgot(new_d, card.stability, r_val));
            res.stability     = new_s;
            res.difficulty    = new_d;
            res.state         = CardState::Relearning;
            res.interval_days = 0;
            res.next_review   = now_ts + RELEARN_STEP_SEC;
        } else {
            res.reps++;
            double new_s = std::max(0.1,
                nextStabilityRecall(new_d, card.stability, r_val, rating));
            res.stability     = new_s;
            res.difficulty    = new_d;
            res.state         = CardState::Review;
            res.interval_days = intervalDays(new_s);
            res.next_review   = now_ts + res.interval_days * SEC_PER_DAY;
        }
    }

    return res;
}

// FIX: derive labels from actual next_review timestamps, not from interval_days alone.
// This correctly distinguishes "1m" (Again) from "10m" (Hard) for learning steps.
IntervalPreview FsrsCalculator::previewIntervals(const CardInfo& card,
                                                   int64_t now_ts) const {
    auto lbl = [&](Rating r) -> std::string {
        auto res = calculate(card, r, now_ts);
        if (res.interval_days == 0) {
            return labelFromSeconds(res.next_review - now_ts);
        }
        return formatInterval(res.interval_days);
    };
    return {lbl(Rating::Again), lbl(Rating::Hard), lbl(Rating::Good), lbl(Rating::Easy)};
}

} // namespace retn
