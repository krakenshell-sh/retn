#include "cli/cli_parser.h"
#include <algorithm>
#include <stdexcept>

namespace retn {

// ── Tokenizer: space-split with double/single-quote grouping ─────────────────
std::vector<std::string> CLIParser::tokenize(const std::string& input) const {
    std::vector<std::string> tokens;
    std::string cur;
    bool   in_quote   = false;
    char   quote_char = 0;

    for (char c : input) {
        if (in_quote) {
            if (c == quote_char) {
                in_quote = false;
                tokens.push_back(cur);
                cur.clear();
            } else {
                cur += c;
            }
        } else if (c == '"' || c == '\'') {
            in_quote   = true;
            quote_char = c;
        } else if (c == ' ' || c == '\t') {
            if (!cur.empty()) {
                tokens.push_back(cur);
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

// ── Parser ────────────────────────────────────────────────────────────────────
ParsedCommand CLIParser::parse(const std::string& input) const {
    ParsedCommand cmd;
    auto tokens = tokenize(input);

    if (tokens.empty()) {
        cmd.error     = true;
        cmd.error_msg = "Empty command";
        return cmd;
    }

    std::string verb = tokens[0];
    // lowercase verb
    std::transform(verb.begin(), verb.end(), verb.begin(), ::tolower);

    if (verb == "add") {
        if (tokens.size() >= 2 && tokens[1] == "--interactive") {
            cmd.type = ParsedCommand::Type::AddInteractive;
            return cmd;
        }
        // add [Q] [A] [--tag T] ...
        cmd.type = ParsedCommand::Type::Add;
        size_t pos = 1;
        // question
        if (pos < tokens.size() && tokens[pos].rfind("--", 0) != 0)
            cmd.question = tokens[pos++];
        // answer
        if (pos < tokens.size() && tokens[pos].rfind("--", 0) != 0)
            cmd.answer = tokens[pos++];
        // flags
        while (pos < tokens.size()) {
            if (tokens[pos] == "--tag" && pos + 1 < tokens.size()) {
                cmd.tag = tokens[++pos];
            }
            ++pos;
        }
        if (cmd.question.empty()) {
            cmd.error     = true;
            cmd.error_msg = "add: question text is required";
        }

    } else if (verb == "delete" || verb == "del") {
        cmd.type = ParsedCommand::Type::Delete;
        if (tokens.size() < 2) {
            cmd.error     = true;
            cmd.error_msg = "delete: card id is required";
            return cmd;
        }
        try {
            cmd.card_id = std::stoll(tokens[1]);
        } catch (...) {
            cmd.error     = true;
            cmd.error_msg = "delete: invalid id '" + tokens[1] + "'";
        }

    } else if (verb == "filter") {
        if (tokens.size() < 2) {
            cmd.error     = true;
            cmd.error_msg = "filter: tag name or 'all' required";
            return cmd;
        }
        if (tokens[1] == "all") {
            cmd.type = ParsedCommand::Type::FilterAll;
        } else {
            cmd.type = ParsedCommand::Type::Filter;
            // strip leading # if present
            cmd.tag = (tokens[1][0] == '#') ? tokens[1].substr(1) : tokens[1];
        }

    } else if (verb == "help" || verb == "?") {
        cmd.type = ParsedCommand::Type::Help;

    } else {
        cmd.type      = ParsedCommand::Type::Unknown;
        cmd.error     = true;
        cmd.error_msg = "Unknown command '" + verb + "'. Type 'help'.";
    }

    return cmd;
}

} // namespace retn
