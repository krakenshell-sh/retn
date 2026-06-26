#pragma once
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <string>

namespace retn::ui {

/// Individual FTXUI components returned by makeAdderComponent.
/// container = CatchEvent wrapper used for event routing in Application.
/// q/a/t_input = individual Input components exposed for per-field rendering.
struct AdderComponents {
    ftxui::Component container;
    ftxui::Component q_input;
    ftxui::Component a_input;
    ftxui::Component t_input;
};

/// Builds the interactive adder form.
/// Strings are owned by the caller; callbacks invoked on save/cancel.
AdderComponents makeAdderComponent(std::string*          question,
                                    std::string*          answer,
                                    std::string*          tags,
                                    std::function<void()> on_save,
                                    std::function<void()> on_cancel);

/// Renders the adder form with three separately-labelled fields.
/// Pass the .Render() output of q_input / a_input / t_input individually.
ftxui::Element renderAdder(ftxui::Element     q_elem,
                            ftxui::Element     a_elem,
                            ftxui::Element     t_elem,
                            const std::string& status_msg,
                            bool               cmd_active,
                            const std::string& cmd_text);

} // namespace retn::ui
