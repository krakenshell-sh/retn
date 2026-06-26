// Standalone database tests (no external test framework required).
#include "db/database.h"
#include "db/fsrs.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

using namespace retn;

static int passed = 0, failed = 0;

#define EXPECT(cond, msg)                                          \
    do {                                                           \
        if (!(cond)) {                                             \
            std::cerr << "FAIL  [" << (msg) << "]: " << #cond << '\n'; \
            ++failed;                                              \
        } else {                                                   \
            std::cout << "PASS  [" << (msg) << "]\n";             \
            ++passed;                                              \
        }                                                          \
    } while (0)

#define EXPECT_EQ(a, b, msg) EXPECT((a) == (b), msg)
#define EXPECT_GT(a, b, msg) EXPECT((a) > (b), msg)

int main() {
    const std::string TEST_DB = "/tmp/retn_unit_test.db";
    std::filesystem::remove(TEST_DB);

    try {
        Database db(TEST_DB);

        // ── Test 1: addCard returns valid id ──────────────────────────────────
        auto id1 = db.addCard("What is 2+2?", "4", {"math"});
        EXPECT_GT(id1, 0, "addCard returns positive id");

        auto id2 = db.addCard("Bonjour = ?", "Hello", {"french", "vocab"});
        EXPECT_GT(id2, 0, "addCard with multiple tags");

        // ── Test 2: getDueCards returns new cards immediately ─────────────────
        auto due = db.getDueCards();
        EXPECT_GT((int)due.size(), 0, "new cards are immediately due");
        EXPECT_EQ(due[0].question, std::string("What is 2+2?"), "first due question matches");

        // ── Test 3: tag filter isolates cards ─────────────────────────────────
        auto math_due = db.getDueCards("math");
        EXPECT_EQ((int)math_due.size(), 1, "math filter returns 1 card");

        auto french_due = db.getDueCards("french");
        EXPECT_EQ((int)french_due.size(), 1, "french filter returns 1 card");

        auto none_due = db.getDueCards("nonexistent");
        EXPECT_EQ((int)none_due.size(), 0, "unknown tag returns 0 cards");

        // ── Test 4: updateFsrsState pushes card to future ─────────────────────
        int64_t future = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count() +
                         3 * 86400; // 3 days from now

        Database::FsrsUpdate upd{3.5, 5.0, 1, 0, 2,
                                  static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                      std::chrono::system_clock::now().time_since_epoch()).count()),
                                  future};
        bool ok = db.updateFsrsState(id1, upd);
        EXPECT(ok, "updateFsrsState succeeds");

        auto due_after = db.getDueCards("math");
        EXPECT_EQ((int)due_after.size(), 0, "updated card no longer due");

        // ── Test 5: getStats counts ───────────────────────────────────────────
        auto stats = db.getStats();
        EXPECT_EQ(stats.total, 2, "stats.total = 2");
        // id2 is still new/due (state=0), id1 is state=2 (review)
        EXPECT_GT(stats.new_cards + stats.review, 0, "stats has new or review cards");

        auto math_stats = db.getStats("math");
        EXPECT_EQ(math_stats.total, 1, "math stats.total=1");

        // ── Test 6: getTagCounts ──────────────────────────────────────────────
        auto tags = db.getTagCounts();
        EXPECT_GT((int)tags.size(), 0, "getTagCounts returns rows");
        // At least math, french, vocab should exist
        bool found_math = false;
        for (auto& tc : tags) { if (tc.name == "math") { found_math = true; } }
        EXPECT(found_math, "math tag appears in getTagCounts");

        // ── Test 7: getCardTags ───────────────────────────────────────────────
        auto card_tags = db.getCardTags(id2);
        EXPECT_EQ((int)card_tags.size(), 2, "id2 has 2 tags");

        // ── Test 8: addTag / removeTag ────────────────────────────────────────
        db.addTag(id1, "extra");
        auto t1 = db.getCardTags(id1);
        EXPECT_EQ((int)t1.size(), 2, "addTag: id1 now has 2 tags (math + extra)");

        db.removeTag(id1, "extra");
        auto t2 = db.getCardTags(id1);
        EXPECT_EQ((int)t2.size(), 1, "removeTag: id1 back to 1 tag");

        // ── Test 9: getCard by id ─────────────────────────────────────────────
        auto card = db.getCard(id2);
        EXPECT_EQ(card.id, id2, "getCard returns correct id");
        EXPECT_EQ(card.question, std::string("Bonjour = ?"), "getCard question matches");
        EXPECT_EQ((int)card.tags.size(), 2, "getCard includes tags");

        // ── Test 10: deleteCard ───────────────────────────────────────────────
        bool del = db.deleteCard(id1);
        EXPECT(del, "deleteCard returns true");

        auto all = db.getAllCards();
        EXPECT_EQ((int)all.size(), 1, "getAllCards has 1 card after delete");

        // Tags should be cascade-deleted
        auto t3 = db.getCardTags(id1);
        EXPECT_EQ((int)t3.size(), 0, "tags cascade-deleted with card");

    } catch (const std::exception& ex) {
        std::cerr << "EXCEPTION: " << ex.what() << '\n';
        ++failed;
    }

    std::filesystem::remove(TEST_DB);

    std::cout << "\n── Database Tests: " << passed << " passed, "
              << failed << " failed ──\n";
    return failed == 0 ? 0 : 1;
}
