#pragma once

#include "tools/transform/subtool.hpp"
#include "tools/tool.hpp"

#include "erhe/components/components.hpp"

#include <string_view>

namespace editor
{

enum class Handle : unsigned int;

class Rotate_tool
    : public erhe::components::Component
    , public Tool
    , public Subtool
{
public:
    static constexpr int              c_priority {1};
    static constexpr std::string_view c_type_name{"Rotate_tool"};
    static constexpr std::string_view c_title    {"Rotate"};
    static constexpr uint32_t         c_type_hash{
        compiletime_xxhash::xxh32(
            c_type_name.data(),
            c_type_name.size(),
            {}
        )
    };

    Rotate_tool ();
    ~Rotate_tool() noexcept override;

    // Implements Component
    [[nodiscard]] auto get_type_hash() const -> uint32_t override { return c_type_hash; }
    void declare_required_components() override;
    void initialize_component       () override;
    void deinitialize_component     () override;

    // Implements Tool
    void handle_priority_update(int old_priority, int new_priority) override;

    // Implemennts Subtool
    void imgui ()                                                       override;
    auto begin (unsigned int axis_mask, Scene_view* scene_view) -> bool override;
    auto update(Scene_view* scene_view) -> bool                         override;

    // Public API (mostly for Transform_tool
    void render(const Render_context& context);

private:
    auto update_circle_around(Scene_view* scene_view) -> bool;
    auto update_parallel     (Scene_view* scene_view) -> bool;
    void update_final        ();

    [[nodiscard]] auto snap(float angle_radians) const -> float;

    int                      m_rotate_snap_index{2};
    glm::vec3                m_normal              {0.0f}; // also rotation axis
    glm::vec3                m_reference_direction {0.0f};
    glm::vec3                m_center_of_rotation  {0.0f};
    std::optional<glm::vec3> m_intersection        {};
    float                    m_start_rotation_angle{0.0f};
    float                    m_current_angle       {0.0f};
};

extern Rotate_tool* g_rotate_tool;

} // namespace editor
