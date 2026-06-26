#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <sqlite3.h>

namespace retn {

// ── Domain types ──────────────────────────────────────────────────────────────
struct Card {
    int64_t     id{0};
    std::string question;
    std::string answer;
    double      stability{0.0};
    double      difficulty{0.0};
    int         reps{0};
    int         lapses{0};
    int         state{0};        // matches CardState enum
    int64_t     last_review{0};
    int64_t     next_review{0};
    int64_t     created_at{0};
    std::vector<std::string> tags;
};

struct TagCount {
    std::string name;
    int         total_count{0};
    int         due_count{0};
};

struct DeckStats {
    int total{0};
    int new_cards{0};
    int learning{0};    // state 1 or 3
    int review{0};      // state 2
    int mature{0};      // state 2 AND stability >= 21
    int due_now{0};
    int due_next7d{0};
};

// ── Database ──────────────────────────────────────────────────────────────────
class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    // ── Card CRUD ─────────────────────────────────────────────────────────────
    int64_t addCard(const std::string& question, const std::string& answer,
                    const std::vector<std::string>& tags = {});
    bool    deleteCard(int64_t id);
    Card    getCard(int64_t id);

    // ── Queries ───────────────────────────────────────────────────────────────
    /// Cards whose next_review <= now, filtered by optional tag, up to limit.
    std::vector<Card> getDueCards(const std::string& tag_filter = "", int limit = 50);
    std::vector<Card> getAllCards();

    // ── Stats ─────────────────────────────────────────────────────────────────
    DeckStats              getStats(const std::string& tag_filter = "");
    std::vector<TagCount>  getTagCounts();

    // ── FSRS state update ─────────────────────────────────────────────────────
    struct FsrsUpdate {
        double  stability;
        double  difficulty;
        int     reps;
        int     lapses;
        int     state;
        int64_t last_review;
        int64_t next_review;
    };
    bool updateFsrsState(int64_t card_id, const FsrsUpdate& upd);

    // ── Tag management ────────────────────────────────────────────────────────
    bool                     addTag(int64_t card_id, const std::string& tag);
    bool                     removeTag(int64_t card_id, const std::string& tag);
    std::vector<std::string> getCardTags(int64_t card_id);

private:
    sqlite3* db_{nullptr};

    struct StmtDeleter { void operator()(sqlite3_stmt* s) { sqlite3_finalize(s); } };
    using StmtPtr = std::unique_ptr<sqlite3_stmt, StmtDeleter>;

    StmtPtr prepare(const char* sql);
    void    runSchema();
    int64_t nowSec() const;
};

} // namespace retn
