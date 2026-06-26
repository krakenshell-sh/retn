#pragma once
#include <string>

namespace retn {

struct Config {
    std::string db_path;

    /// Default: $HOME/.local/share/retn/retn.db
    static Config defaults();

    /// Override the database path (e.g. from --db CLI flag).
    static Config with_db_path(std::string path);
};

} // namespace retn
