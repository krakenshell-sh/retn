#pragma once
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <string>

namespace retn::ui {

// ── Nordic Monochrome palette ─────────────────────────────────────────────────
namespace pal {
    inline const ftxui::Color Bg        = ftxui::Color::RGB(0x1e, 0x1e, 0x2e);
    inline const ftxui::Color Muted     = ftxui::Color::RGB(0x6c, 0x70, 0x86);
    inline const ftxui::Color Primary   = ftxui::Color::RGB(0xcd, 0xd6, 0xf4);
    inline const ftxui::Color Highlight = ftxui::Color::RGB(0x89, 0xb4, 0xfa);
    inline const ftxui::Color Success   = ftxui::Color::RGB(0xa6, 0xe3, 0xa1);
    inline const ftxui::Color Warn      = ftxui::Color::RGB(0xf9, 0xe2, 0xaf);
    inline const ftxui::Color Err       = ftxui::Color::RGB(0xf3, 0x8b, 0xa8);
} // namespace pal

// ── Shared element builders ───────────────────────────────────────────────────

/// Dimmed key-hint label: "[key] description"
ftxui::Element keyHint(const std::string& key, const std::string& desc);

/// Pair: muted label + primary value, left-padded by `pad` spaces.
ftxui::Element statRow(int pad, const std::string& label, const std::string& value,
                        bool highlight = false);

/// Rating button: "[N] Label (interval)"
ftxui::Element ratingButton(int num, const std::string& label, const std::string& interval);

/// Centered modal dialog box (use inside dbox).
ftxui::Element modalBox(const std::string& title, ftxui::Elements body_lines);

/// Single-line status bar element (empty if msg is blank).
ftxui::Element statusLine(const std::string& msg);

/// Horizontal separator with Unicode box chars (always renders as ─).
ftxui::Element hSep();

} // namespace retn::ui
