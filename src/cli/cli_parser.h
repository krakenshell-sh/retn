#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace retn {

struct ParsedCommand {
    enum class Type {
        Add,
        AddInteractive,
        Delete,
        Filter,
        FilterAll,
        Help,
        Unknown,
    };

    Type        type{Type::Unknown};
    std::string question;
    std::string answer;
    std::string tag;       // first --tag value for Add
    int64_t     card_id{-1};
    bool        error{false};
    std::string error_msg;
};

class CLIParser {
public:
    /// Parse a command string typed into the command bar.
    /// Supports:
    ///   add "Q" "A" [--tag t]
    ///   add --interactive
    ///   delete <id>
    ///   filter <tag> | filter all
    ///   help
    ParsedCommand parse(const std::string& input) const;

private:
    std::vector<std::string> tokenize(const std::string& input) const;
};

} // namespace retn
