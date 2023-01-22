#pragma once

#include "tools/tool.hpp"

#include "erhe/components/components.hpp"

#include <string_view>

namespace editor
{

class Rotate_tool
    : public erhe::components::Component
    , public Tool
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

    // Implements Tool
    void handle_priority_update(int old_priority, int new_priority) override;
};

extern Rotate_tool* g_rotate_tool;

} // namespace editor
