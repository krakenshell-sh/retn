cmake_minimum_required(VERSION 3.20)
project(retn VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ────────────────────────────────────────────────────────────
find_package(SQLite3 REQUIRED)

include(FetchContent)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
    GIT_TAG        v5.0.0
    GIT_SHALLOW    TRUE
)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftxui)

# ── Main executable ──────────────────────────────────────────────────────────
add_executable(retn
    src/main.cpp
    src/app/application.cpp
    src/app/config.cpp
    src/db/database.cpp
    src/db/fsrs.cpp
    src/ui/components.cpp
    src/ui/dashboard.cpp
    src/ui/review.cpp
    src/ui/adder.cpp
    src/ui/command_bar.cpp
    src/cli/cli_parser.cpp
)

target_include_directories(retn PRIVATE src)
target_link_libraries(retn PRIVATE
    SQLite::SQLite3
    ftxui::screen
    ftxui::dom
    ftxui::component
)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()

add_executable(test_fsrs
    tests/test_fsrs.cpp
    src/db/fsrs.cpp
)
target_include_directories(test_fsrs PRIVATE src)
add_test(NAME test_fsrs COMMAND test_fsrs)

add_executable(test_database
    tests/test_database.cpp
    src/db/database.cpp
    src/db/fsrs.cpp
    src/app/config.cpp
)
target_include_directories(test_database PRIVATE src)
target_link_libraries(test_database PRIVATE SQLite::SQLite3)
add_test(NAME test_database COMMAND test_database)
