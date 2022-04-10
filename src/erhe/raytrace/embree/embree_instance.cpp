#include "erhe/raytrace/embree/embree_instance.hpp"
#include "erhe/raytrace/embree/embree_device.hpp"
#include "erhe/raytrace/embree/embree_scene.hpp"
#include "erhe/raytrace/log.hpp"
#include "erhe/log/log_glm.hpp"

namespace erhe::raytrace
{

auto IInstance::create(const std::string_view debug_label) -> IInstance*
{
    return new Embree_instance(debug_label);
}

auto IInstance::create_shared(const std::string_view debug_label) -> std::shared_ptr<IInstance>
{
    return std::make_shared<Embree_instance>(debug_label);
}

auto IInstance::create_unique(const std::string_view debug_label) -> std::unique_ptr<IInstance>
{
    return std::make_unique<Embree_instance>(debug_label);
}

Embree_instance::Embree_instance(const std::string_view debug_label)
    : m_debug_label{debug_label}
{
    m_geometry = rtcNewGeometry(
        Embree_device::get_instance().get_rtc_device(),
        RTC_GEOMETRY_TYPE_INSTANCE
    );
    //log_embree.trace("rtcNewGeometry(instance = {})\n", debug_label);
    if (m_geometry != nullptr)
    {
        rtcSetGeometryUserData(m_geometry, this);
    }
}

Embree_instance::~Embree_instance()
{
    //log_embree.trace("rtcReleaseGeometry(instance = {})\n", m_debug_label);
    rtcReleaseGeometry(m_geometry);
}

auto Embree_instance::get_rtc_geometry() -> RTCGeometry
{
    return m_geometry;
}

void Embree_instance::enable()
{
    //log_embree.trace("rtcEnableGeometry(instance = {})\n", m_debug_label);
    rtcEnableGeometry(m_geometry);
    m_enabled = true;
}

void Embree_instance::disable()
{
    //log_embree.trace("rtcDisableGeometry(instance = {})\n", m_debug_label);
    rtcDisableGeometry(m_geometry);
    m_enabled = false;
}

[[nodiscard]] auto Embree_instance::is_enabled() const -> bool
{
    return m_enabled;
}

void Embree_instance::set_mask(const uint32_t mask)
{
    //log_embree.trace("rtcSetGeometryMask(instance = {}, mask = {:#04x})\n", m_debug_label, mask);
    rtcSetGeometryMask(m_geometry, mask);
    m_mask = mask;
}

auto Embree_instance::get_mask() const -> uint32_t
{
    return m_mask;
}

void Embree_instance::set_transform(const glm::mat4 transform)
{
    const unsigned int time_step{0};
    //log_embree.trace(
    //    "rtcSetGeometryTransform(\n"
    //    "    instance = {},\n"
    //    "    transform =\n"
    //    "        {}\n"
    //    "        {}\n"
    //    "        {}\n"
    //    "        {}\n"
    //    ")\n",
    //    m_debug_label,
    //    transform[0],
    //    transform[1],
    //    transform[2],
    //    transform[3]
    //);
    rtcSetGeometryTransform(
        m_geometry,
        time_step,
        RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR,
        &transform[0][0]
    );
}

void Embree_instance::set_scene(IScene* scene)
{
    m_scene = static_cast<Embree_scene*>(scene);
    //log_embree.trace(
    //    "rtcSetGeometryInstancedScene(instance = {}, scene = {})\n",
    //    m_debug_label,
    //    (m_scene != nullptr)
    //        ? m_scene->debug_label()
    //        : "()"
    //);
    m_scene->commit();
    rtcSetGeometryInstancedScene(
        m_geometry,
        (m_scene != nullptr)
            ? m_scene->get_rtc_scene()
            : nullptr
    );
}

auto Embree_instance::get_embree_scene() const -> Embree_scene*
{
    return m_scene;
}

void Embree_instance::commit()
{
    //log_embree.trace("rtcCommitGeometry(instance = {})\n", m_debug_label);
    rtcCommitGeometry(m_geometry);
}

void Embree_instance::set_user_data(void* ptr)
{
    m_user_data = ptr;
}

auto Embree_instance::get_transform() const -> glm::mat4
{
    glm::mat4 transform{1.0f};
    rtcGetGeometryTransform(
        m_geometry,
        0.0f,
        RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR,
        &transform[0][0]
    );
    return transform;
}

auto Embree_instance::get_scene() const -> IScene*
{
    return m_scene;
}

auto Embree_instance::get_user_data() const -> void*
{
    return m_user_data;
}

auto Embree_instance::debug_label() const -> std::string_view
{
    return m_debug_label;
}

} // namespace erhe::raytrace
