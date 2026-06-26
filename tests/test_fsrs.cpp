// Standalone FSRS tests (no external test framework required).
#include "db/fsrs.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

using namespace retn;

static int passed = 0, failed = 0;

#define EXPECT(cond, msg)                                         \
    do {                                                          \
        if (!(cond)) {                                            \
            std::cerr << "FAIL  [" << (msg) << "]: "             \
                      << #cond << "\n";                           \
            ++failed;                                             \
        } else {                                                  \
            std::cout << "PASS  [" << (msg) << "]\n";            \
            ++passed;                                             \
        }                                                         \
    } while (0)

#define EXPECT_GT(a, b, msg) EXPECT((a) > (b),  msg)
#define EXPECT_GE(a, b, msg) EXPECT((a) >= (b), msg)
#define EXPECT_EQ(a, b, msg) EXPECT((a) == (b), msg)
#define EXPECT_LT(a, b, msg) EXPECT((a) < (b),  msg)

int main() {
    FsrsCalculator fsrs;
    constexpr int64_t T0 = 1'700'000'000LL;

    // ── Test 1: New card rated Good ───────────────────────────────────────────
    // After fix: Good new card → Review with fixed 1-day interval (Py-FSRS ref)
    {
        CardInfo c{};
        auto r = fsrs.calculate(c, Rating::Good, T0);

        EXPECT_GT(r.stability,     0.0,               "new+Good stability > 0");
        EXPECT_EQ(r.state,         CardState::Review,  "new+Good state=Review");
        EXPECT_EQ(r.interval_days, 1,                  "new+Good interval=1d (fixed, Py-FSRS)");
        EXPECT_GT(r.reps,          0,                  "new+Good reps incremented");
        EXPECT_GT(r.next_review,   T0,                 "new+Good next_review in future");

        std::cout << "  stability=" << r.stability
                  << " interval=" << r.interval_days << "d\n";
    }

    // ── Test 2: Two consecutive Good ratings → interval grows ─────────────────
    {
        CardInfo c1{};
        auto r1 = fsrs.calculate(c1, Rating::Good, T0);

        // Second review: card is now in Review state with computed stability
        int64_t  T1 = T0 + r1.interval_days * 86400LL;
        CardInfo c2{r1.stability, r1.difficulty, r1.reps, r1.lapses, r1.state, T0};
        auto r2 = fsrs.calculate(c2, Rating::Good, T1);

        EXPECT_GT(r2.interval_days, r1.interval_days, "second Good > first Good interval");
        EXPECT_GT(r2.stability,     r1.stability,      "second Good stability grows");

        std::cout << "  r1.interval=" << r1.interval_days
                  << "d  r2.interval=" << r2.interval_days << "d\n";
    }

    // ── Test 3: New card rated Again → Learning state, 1-minute step ──────────
    // FIX verified: step changed from 10 min → 1 min to align with Py-FSRS
    {
        CardInfo c{};
        auto r = fsrs.calculate(c, Rating::Again, T0);

        EXPECT_EQ(r.state,         CardState::Learning, "new+Again state=Learning");
        EXPECT_EQ(r.interval_days, 0,                   "new+Again interval=0 (step)");
        EXPECT_GT(r.reps,          0,                   "new+Again reps incremented");
        EXPECT_GT(r.next_review,   T0,                  "new+Again next_review > now");
        // 1-minute step: next_review = T0+60. Any step > 30s is valid.
        EXPECT_GT(r.next_review,   T0 + 30,             "next_review at least 30s ahead");
        // Confirm it is NOT 10 minutes (old wrong value)
        EXPECT_LT(r.next_review,   T0 + 300,            "next_review < 5 min (not old 10m)");

        std::cout << "  Again -> next in " << (r.next_review - T0) << "s\n";
    }

    // ── Test 3b: New card rated Hard → Learning state, 10-minute step ─────────
    // FIX verified: Hard now goes to Learning (not Review) for new cards
    {
        CardInfo c{};
        auto r = fsrs.calculate(c, Rating::Hard, T0);

        EXPECT_EQ(r.state,         CardState::Learning, "new+Hard state=Learning");
        EXPECT_EQ(r.interval_days, 0,                   "new+Hard interval=0 (step)");
        EXPECT_GT(r.stability,     0.0,                 "new+Hard stability > 0");
        EXPECT_GT(r.next_review,   T0 + 300,            "new+Hard step > 5 min");
        EXPECT_LT(r.next_review,   T0 + 3600,           "new+Hard step < 1 hour");

        std::cout << "  Hard -> next in " << (r.next_review - T0) << "s\n";
    }

    // ── Test 4: Review card + Again → Relearning, lapses incremented ──────────
    {
        CardInfo c{3.5, 5.0, 2, 0, CardState::Review, T0 - 3 * 86400LL};
        auto     r = fsrs.calculate(c, Rating::Again, T0);

        EXPECT_EQ(r.state,         CardState::Relearning, "review+Again → Relearning");
        EXPECT_EQ(r.lapses,        1,                      "review+Again lapses=1");
        EXPECT_EQ(r.interval_days, 0,                      "review+Again interval=0");
        EXPECT_GT(r.stability,     0.0,                    "review+Again stability > 0");
    }

    // ── Test 5: Easy > Good > Hard > Again stability and intervals ────────────
    {
        CardInfo c{};
        auto ra = fsrs.calculate(c, Rating::Again, T0);
        auto rh = fsrs.calculate(c, Rating::Hard,  T0);
        auto rg = fsrs.calculate(c, Rating::Good,  T0);
        auto re = fsrs.calculate(c, Rating::Easy,  T0);

        // Stability ordering always holds
        EXPECT_GT(re.stability, rg.stability, "Easy stability > Good");
        EXPECT_GT(rg.stability, rh.stability, "Good stability > Hard");
        EXPECT_GT(rh.stability, ra.stability, "Hard stability > Again");

        // Interval ordering: Easy(~15d) >= Good(1d) >= Hard(0, step) >= Again(0, step)
        EXPECT_GE(re.interval_days, rg.interval_days, "Easy interval >= Good");
        EXPECT_GE(rg.interval_days, rh.interval_days, "Good interval >= Hard");

        std::cout << "  Again=" << ra.interval_days << "d"
                  << "  Hard="  << rh.interval_days << "d"
                  << "  Good="  << rg.interval_days << "d"
                  << "  Easy="  << re.interval_days << "d\n";
    }

    // ── Test 6: previewIntervals returns correct string labels ────────────────
    // FIX verified: IntervalPreview now carries string labels, not int days.
    // Labels for learning steps are derived from next_review - now_ts.
    {
        CardInfo c{};
        auto pv = fsrs.previewIntervals(c, T0);

        // All labels must be non-empty
        EXPECT(!pv.again.empty(), "preview again label non-empty");
        EXPECT(!pv.hard.empty(),  "preview hard label non-empty");
        EXPECT(!pv.good.empty(),  "preview good label non-empty");
        EXPECT(!pv.easy.empty(),  "preview easy label non-empty");

        // Again (1 min) should be labelled "1m"
        EXPECT_EQ(pv.again, std::string("1m"),  "again label = 1m");
        // Hard (10 min) should be labelled "10m"
        EXPECT_EQ(pv.hard,  std::string("10m"), "hard label = 10m");
        // Good (1 day fixed) should be "1d"
        EXPECT_EQ(pv.good,  std::string("1d"),  "good label = 1d");
        // Easy uses S₀=15.47 → "15d"
        EXPECT(!pv.easy.empty(), "easy label non-empty");

        std::cout << "  preview again=" << pv.again
                  << " hard=" << pv.hard
                  << " good=" << pv.good
                  << " easy=" << pv.easy << "\n";
    }

    // ── Test 7: formatInterval labels (unchanged utility function) ────────────
    {
        EXPECT_EQ(FsrsCalculator::formatInterval(1),  std::string("1d"),   "format 1 -> 1d");
        EXPECT_EQ(FsrsCalculator::formatInterval(14), std::string("14d"),  "format 14 -> 14d");
        EXPECT_EQ(FsrsCalculator::formatInterval(30), std::string("1mo"),  "format 30 -> 1mo");
        EXPECT_EQ(FsrsCalculator::formatInterval(60), std::string("2mo"),  "format 60 -> 2mo");
    }

    // ── Test 8: labelFromSeconds ──────────────────────────────────────────────
    {
        EXPECT_EQ(FsrsCalculator::labelFromSeconds(60),    std::string("1m"),  "60s -> 1m");
        EXPECT_EQ(FsrsCalculator::labelFromSeconds(600),   std::string("10m"), "600s -> 10m");
        EXPECT_EQ(FsrsCalculator::labelFromSeconds(86400), std::string("1d"),  "86400s -> 1d");
    }

    // ── Test 9: intervalDays respects DESIRED_RETENTION ──────────────────────
    // At Rd=0.9: ln(0.9)/ln(0.9)=1.0, so intervalDays(S) = round(S).
    // Verify the formula is consistent with this expectation.
    {
        CardInfo c{};
        auto r = fsrs.calculate(c, Rating::Easy, T0);
        // S₀(Easy) = w[3] = 15.4722  → intervalDays = round(15.4722) = 15
        EXPECT_EQ(r.interval_days, 15, "intervalDays(15.47) = 15 at Rd=0.9");
    }

    std::cout << "\n── FSRS Tests: " << passed << " passed, "
              << failed << " failed ──\n";
    return failed == 0 ? 0 : 1;
}
