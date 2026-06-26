#include "ui/adder.h"
#include "ui/components.h"
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace retn::ui {

// ── Component factory ─────────────────────────────────────────────────────────
AdderComponents makeAdderComponent(std::string*          question,
                                    std::string*          answer,
                                    std::string*          tags,
                                    std::function<void()> on_save,
                                    std::function<void()> on_cancel) {
    auto q = Input(question, "Question...");

    InputOption ao;
    ao.multiline = true;
    auto a = Input(answer, "Answer (Enter = newline)...", ao);

    auto t = Input(tags, "Tags separated by spaces, no # prefix...");

    auto container = Container::Vertical({q, a, t});

    // Wrap with CatchEvent so Ctrl+D / Esc are captured before reaching inputs.
    // Application's outer CatchEvent intercepts these first anyway, but this
    // provides defense-in-depth if routing changes.
    auto wrapped = CatchEvent(container, [on_save, on_cancel](Event e) -> bool {
        if (e == Event::Special(std::string{'\x04'})) { on_save();   return true; }
        if (e == Event::Escape)                        { on_cancel(); return true; }
        return false;
    });

    return AdderComponents{wrapped, q, a, t};
}

// ── Renderer ──────────────────────────────────────────────────────────────────
Element renderAdder(Element        q_elem,
                     Element        a_elem,
                     Element        t_elem,
                     const std::string& status_msg,
                     bool               cmd_active,
                     const std::string& cmd_text) {
    // Header
    auto hdr = hbox({
        text("  ADD NEW CARD") | bold | color(pal::Primary),
        filler(),
        text("[Ctrl+D] Save   [Esc] Cancel") | color(pal::Muted),
        text("  "),
    });

    // Three separately-labelled fields
    auto field = [](const std::string& label, Element input_elem) -> Element {
        return vbox({
            hbox({text("  " + label) | bold | color(pal::Muted), filler()}),
            hbox({text("  "), input_elem | flex}) | border,
        });
    };

    auto form = vbox({
        separatorEmpty(),
        field("Question :", q_elem),
        separatorEmpty(),
        field("Answer   :", a_elem | flex),   // flex → multiline grows
        separatorEmpty(),
        field("Tags     :", t_elem),
        separatorEmpty(),
    }) | flex;

    // Footer: hint row always visible, input row shows command or tab hint
    auto footer_hint = hbox({
        text("  [Tab] Next field  [Shift+Tab] Prev field") | color(pal::Muted),
        filler(),
    });

    Element footer_input;
    if (cmd_active) {
        footer_input = hbox({
            text("  >> ") | color(pal::Muted),
            text(cmd_text) | color(pal::Primary),
            text("▌") | color(pal::Highlight),
            filler(),
        });
    } else {
        footer_input = hbox({
            text("  >> ") | color(pal::Muted),
            text("_") | color(pal::Muted),
            filler(),
        });
    }

    return vbox({
        hdr,
        separator(),
        form,
        separator(),
        statusLine(status_msg),
        separator(),
        footer_hint,
        footer_input,
    }) | border | flex;
}

} // namespace retn::ui
