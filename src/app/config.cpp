#include "app/config.h"
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace retn {

Config Config::defaults() {
    const char* home = std::getenv("HOME");
    if (!home) throw std::runtime_error("HOME environment variable not set");

    std::filesystem::path dir =
        std::filesystem::path(home) / ".local" / "share" / "retn";
    std::filesystem::create_directories(dir);

    Config cfg;
    cfg.db_path = (dir / "retn.db").string();
    return cfg;
}

Config Config::with_db_path(std::string path) {
    Config cfg;
    cfg.db_path = std::move(path);
    // Ensure parent directory exists.
    auto parent = std::filesystem::path(cfg.db_path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    return cfg;
}

} // namespace retn
