#include "ShaderMetadataContract.hpp"

namespace octaryn::client::rendering {

auto validate_graphics_shader_metadata(const GraphicsShaderMetadata& metadata) -> bool
{
    return metadata.uniformBuffers <= shader_metadata_uniform_buffer_limit;
}

auto validate_compute_shader_metadata(const ComputeShaderMetadata& metadata) -> bool
{
    return metadata.uniformBuffers <= shader_metadata_uniform_buffer_limit;
}

} // namespace octaryn::client::rendering
