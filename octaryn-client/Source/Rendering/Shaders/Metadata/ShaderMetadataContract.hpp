#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace octaryn::client::rendering {

inline constexpr std::uint32_t shader_metadata_uniform_buffer_limit = 4u;

struct ShaderIoMetadata
{
    std::string name{};
    std::string type{};
    std::uint32_t location{};
};

struct GraphicsShaderMetadata
{
    std::uint32_t samplers{};
    std::uint32_t storageTextures{};
    std::uint32_t storageBuffers{};
    std::uint32_t uniformBuffers{};
    std::vector<ShaderIoMetadata> inputs{};
    std::vector<ShaderIoMetadata> outputs{};
};

struct ComputeShaderMetadata
{
    std::uint32_t samplers{};
    std::uint32_t readonlyStorageTextures{};
    std::uint32_t readonlyStorageBuffers{};
    std::uint32_t readwriteStorageTextures{};
    std::uint32_t readwriteStorageBuffers{};
    std::uint32_t uniformBuffers{};
    std::uint32_t threadcountX{};
    std::uint32_t threadcountY{};
    std::uint32_t threadcountZ{};
};

auto validate_graphics_shader_metadata(const GraphicsShaderMetadata& metadata) -> bool;
auto validate_compute_shader_metadata(const ComputeShaderMetadata& metadata) -> bool;

} // namespace octaryn::client::rendering
