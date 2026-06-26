#include "app/application.h"
#include "app/config.h"
#include <cstring>
#include <iostream>
#include <string>

static void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [--db <path>]\n\n"
              << "  --db <path>   Use a custom database file\n"
              << "  --help        Show this message\n\n"
              << "Without arguments, retn uses $HOME/.local/share/retn/retn.db\n";
}

int main(int argc, char* argv[]) {
    retn::Config cfg;
    bool         custom_db = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h")     == 0) {
            printUsage(argv[0]);
            return 0;
        }
        if ((std::strcmp(argv[i], "--db") == 0 ||
             std::strcmp(argv[i], "-d")   == 0) &&
            i + 1 < argc) {
            cfg       = retn::Config::with_db_path(argv[++i]);
            custom_db = true;
        }
    }

    if (!custom_db) {
        try {
            cfg = retn::Config::defaults();
        } catch (const std::exception& ex) {
            std::cerr << "Error resolving default config: " << ex.what() << '\n';
            return 1;
        }
    }

    try {
        retn::Application app(std::move(cfg));
        app.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
