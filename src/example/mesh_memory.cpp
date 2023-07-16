#include "mesh_memory.hpp"

#include "erhe/configuration/configuration.hpp"
#include "erhe/scene_renderer/program_interface.hpp"

namespace example {

static constexpr gl::Buffer_storage_mask storage_mask{gl::Buffer_storage_mask::map_write_bit};

auto Mesh_memory::get_vertex_buffer_size() const -> std::size_t
{
    int vertex_buffer_size{32}; // in megabytes
    auto ini = erhe::configuration::get_ini("erhe.ini", "mesh_memory");
    ini->get("vertex_buffer_size", vertex_buffer_size);
    return vertex_buffer_size * 1024 * 1024;
}

auto Mesh_memory::get_index_buffer_size() const -> std::size_t
{
    int index_buffer_size{8}; // in megabytes
    auto ini = erhe::configuration::get_ini("erhe.ini", "mesh_memory");
    ini->get("index_buffer_size",  index_buffer_size);
    return index_buffer_size * 1024 * 1024;
}

Mesh_memory::Mesh_memory(
    erhe::graphics::Instance&                graphics_instance,
    erhe::scene_renderer::Program_interface& program_interface
)
    : graphics_instance{graphics_instance}
    , vertex_format{
        erhe::graphics::Vertex_attribute::position_float3 (),
        erhe::graphics::Vertex_attribute::normal_float3   (),
        erhe::graphics::Vertex_attribute::texcoord0_float2(),
        erhe::graphics::Vertex_attribute::color_ubyte4    ()
    }
    , gl_vertex_buffer{
        graphics_instance,
        gl::Buffer_target::array_buffer,
        get_vertex_buffer_size(),
        storage_mask
    }
    , gl_index_buffer{
        graphics_instance,
        gl::Buffer_target::element_array_buffer,
        get_index_buffer_size(),
        storage_mask
    }
    , gl_buffer_sink{
        gl_buffer_transfer_queue,
        gl_vertex_buffer,
        gl_index_buffer
    }
    , buffer_info{
        .index_type    = gl::Draw_elements_type::unsigned_int,
        .vertex_format = vertex_format,
        .buffer_sink   = gl_buffer_sink
    }
    , vertex_input{
        erhe::graphics::Vertex_input_state_data::make(
            program_interface.attribute_mappings,
            vertex_format,
            &gl_vertex_buffer,
            &gl_index_buffer
        )
    }
{
    gl_vertex_buffer.set_debug_label("Mesh Memory Vertex");
    gl_index_buffer .set_debug_label("Mesh Memory Index");
}

}
