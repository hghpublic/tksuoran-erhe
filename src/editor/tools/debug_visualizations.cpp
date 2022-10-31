#include "tools/debug_visualizations.hpp"

#include "editor_log.hpp"
#include "editor_rendering.hpp"
#include "renderers/render_context.hpp"
#include "renderers/shadow_renderer.hpp"
#include "rendergraph/shadow_render_node.hpp"
#include "scene/node_physics.hpp"
#include "scene/node_raytrace.hpp"
#include "scene/scene_root.hpp"
#include "scene/viewport_window.hpp"
#include "tools/selection_tool.hpp"
#include "tools/tools.hpp"
#include "tools/trs_tool.hpp"
#include "windows/viewport_config.hpp"

#include "erhe/application/renderers/line_renderer.hpp"
#include "erhe/application/time.hpp"
#include "erhe/application/view.hpp"
#include "erhe/application/imgui/imgui_window.hpp"
#include "erhe/application/imgui/imgui_windows.hpp"
#include "erhe/primitive/primitive_geometry.hpp"
#include "erhe/raytrace/iinstance.hpp"
#include "erhe/scene/camera.hpp"
#include "erhe/scene/light.hpp"
#include "erhe/scene/mesh.hpp"
#include "erhe/scene/scene.hpp"
#include "erhe/toolkit/math_util.hpp"

#if defined(ERHE_GUI_LIBRARY_IMGUI)
#   include <imgui.h>
#endif

namespace editor
{

using glm::mat4;
using glm::vec3;
using glm::vec4;

Debug_visualizations::Debug_visualizations()
    : erhe::application::Imgui_window{c_title}
    , erhe::components::Component    {c_type_name}
{
}

Debug_visualizations::~Debug_visualizations() noexcept
{
}

void Debug_visualizations::declare_required_components()
{
    require<erhe::application::Imgui_windows>();
    require<Tools>();
}

void Debug_visualizations::initialize_component()
{
    get<erhe::application::Imgui_windows>()->register_imgui_window(this);
    get<Tools>()->register_tool(this);
}

void Debug_visualizations::post_initialize()
{
    m_line_renderer_set = get<erhe::application::Line_renderer_set>();
    m_selection_tool    = get<Selection_tool >();
    m_trs_tool          = get<Trs_tool       >();
    m_viewport_config   = get<Viewport_config>();
}

auto Debug_visualizations::description() -> const char*
{
    return c_title.data();
}

namespace
{

auto sign(const float x) -> float
{
    return x < 0.0f ? -1.0f : 1.0f;
}

constexpr vec3 clip_min_corner{-1.0f, -1.0f, 0.0f};
constexpr vec3 clip_max_corner{ 1.0f,  1.0f, 1.0f};
constexpr vec3 O              { 0.0f };
constexpr vec3 axis_x         { 1.0f,  0.0f, 0.0f};
constexpr vec3 axis_y         { 0.0f,  1.0f, 0.0f};
constexpr vec3 axis_z         { 0.0f,  0.0f, 1.0f};

}

void Debug_visualizations::mesh_selection_visualization(
    const Render_context& render_context,
    erhe::scene::Mesh*    mesh
)
{
    auto&          line_renderer = *m_line_renderer_set->hidden.at(2).get();
    const uint32_t major_color   = erhe::toolkit::convert_float4_to_uint32(m_selection_major_color);
    const uint32_t minor_color   = erhe::toolkit::convert_float4_to_uint32(m_selection_minor_color);

    for (const auto& primitive : mesh->mesh_data.primitives)
    {
        if (!primitive.source_geometry)
        {
            continue;
        }
        const auto& primitive_geometry = primitive.gl_primitive_geometry;

        const float box_volume             = primitive_geometry.bounding_box.volume();
        const float sphere_volume          = primitive_geometry.bounding_sphere.volume();
        const bool  smallest_visualization = !m_viewport_config->selection_bounding_box && !m_viewport_config->selection_bounding_sphere;
        const bool  box_smaller            = box_volume < sphere_volume;

        if (box_smaller)
        {
            m_selection_bounding_volume.add_box(
                mesh->world_from_node(),
                primitive_geometry.bounding_box.min,
                primitive_geometry.bounding_box.max
            );
        }
        if ((box_smaller && smallest_visualization) || m_viewport_config->selection_bounding_box)
        {
            line_renderer.set_thickness(m_selection_major_width);
            line_renderer.add_cube(
                mesh->world_from_node(),
                major_color,
                primitive_geometry.bounding_box.min - glm::vec3{m_gap, m_gap, m_gap},
                primitive_geometry.bounding_box.max + glm::vec3{m_gap, m_gap, m_gap}
            );
        }
        if (!box_smaller)
        {
            m_selection_bounding_volume.add_sphere(
                mesh->world_from_node(),
                primitive_geometry.bounding_sphere.center,
                primitive_geometry.bounding_sphere.radius
            );
        }
        if ((!box_smaller && smallest_visualization) || m_viewport_config->selection_bounding_sphere)
        {
            if (render_context.scene_view != nullptr)
            {
                const auto& view_camera = render_context.scene_view->get_camera();
                if (view_camera)
                {
                    const erhe::scene::Transform& camera_world_from_node_transform = view_camera->world_from_node_transform();
                    line_renderer.add_sphere(
                        mesh->world_from_node_transform(),
                        major_color,
                        minor_color,
                        m_selection_major_width,
                        m_selection_minor_width,
                        primitive_geometry.bounding_sphere.center,
                        primitive_geometry.bounding_sphere.radius + m_gap,
                        &camera_world_from_node_transform,
                        m_sphere_step_count
                    );
                }
            }
        }
    }
}

namespace {

[[nodiscard]] auto test_bits(uint64_t mask, uint64_t test_bits) -> bool
{
    return (mask & test_bits) == test_bits;
}

}

void Debug_visualizations::light_visualization(
    const Render_context&                render_context,
    std::shared_ptr<erhe::scene::Camera> selected_camera,
    const erhe::scene::Light*            light
)
{
    if (!test_bits(light->node_data.flag_bits, erhe::scene::Node_flag_bit::show_debug_visualizations))
    {
        return;
    }

    Light_visualization_context light_context
    {
        .render_context   = render_context,
        .selected_camera  = selected_camera,
        .light            = light,
        .light_color      = erhe::toolkit::convert_float4_to_uint32(light->color),
        .half_light_color = erhe::toolkit::convert_float4_to_uint32(
            glm::vec4{0.5f * light->color, 0.5f}
        )
    };

    switch (light->type)
    {
        case erhe::scene::Light_type::directional: directional_light_visualization(light_context); break;
        case erhe::scene::Light_type::point:       point_light_visualization      (light_context); break;
        case erhe::scene::Light_type::spot:        spot_light_visualization       (light_context); break;
        default: break;
    }
}

void Debug_visualizations::directional_light_visualization(
    Light_visualization_context& context
)
{
    const auto shadow_renderer    = get<Shadow_renderer>();
    const auto shadow_render_node = shadow_renderer->get_node_for_view(context.render_context.scene_view);
    if (!shadow_render_node)
    {
        return;
    }

    auto& line_renderer = *m_line_renderer_set->hidden.at(2).get();
    const Light_projections& light_projections           = shadow_render_node->get_light_projections();
    const auto*              light                       = context.light;
    const auto               light_projection_transforms = light_projections.get_light_projection_transforms_for_light(light);

    if (light_projection_transforms == nullptr)
    {
        return;
    }

    const glm::mat4 world_from_light_clip   = light_projection_transforms->clip_from_world.inverse_matrix();
    const glm::mat4 world_from_light_camera = light_projection_transforms->world_from_light_camera.matrix();

    line_renderer.set_thickness(m_light_visualization_width);
    line_renderer.add_cube(
        world_from_light_clip,
        context.light_color,
        clip_min_corner,
        clip_max_corner
    );

    line_renderer.add_lines(
        world_from_light_camera,
        context.light_color,
        {{ O, -1.0f * axis_z }}
    );
}

void Debug_visualizations::point_light_visualization(Light_visualization_context& context)
{
    auto& line_renderer = *m_line_renderer_set->hidden.at(2).get();

    constexpr float scale = 0.5f;
    const auto nnn = scale * glm::normalize(-axis_x - axis_y - axis_z);
    const auto nnp = scale * glm::normalize(-axis_x - axis_y + axis_z);
    const auto npn = scale * glm::normalize(-axis_x + axis_y - axis_z);
    const auto npp = scale * glm::normalize(-axis_x + axis_y + axis_z);
    const auto pnn = scale * glm::normalize( axis_x - axis_y - axis_z);
    const auto pnp = scale * glm::normalize( axis_x - axis_y + axis_z);
    const auto ppn = scale * glm::normalize( axis_x + axis_y - axis_z);
    const auto ppp = scale * glm::normalize( axis_x + axis_y + axis_z);
    line_renderer.set_thickness(m_light_visualization_width);
    line_renderer.add_lines(
        context.light->world_from_node(),
        context.light_color,
        {
            { -scale * axis_x, scale * axis_x},
            { -scale * axis_y, scale * axis_y},
            { -scale * axis_z, scale * axis_z},
            { nnn, ppp },
            { nnp, ppn },
            { npn, pnp },
            { npp, pnn }
        }
    );
}

void Debug_visualizations::spot_light_visualization(Light_visualization_context& context)
{
    auto& line_renderer = *m_line_renderer_set->hidden.at(2).get();
    const Render_context&     render_context = context.render_context;
    const erhe::scene::Light* light = context.light;

    constexpr int   edge_count       = 200;
    constexpr float light_cone_sides = edge_count * 6;
    const float outer_alpha   = light->outer_spot_angle;
    const float inner_alpha   = light->inner_spot_angle;
    const float length        = light->range;
    const float outer_apothem = length * std::tan(outer_alpha * 0.5f);
    const float inner_apothem = length * std::tan(inner_alpha * 0.5f);
    const float outer_radius  = outer_apothem / std::cos(glm::pi<float>() / static_cast<float>(light_cone_sides));
    const float inner_radius  = inner_apothem / std::cos(glm::pi<float>() / static_cast<float>(light_cone_sides));

    const mat4 m             = light->world_from_node();
    const vec3 view_position = light->transform_point_from_world_to_local(
        render_context.camera->position_in_world()
    );

    //auto* editor_time = get<Editor_time>();
    //const float time = static_cast<float>(editor_time->time());
    //const float half_position = (editor_time != nullptr)
    //    ? time - floor(time)
    //    : 0.5f;

    line_renderer.set_thickness(m_light_visualization_width);

    for (int i = 0; i < light_cone_sides; ++i)
    {
        const float t0 = glm::two_pi<float>() * static_cast<float>(i    ) / static_cast<float>(light_cone_sides);
        const float t1 = glm::two_pi<float>() * static_cast<float>(i + 1) / static_cast<float>(light_cone_sides);
        line_renderer.add_lines(
            m,
            context.light_color,
            {
                {
                    -length * axis_z
                    + outer_radius * std::cos(t0) * axis_x
                    + outer_radius * std::sin(t0) * axis_y,
                    -length * axis_z
                    + outer_radius * std::cos(t1) * axis_x
                    + outer_radius * std::sin(t1) * axis_y
                }
            }
        );
        line_renderer.add_lines(
            m,
            context.half_light_color,
            {
                {
                    -length * axis_z + inner_radius * std::cos(t0) * axis_x + inner_radius * std::sin(t0) * axis_y,
                    -length * axis_z + inner_radius * std::cos(t1) * axis_x + inner_radius * std::sin(t1) * axis_y
                }
                //{
                //    -length * axis_z * half_position
                //    + outer_radius * std::cos(t0) * axis_x * half_position
                //    + outer_radius * std::sin(t0) * axis_y * half_position,
                //    -length * axis_z * half_position
                //    + outer_radius * std::cos(t1) * axis_x * half_position
                //    + outer_radius * std::sin(t1) * axis_y * half_position
                //}
            }
        );
    }
    line_renderer.add_lines(
        m,
        context.half_light_color,
        {
            //{
            //    O,
            //    -length * axis_z - outer_radius * axis_x,
            //},
            //{
            //    O,
            //    -length * axis_z + outer_radius * axis_x
            //},
            //{
            //    O,
            //    -length * axis_z - outer_radius * axis_y,
            //},
            //{
            //    O,
            //    -length * axis_z + outer_radius * axis_y
            //},
            {
                O,
                -length * axis_z
            },
            {
                -length * axis_z - inner_radius * axis_x,
                -length * axis_z + inner_radius * axis_x
            },
            {
                -length * axis_z - inner_radius * axis_y,
                -length * axis_z + inner_radius * axis_y
            }
        }
    );

    class Cone_edge
    {
    public:
        Cone_edge(
            const vec3& p,
            const vec3& n,
            const vec3& t,
            const vec3& b,
            const float phi,
            const float n_dot_v
        )
        : p      {p}
        , n      {n}
        , t      {t}
        , b      {b}
        , phi    {phi}
        , n_dot_v{n_dot_v}
        {
        }

        vec3  p;
        vec3  n;
        vec3  t;
        vec3  b;
        float phi;
        float n_dot_v;
    };

    std::vector<Cone_edge> cone_edges;
    for (int i = 0; i < edge_count; ++i)
    {
        const float phi     = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(edge_count);
        const float sin_phi = std::sin(phi);
        const float cos_phi = std::cos(phi);

        const vec3 p{
            + outer_radius * sin_phi,
            + outer_radius * cos_phi,
            -length
        };

        const vec3 B = normalize(O - p); // generatrix
        const vec3 T{
            static_cast<float>(std::sin(phi + glm::half_pi<float>())),
            static_cast<float>(std::cos(phi + glm::half_pi<float>())),
            0.0f
        };
        const vec3  N0      = glm::cross(B, T);
        const vec3  N       = glm::normalize(N0);
        const vec3  v       = normalize(p - view_position);
        const float n_dot_v = dot(N, v);

        cone_edges.emplace_back(
            p,
            N,
            T,
            B,
            phi,
            n_dot_v
        );
    }

    std::vector<Cone_edge> sign_flip_edges;
    for (size_t i = 0; i < cone_edges.size(); ++i)
    {
        const std::size_t next_i    = (i + 1) % cone_edges.size();
        const auto&       edge      = cone_edges[i];
        const auto&       next_edge = cone_edges[next_i];
        if (sign(edge.n_dot_v) != sign(next_edge.n_dot_v))
        {
            if (std::abs(edge.n_dot_v) < std::abs(next_edge.n_dot_v))
            {
                sign_flip_edges.push_back(edge);
            }
            else
            {
                sign_flip_edges.push_back(next_edge);
            }
        }
    }

    for (auto& edge : sign_flip_edges)
    {
        line_renderer.add_lines(m, context.light_color, { { O, edge.p } } );
        //line_renderer.add_lines(m, red,   { { edge.p, edge.p + 0.1f * edge.n } }, thickness);
        //line_renderer.add_lines(m, green, { { edge.p, edge.p + 0.1f * edge.t } }, thickness);
        //line_renderer.add_lines(m, blue,  { { edge.p, edge.p + 0.1f * edge.b } }, thickness);
    }
}

void Debug_visualizations::camera_visualization(
    const Render_context&       render_context,
    const erhe::scene::Camera*  camera
)
{
    if (camera == render_context.camera)
    {
        return;
    }

    if (!test_bits(camera->node_data.flag_bits, erhe::scene::Node_flag_bit::show_debug_visualizations))
    {
        return;
    }

    const mat4 clip_from_node  = camera->projection()->get_projection_matrix(1.0f, render_context.viewport.reverse_depth);
    const mat4 node_from_clip  = inverse(clip_from_node);
    const mat4 world_from_clip = camera->world_from_node() * node_from_clip;

    const uint32_t color      = erhe::toolkit::convert_float4_to_uint32(camera->node_data.wireframe_color);
    //const uint32_t half_color = erhe::toolkit::convert_float4_to_uint32(
    //    vec4{0.5f * vec3{camera->node_data.wireframe_color}, 0.5f}
    //);

    auto& line_renderer = *m_line_renderer_set->hidden.at(2).get();
    line_renderer.set_thickness(m_camera_visualization_width);
    line_renderer.add_cube(
        world_from_clip,
        color,
        clip_min_corner,
        clip_max_corner,
        true
    );
}

void Debug_visualizations::tool_render(
    const Render_context& context
)
{
    if (m_line_renderer_set == nullptr)
    {
        return;
    }

    if (m_tool_hide && m_trs_tool && m_trs_tool->is_active())
    {
        return;
    }

    auto& line_renderer = *m_line_renderer_set->hidden.at(2).get();
    std::shared_ptr<erhe::scene::Camera> selected_camera;
    const auto& selection = m_selection_tool->selection();
    for (const auto& node : selection)
    {
        if (is_camera(node))
        {
            selected_camera = as_camera(node);
        }
    }

    if (m_selection)
    {
        m_selection_bounding_volume = erhe::toolkit::Bounding_volume_combiner{}; // reset
        for (const auto& node : selection)
        {
            if (m_selection_node_axis_visible)
            {
                const uint32_t red   = 0xff0000ffu;
                const uint32_t green = 0xff00ff00u;
                const uint32_t blue  = 0xffff0000u;
                const mat4 m{node->world_from_node()};
                line_renderer.set_thickness(m_selection_node_axis_width);
                line_renderer.add_lines( m, red,   {{ O, axis_x }} );
                line_renderer.add_lines( m, green, {{ O, axis_y }} );
                line_renderer.add_lines( m, blue,  {{ O, axis_z }} );
            }

            if (is_mesh(node))
            {
                mesh_selection_visualization(context, as_mesh(node).get());
            }

            if (
                is_camera(node) &&
                (m_viewport_config->debug_visualizations.camera == Visualization_mode::selected)
            )
            {
                camera_visualization(context, as_camera(node).get());
            }
        }

        if (m_selection_bounding_volume.get_element_count() > 1)
        {
            erhe::toolkit::Bounding_box    selection_bounding_box;
            erhe::toolkit::Bounding_sphere selection_bounding_sphere;
            erhe::toolkit::calculate_bounding_volume(m_selection_bounding_volume, selection_bounding_box, selection_bounding_sphere);
            const float    box_volume    = selection_bounding_box.volume();
            const float    sphere_volume = selection_bounding_sphere.volume();
            const uint32_t major_color   = erhe::toolkit::convert_float4_to_uint32(m_group_selection_major_color);
            const uint32_t minor_color   = erhe::toolkit::convert_float4_to_uint32(m_group_selection_minor_color);
            if (
                (box_volume > 0.0f) &&
                (box_volume < sphere_volume)
            )
            {
                line_renderer.set_thickness(m_selection_major_width);
                line_renderer.add_cube(
                    glm::mat4{1.0f},
                    major_color,
                    selection_bounding_box.min - glm::vec3{m_gap, m_gap, m_gap},
                    selection_bounding_box.max + glm::vec3{m_gap, m_gap, m_gap}
                );
            }
            else if (sphere_volume > 0.0f)
            {
                line_renderer.add_sphere(
                    erhe::scene::Transform{},
                    major_color,
                    minor_color,
                    m_selection_major_width,
                    m_selection_minor_width,
                    selection_bounding_sphere.center,
                    selection_bounding_sphere.radius + m_gap,
                    &(context.camera->world_from_node_transform()),
                    m_sphere_step_count
                );
            }
        }
    }

    if (context.scene_view == nullptr)
    {
        return;
    }

    const auto scene_root = context.scene_view->get_scene_root();
    if (!scene_root)
    {
        return;
    }

    if (m_lights)
    {
        for (const auto& light : scene_root->layers().light()->lights)
        {
            light_visualization(context, selected_camera, light.get());
        }
    }

    if (m_cameras)
    //if (m_viewport_config->debug_visualizations.camera == Visualization_mode::all)
    {
        for (const auto& camera : scene_root->scene().cameras)
        {
            camera_visualization(context, camera.get());
        }
    }

    if (m_physics)
    {
        for (const auto& mesh : scene_root->layers().content()->meshes)
        {
            std::shared_ptr<Node_physics> node_physics = get_physics_node(mesh.get());
            if (node_physics)
            {
                const erhe::physics::IRigid_body* rigid_body = node_physics->rigid_body();
                if (rigid_body != nullptr)
                {
                    const erhe::physics::Transform transform = rigid_body->get_world_transform();
                    {
                        const uint32_t half_red  {0x880000ffu};
                        const uint32_t half_green{0x8800ff00u};
                        const uint32_t half_blue {0x88ff0000u};
                        glm::mat4 m{transform.basis};
                        m[3] = glm::vec4{
                            transform.origin.x,
                            transform.origin.y,
                            transform.origin.z,
                            1.0f
                        };
                        line_renderer.add_lines( m, half_red,   {{ O, axis_x }} );
                        line_renderer.add_lines( m, half_green, {{ O, axis_y }} );
                        line_renderer.add_lines( m, half_blue,  {{ O, axis_z }} );
                    }
                    {
                        const uint32_t cyan{0xffffff00u};
                        const glm::vec3 velocity = rigid_body->get_linear_velocity();
                        glm::mat4 m{1.0f};
                        m[3] = glm::vec4{
                            transform.origin.x,
                            transform.origin.y,
                            transform.origin.z,
                            1.0f
                        };
                        line_renderer.add_lines( m, cyan, {{ O, 4.0f * velocity }} );
                    }
                }
            }
        }
    }

    if (m_raytrace)
    {
        const uint32_t red  {0xff0000ffu};
        const uint32_t green{0xff00ff00u};
        const uint32_t blue {0xffff0000u};

        for (const auto& mesh : scene_root->layers().content()->meshes)
        {
            std::shared_ptr<Node_raytrace> node_raytrace = get_raytrace(mesh.get());
            if (node_raytrace)
            {
                const erhe::raytrace::IInstance* instance = node_raytrace->raytrace_instance();
                const auto m = instance->get_transform();
                line_renderer.add_lines( m, red,   {{ O, axis_x }} );
                line_renderer.add_lines( m, green, {{ O, axis_y }} );
                line_renderer.add_lines( m, blue,  {{ O, axis_z }} );
            }
        }
    }
}

void Debug_visualizations::imgui()
{
#if defined(ERHE_GUI_LIBRARY_IMGUI)
    //ImGui::Text("Cam X = %f, Y = %f, Z = %f", m_camera_position.x, m_camera_position.y, m_camera_position.z);
    ImGui::ColorEdit4 ("Selection Major Color", &m_selection_major_color.x, ImGuiColorEditFlags_Float);
    ImGui::ColorEdit4 ("Selection Minor Color", &m_selection_minor_color.x, ImGuiColorEditFlags_Float);
    ImGui::SliderFloat("Selection Major Width", &m_selection_major_width, 0.1f, 100.0f);
    ImGui::SliderFloat("Selection Minor Width", &m_selection_minor_width, 0.1f, 100.0f);
    ImGui::SliderInt  ("Sphere Step Count", &m_sphere_step_count, 1, 200);
    ImGui::SliderFloat("Gap", &m_gap, 0.0001f, 0.1f);
    ImGui::Checkbox("Tool Hide", &m_tool_hide);
    ImGui::Checkbox("Raytrace",  &m_raytrace);
    ImGui::Checkbox("Physics",   &m_physics);
    ImGui::Checkbox("Lights",    &m_lights);
    ImGui::Checkbox("Cameras",   &m_cameras);
    ImGui::Checkbox("Selection", &m_selection);
#endif
}

} // namespace editor

