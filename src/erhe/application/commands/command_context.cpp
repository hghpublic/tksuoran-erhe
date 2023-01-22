#include "erhe/application/commands/command_context.hpp"
#include "erhe/application/commands/command.hpp"
#include "erhe/application/commands/commands.hpp"
#include "erhe/application/application_view.hpp"

namespace erhe::application
{

Command_context::Command_context(
    const uint32_t button_bits,
    glm::dvec2     vec2_absolute_value,
    glm::dvec2     vec2_relative_value
)
    : m_button_bits        {button_bits}
    , m_vec2_absolute_value{vec2_absolute_value}
    , m_vec2_relative_value{vec2_relative_value}
{
}

auto Command_context::get_button_bits() const -> uint32_t
{
    return m_button_bits;
}

auto Command_context::get_vec2_absolute_value() const -> glm::dvec2
{
    return m_vec2_absolute_value;
}

auto Command_context::get_vec2_relative_value() const -> glm::dvec2
{
    return m_vec2_relative_value;
}

auto Command_context::accept_mouse_command(Command* command) -> bool
{
    return g_commands->accept_mouse_command(command);
}

auto Command_context::accept_controller_trigger_command(Command* command) -> bool
{
    return g_commands->accept_controller_trigger_command(command);
}

} // namespace erhe::application

