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
    state       INTEGER DEFAULT 0,   -- 0=new 1=learning 2=review 3=relearning
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
