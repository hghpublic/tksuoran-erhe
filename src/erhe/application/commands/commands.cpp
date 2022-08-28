// #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include "erhe/application/commands/commands.hpp"
#include "erhe/application/configuration.hpp"
#include "erhe/application/application_log.hpp"
#include "erhe/application/commands/command.hpp"
#include "erhe/application/commands/command_context.hpp"
#include "erhe/application/commands/key_binding.hpp"
#include "erhe/application/commands/mouse_click_binding.hpp"
#include "erhe/application/commands/mouse_drag_binding.hpp"
#include "erhe/application/commands/mouse_motion_binding.hpp"
#include "erhe/application/commands/mouse_wheel_binding.hpp"
#include "erhe/application/window.hpp"


namespace erhe::application {

Commands::Commands()
    : erhe::components::Component{c_type_name}
{
}

Commands::~Commands() noexcept
{
}

void Commands::declare_required_components()
{
    require<Window>();
}

void Commands::initialize_component()
{
    double mouse_x{};
    double mouse_y{};
    get<Window>()->get_context_window()->get_cursor_position(mouse_x, mouse_y);
    m_last_mouse_position = glm::dvec2{mouse_x, mouse_y};
}

void Commands::post_initialize()
{
    m_configuration  = get<Configuration>();
}

void Commands::register_command(Command* const command)
{
    std::lock_guard<std::mutex> lock{m_command_mutex};

    m_commands.push_back(command);
}

auto Commands::bind_command_to_key(
    Command* const                   command,
    const erhe::toolkit::Keycode     code,
    const bool                       pressed,
    const nonstd::optional<uint32_t> modifier_mask
) -> erhe::toolkit::Unique_id<Key_binding>::id_type
{
    std::lock_guard<std::mutex> lock{m_command_mutex};

    auto& binding = m_key_bindings.emplace_back(command, code, pressed, modifier_mask);
    return binding.get_id();
}

auto Commands::bind_command_to_mouse_click(
    Command* const                    command,
    const erhe::toolkit::Mouse_button button
) -> erhe::toolkit::Unique_id<Mouse_click_binding>::id_type
{
    std::lock_guard<std::mutex> lock{m_command_mutex};

    auto binding = std::make_unique<Mouse_click_binding>(command, button);
    auto id = binding->get_id();
    m_mouse_bindings.push_back(std::move(binding));
    return id;
}

auto Commands::bind_command_to_mouse_wheel(
    Command* const command
) -> erhe::toolkit::Unique_id<Mouse_wheel_binding>::id_type
{
    std::lock_guard<std::mutex> lock{m_command_mutex};

    auto binding = std::make_unique<Mouse_wheel_binding>(command);
    auto id = binding->get_id();
    m_mouse_wheel_bindings.push_back(std::move(binding));
    return id;
}

auto Commands::bind_command_to_mouse_motion(
    Command* const command
) -> erhe::toolkit::Unique_id<Mouse_motion_binding>::id_type
{
    std::lock_guard<std::mutex> lock{m_command_mutex};

    auto binding = std::make_unique<Mouse_motion_binding>(command);
    auto id = binding->get_id();
    m_mouse_bindings.push_back(std::move(binding));
    return id;
}

auto Commands::bind_command_to_mouse_drag(
    Command* const                    command,
    const erhe::toolkit::Mouse_button button
) -> erhe::toolkit::Unique_id<Mouse_drag_binding>::id_type
{
    std::lock_guard<std::mutex> lock{m_command_mutex};

    auto binding = std::make_unique<Mouse_drag_binding>(command, button);
    auto id = binding->get_id();
    m_mouse_bindings.push_back(std::move(binding));
    return id;
}

void Commands::remove_command_binding(
    const erhe::toolkit::Unique_id<Command_binding>::id_type binding_id
)
{
    std::lock_guard<std::mutex> lock{m_command_mutex};

    m_key_bindings.erase(
        std::remove_if(
            m_key_bindings.begin(),
            m_key_bindings.end(),
            [binding_id](const Key_binding& binding)
            {
                return binding.get_id() == binding_id;
            }
        ),
        m_key_bindings.end()
    );
    m_mouse_bindings.erase(
        std::remove_if(
            m_mouse_bindings.begin(),
            m_mouse_bindings.end(),
            [binding_id](const std::unique_ptr<Mouse_binding>& binding)
            {
                return binding.get()->get_id() == binding_id;
            }
        ),
        m_mouse_bindings.end()
    );
    m_mouse_wheel_bindings.erase(
        std::remove_if(
            m_mouse_wheel_bindings.begin(),
            m_mouse_wheel_bindings.end(),
            [binding_id](const std::unique_ptr<Mouse_wheel_binding>& binding)
            {
                return binding.get()->get_id() == binding_id;
            }
        ),
        m_mouse_wheel_bindings.end()
    );
}

void Commands::command_inactivated(Command* const command)
{
    // std::lock_guard<std::mutex> lock{m_command_mutex};

    if (m_active_mouse_command == command)
    {
        m_active_mouse_command = nullptr;
    }
}

[[nodiscard]] auto Commands::mouse_input_sink() const -> Imgui_window*
{
    return m_mouse_input_sink;
}

void Commands::on_key(
    const erhe::toolkit::Keycode code,
    const uint32_t               modifier_mask,
    const bool                   pressed
)
{
    std::lock_guard<std::mutex> lock{m_command_mutex};

    Command_context context{
        *this,
        m_mouse_input_sink
    };

    for (auto& binding : m_key_bindings)
    {
        if (binding.on_key(context, pressed, code, modifier_mask))
        {
            return;
        }
    }

    log_input_event_filtered->trace(
        "key {} {} not consumed",
        erhe::toolkit::c_str(code),
        pressed ? "press" : "release"
    );
}

namespace {

[[nodiscard]] auto get_priority(const State state) -> int
{
    switch (state)
    {
        //using enum State;
        case State::Active:   return 1;
        case State::Ready:    return 2;
        case State::Inactive: return 3;
        case State::Disabled: return 4;
    }
    return 999;
}

};

auto Commands::get_command_priority(Command* const command) const -> int
{
    if (command == m_active_mouse_command)
    {
        return 0;
    }
    return get_priority(command->state());
}

void Commands::sort_mouse_bindings()
{
    std::sort(
        m_mouse_bindings.begin(),
        m_mouse_bindings.end(),
        [this](
            const std::unique_ptr<Mouse_binding>& lhs,
            const std::unique_ptr<Mouse_binding>& rhs
        ) -> bool
        {
            auto* const lhs_command = lhs.get()->get_command();
            auto* const rhs_command = rhs.get()->get_command();
            ERHE_VERIFY(lhs_command != nullptr);
            ERHE_VERIFY(rhs_command != nullptr);
            return get_command_priority(lhs_command) < get_command_priority(rhs_command);
        }
    );
}

void Commands::inactivate_ready_commands()
{
    //std::lock_guard<std::mutex> lock{m_command_mutex};

    Command_context context{
        *this,
        m_mouse_input_sink
    };
    for (auto* command : m_commands)
    {
        if (command->state() == State::Ready)
        {
            command->set_inactive(context);
        }
    }
}

void Commands::set_mouse_input_sink(Imgui_window* mouse_input_sink)
{
    m_mouse_input_sink = mouse_input_sink;

//// #if defined(ERHE_GUI_LIBRARY_IMGUI)
////     if (
////         m_configuration->imgui.window_viewport &&
////         (mouse_input_sink != nullptr)
////     )
////     {
////         m_window_position           = glm::vec2{ImGui::GetWindowPos()};
////         m_window_size               = glm::vec2{ImGui::GetWindowSize()};
////         m_window_content_region_min = glm::vec2{ImGui::GetWindowContentRegionMin()};
////         m_window_content_region_max = glm::vec2{ImGui::GetWindowContentRegionMax()};
////     }
////     else
//// #endif
////     {
////         m_window_position = glm::vec2{0.0f, 0.0f};
////         m_window_size     = glm::vec2{0.0f, 0.0f};
////     }
}

auto Commands::last_mouse_position() const -> glm::dvec2
{
    return m_last_mouse_position;
}

auto Commands::last_mouse_position_delta() const -> glm::dvec2
{
    return m_last_mouse_position_delta;
}

auto Commands::last_mouse_wheel_delta() const -> glm::dvec2
{
    return m_last_mouse_wheel_delta;
}

void Commands::update_active_mouse_command(
    Command* const command)
{
    inactivate_ready_commands();

    if (
        (command->state() == State::Active) &&
        (m_active_mouse_command != command)
    )
    {
        ERHE_VERIFY(m_active_mouse_command == nullptr);
        m_active_mouse_command = command;
    }
    else if (
        (command->state() != State::Active) &&
        (m_active_mouse_command == command)
    )
    {
        m_active_mouse_command = nullptr;
    }
}

void Commands::on_mouse_click(
    const erhe::toolkit::Mouse_button button,
    const int                         count
)
{
    std::lock_guard<std::mutex> lock{m_command_mutex};

    sort_mouse_bindings();

////     if (m_active_mouse_command == nullptr)
////     {
////         return;
////     }

    Command_context context{
        *this,
        m_mouse_input_sink,
        m_last_mouse_position
    };
    for (const auto& binding : m_mouse_bindings)
    {
        auto* const command = binding->get_command();
        ERHE_VERIFY(command != nullptr);
        if (binding->on_button(context, button, count))
        {
            update_active_mouse_command(command);
            break;
        }
    }
}

void Commands::on_mouse_wheel(const double x, const double y)
{
    std::lock_guard<std::mutex> lock{m_command_mutex};

    sort_mouse_bindings();

    //// if (m_active_mouse_command == nullptr)
    //// {
    ////     return;
    //// }

    m_last_mouse_wheel_delta.x = x;
    m_last_mouse_wheel_delta.y = y;

    Command_context context{
        *this,
        m_mouse_input_sink,
        m_last_mouse_position,
        m_last_mouse_wheel_delta
    };
    for (const auto& binding : m_mouse_wheel_bindings)
    {
        auto* const command = binding->get_command();
        ERHE_VERIFY(command != nullptr);
        binding->on_wheel(context); // does not set active mouse command - each wheel event is one shot
    }
}

void Commands::on_mouse_move(const double x, const double y)
{
    std::lock_guard<std::mutex> lock{m_command_mutex};

    glm::dvec2 new_mouse_position{x, y};
    m_last_mouse_position_delta = m_last_mouse_position - new_mouse_position;

    m_last_mouse_position = new_mouse_position;
    Command_context context{
        *this,
        m_mouse_input_sink,
        m_last_mouse_position,
        m_last_mouse_position_delta
    };

    for (const auto& binding : m_mouse_bindings)
    {
        auto* const command = binding->get_command();
        ERHE_VERIFY(command != nullptr);
        if (binding->on_motion(context))
        {
            update_active_mouse_command(command);
            break;
        }
    }
}

}  // namespace editor
