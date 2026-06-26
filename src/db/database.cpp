#include "db/database.h"
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace retn {

// ── Embedded schema ───────────────────────────────────────────────────────────
static constexpr const char* SCHEMA = R"SQL(
PRAGMA journal_mode = WAL;
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS cards (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    question    TEXT    NOT NULL,
    answer      TEXT    NOT NULL,
    stability   REAL    DEFAULT 0.0,
    difficulty  REAL    DEFAULT 0.0,
    reps        INTEGER DEFAULT 0,
    lapses      INTEGER DEFAULT 0,
    state       INTEGER DEFAULT 0,
    last_review INTEGER DEFAULT 0,
    next_review INTEGER DEFAULT (strftime('%s','now')),
    created_at  INTEGER DEFAULT (strftime('%s','now'))
);

CREATE TABLE IF NOT EXISTS tags (
    card_id  INTEGER NOT NULL,
    tag_name TEXT    NOT NULL,
    PRIMARY KEY (card_id, tag_name),
    FOREIGN KEY (card_id) REFERENCES cards(id) ON DELETE CASCADE
);
)SQL";

// ── Helpers ───────────────────────────────────────────────────────────────────
static void check(int rc, sqlite3* db, const char* ctx) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        throw std::runtime_error(std::string(ctx) + ": " + sqlite3_errmsg(db));
    }
}

static Card rowToCard(sqlite3_stmt* s) {
    Card c;
    c.id          = sqlite3_column_int64(s, 0);
    auto q        = sqlite3_column_text(s, 1);
    c.question    = q ? reinterpret_cast<const char*>(q) : "";
    auto a        = sqlite3_column_text(s, 2);
    c.answer      = a ? reinterpret_cast<const char*>(a) : "";
    c.stability   = sqlite3_column_double(s, 3);
    c.difficulty  = sqlite3_column_double(s, 4);
    c.reps        = sqlite3_column_int(s, 5);
    c.lapses      = sqlite3_column_int(s, 6);
    c.state       = sqlite3_column_int(s, 7);
    c.last_review = sqlite3_column_int64(s, 8);
    c.next_review = sqlite3_column_int64(s, 9);
    c.created_at  = sqlite3_column_int64(s, 10);
    return c;
}

// ── Constructor / Destructor ──────────────────────────────────────────────────
Database::Database(const std::string& path) {
    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);

    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        throw std::runtime_error("Cannot open DB at '" + path + "': " + err);
    }
    // 5-second busy timeout
    sqlite3_busy_timeout(db_, 5000);
    runSchema();
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void Database::runSchema() {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, SCHEMA, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string err = errmsg ? errmsg : "unknown";
        sqlite3_free(errmsg);
        throw std::runtime_error("Schema migration failed: " + err);
    }
}

Database::StmtPtr Database::prepare(const char* sql) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    check(rc, db_, sql);
    return StmtPtr(stmt);
}

int64_t Database::nowSec() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// ── Card CRUD ─────────────────────────────────────────────────────────────────
int64_t Database::addCard(const std::string& question, const std::string& answer,
                           const std::vector<std::string>& tags) {
    int64_t now = nowSec();
    auto stmt = prepare(
        "INSERT INTO cards (question, answer, next_review, created_at) VALUES (?,?,?,?)");
    sqlite3_bind_text(stmt.get(), 1, question.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, answer.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 3, now);
    sqlite3_bind_int64(stmt.get(), 4, now);
    check(sqlite3_step(stmt.get()), db_, "addCard insert");

    int64_t id = sqlite3_last_insert_rowid(db_);
    for (const auto& t : tags) addTag(id, t);
    return id;
}

bool Database::deleteCard(int64_t id) {
    auto stmt = prepare("DELETE FROM cards WHERE id=?");
    sqlite3_bind_int64(stmt.get(), 1, id);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

Card Database::getCard(int64_t id) {
    auto stmt = prepare(
        "SELECT id,question,answer,stability,difficulty,reps,lapses,"
        "state,last_review,next_review,created_at FROM cards WHERE id=?");
    sqlite3_bind_int64(stmt.get(), 1, id);
    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        auto c = rowToCard(stmt.get());
        c.tags = getCardTags(id);
        return c;
    }
    return {};
}

// ── Queries ───────────────────────────────────────────────────────────────────
std::vector<Card> Database::getDueCards(const std::string& tag_filter, int limit) {
    int64_t now = nowSec();

    StmtPtr stmt;
    if (tag_filter.empty()) {
        stmt = prepare(
            "SELECT c.id,c.question,c.answer,c.stability,c.difficulty,c.reps,c.lapses,"
            "c.state,c.last_review,c.next_review,c.created_at "
            "FROM cards c WHERE c.next_review<=? ORDER BY c.next_review ASC LIMIT ?");
        sqlite3_bind_int64(stmt.get(), 1, now);
        sqlite3_bind_int(stmt.get(), 2, limit);
    } else {
        stmt = prepare(
            "SELECT c.id,c.question,c.answer,c.stability,c.difficulty,c.reps,c.lapses,"
            "c.state,c.last_review,c.next_review,c.created_at "
            "FROM cards c INNER JOIN tags t ON c.id=t.card_id "
            "WHERE c.next_review<=? AND t.tag_name=? "
            "ORDER BY c.next_review ASC LIMIT ?");
        sqlite3_bind_int64(stmt.get(), 1, now);
        sqlite3_bind_text(stmt.get(), 2, tag_filter.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt.get(), 3, limit);
    }

    std::vector<Card> out;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        auto c = rowToCard(stmt.get());
        c.tags = getCardTags(c.id);
        out.push_back(std::move(c));
    }
    return out;
}

std::vector<Card> Database::getAllCards() {
    auto stmt = prepare(
        "SELECT id,question,answer,stability,difficulty,reps,lapses,"
        "state,last_review,next_review,created_at FROM cards ORDER BY created_at DESC");
    std::vector<Card> out;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) out.push_back(rowToCard(stmt.get()));
    return out;
}

// ── Stats ─────────────────────────────────────────────────────────────────────
DeckStats Database::getStats(const std::string& f) {
    int64_t now  = nowSec();
    int64_t week = now + 7 * 86400;
    bool    hf   = !f.empty();
    DeckStats s{};

    // -- counts by state --
    {
        StmtPtr stmt;
        if (hf) {
            stmt = prepare(
                "SELECT COUNT(*)"
                ",SUM(CASE WHEN c.state=0 THEN 1 ELSE 0 END)"
                ",SUM(CASE WHEN c.state=1 OR c.state=3 THEN 1 ELSE 0 END)"
                ",SUM(CASE WHEN c.state=2 THEN 1 ELSE 0 END)"
                " FROM cards c INNER JOIN tags t ON c.id=t.card_id"
                " WHERE t.tag_name=?");
            sqlite3_bind_text(stmt.get(), 1, f.c_str(), -1, SQLITE_TRANSIENT);
        } else {
            stmt = prepare(
                "SELECT COUNT(*)"
                ",SUM(CASE WHEN state=0 THEN 1 ELSE 0 END)"
                ",SUM(CASE WHEN state=1 OR state=3 THEN 1 ELSE 0 END)"
                ",SUM(CASE WHEN state=2 THEN 1 ELSE 0 END)"
                " FROM cards");
        }
        if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            s.total    = sqlite3_column_int(stmt.get(), 0);
            s.new_cards = sqlite3_column_int(stmt.get(), 1);
            s.learning  = sqlite3_column_int(stmt.get(), 2);
            s.review    = sqlite3_column_int(stmt.get(), 3);
        }
    }

    // -- mature (stability >= 21 days) --
    {
        StmtPtr stmt;
        if (hf) {
            stmt = prepare(
                "SELECT COUNT(*) FROM cards c INNER JOIN tags t ON c.id=t.card_id"
                " WHERE t.tag_name=? AND c.state=2 AND c.stability>=21.0");
            sqlite3_bind_text(stmt.get(), 1, f.c_str(), -1, SQLITE_TRANSIENT);
        } else {
            stmt = prepare(
                "SELECT COUNT(*) FROM cards WHERE state=2 AND stability>=21.0");
        }
        if (sqlite3_step(stmt.get()) == SQLITE_ROW)
            s.mature = sqlite3_column_int(stmt.get(), 0);
    }

    // -- due now --
    {
        StmtPtr stmt;
        if (hf) {
            stmt = prepare(
                "SELECT COUNT(*) FROM cards c INNER JOIN tags t ON c.id=t.card_id"
                " WHERE t.tag_name=? AND c.next_review<=?");
            sqlite3_bind_text(stmt.get(), 1, f.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt.get(), 2, now);
        } else {
            stmt = prepare("SELECT COUNT(*) FROM cards WHERE next_review<=?");
            sqlite3_bind_int64(stmt.get(), 1, now);
        }
        if (sqlite3_step(stmt.get()) == SQLITE_ROW)
            s.due_now = sqlite3_column_int(stmt.get(), 0);
    }

    // -- due within 7 days --
    {
        StmtPtr stmt;
        if (hf) {
            stmt = prepare(
                "SELECT COUNT(*) FROM cards c INNER JOIN tags t ON c.id=t.card_id"
                " WHERE t.tag_name=? AND c.next_review<=?");
            sqlite3_bind_text(stmt.get(), 1, f.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt.get(), 2, week);
        } else {
            stmt = prepare("SELECT COUNT(*) FROM cards WHERE next_review<=?");
            sqlite3_bind_int64(stmt.get(), 1, week);
        }
        if (sqlite3_step(stmt.get()) == SQLITE_ROW)
            s.due_next7d = sqlite3_column_int(stmt.get(), 0);
    }

    return s;
}

std::vector<TagCount> Database::getTagCounts() {
    int64_t now = nowSec();
    auto stmt = prepare(
        "SELECT t.tag_name, COUNT(*), SUM(CASE WHEN c.next_review<=? THEN 1 ELSE 0 END)"
        " FROM tags t INNER JOIN cards c ON t.card_id=c.id"
        " GROUP BY t.tag_name ORDER BY 3 DESC, 1 ASC");
    sqlite3_bind_int64(stmt.get(), 1, now);

    std::vector<TagCount> out;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        auto n = sqlite3_column_text(stmt.get(), 0);
        out.push_back({
            n ? reinterpret_cast<const char*>(n) : "",
            sqlite3_column_int(stmt.get(), 1),
            sqlite3_column_int(stmt.get(), 2)
        });
    }
    return out;
}

// ── FSRS update ───────────────────────────────────────────────────────────────
bool Database::updateFsrsState(int64_t card_id, const FsrsUpdate& u) {
    auto stmt = prepare(
        "UPDATE cards SET stability=?,difficulty=?,reps=?,lapses=?,"
        "state=?,last_review=?,next_review=? WHERE id=?");
    sqlite3_bind_double(stmt.get(), 1, u.stability);
    sqlite3_bind_double(stmt.get(), 2, u.difficulty);
    sqlite3_bind_int(stmt.get(), 3, u.reps);
    sqlite3_bind_int(stmt.get(), 4, u.lapses);
    sqlite3_bind_int(stmt.get(), 5, u.state);
    sqlite3_bind_int64(stmt.get(), 6, u.last_review);
    sqlite3_bind_int64(stmt.get(), 7, u.next_review);
    sqlite3_bind_int64(stmt.get(), 8, card_id);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

// ── Tag management ────────────────────────────────────────────────────────────
bool Database::addTag(int64_t card_id, const std::string& tag) {
    if (tag.empty()) return false;
    auto stmt = prepare("INSERT OR IGNORE INTO tags (card_id,tag_name) VALUES (?,?)");
    sqlite3_bind_int64(stmt.get(), 1, card_id);
    sqlite3_bind_text(stmt.get(), 2, tag.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

bool Database::removeTag(int64_t card_id, const std::string& tag) {
    auto stmt = prepare("DELETE FROM tags WHERE card_id=? AND tag_name=?");
    sqlite3_bind_int64(stmt.get(), 1, card_id);
    sqlite3_bind_text(stmt.get(), 2, tag.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(stmt.get()) == SQLITE_DONE;
}

std::vector<std::string> Database::getCardTags(int64_t card_id) {
    auto stmt = prepare(
        "SELECT tag_name FROM tags WHERE card_id=? ORDER BY tag_name");
    sqlite3_bind_int64(stmt.get(), 1, card_id);
    std::vector<std::string> out;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        auto t = sqlite3_column_text(stmt.get(), 0);
        if (t) out.emplace_back(reinterpret_cast<const char*>(t));
    }
    return out;
}

} // namespace retn
