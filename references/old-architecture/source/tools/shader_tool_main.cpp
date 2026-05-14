#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <shaderc/shaderc.hpp>
#include <spirv-tools/libspirv.hpp>
#include <spirv_reflect.hpp>
#include <taskflow/taskflow.hpp>

#include "core/crash_diagnostics.h"
#include "core/log.h"
#include "core/memory_mimalloc.h"
#include "render/shader/asset_metadata.hpp"

namespace {

struct sdl_free_deleter {
  void operator()(void* pointer) const
  {
    SDL_free(pointer);
  }
};

using sdl_memory_ptr = std::unique_ptr<void, sdl_free_deleter>;

struct compiled_shader {
  sdl_memory_ptr data{};
  size_t size = 0;
};

auto read_text_file(const std::filesystem::path& path) -> std::string;

class filesystem_includer final : public shaderc::CompileOptions::IncluderInterface {
 public:
  explicit filesystem_includer(std::filesystem::path root)
      : root_(std::move(root)) {}

  shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type,
                                     const char* requesting_source, size_t include_depth) override
  {
    (void) type;
    (void) include_depth;

    auto* result = new include_result_data{};
    result->resolved_path = resolve_include_path(requested_source, requesting_source);
    if (result->resolved_path.empty()) {
      result->source_name = requested_source;
      result->content = "Failed to resolve include: " + std::string(requested_source);
    }
    else {
      result->source_name = result->resolved_path.string();
      result->content = read_text_file(result->resolved_path);
      if (result->content.empty()) {
        result->content = "Failed to read include: " + result->resolved_path.string();
      }
    }

    result->result.source_name = result->source_name.c_str();
    result->result.source_name_length = result->source_name.size();
    result->result.content = result->content.c_str();
    result->result.content_length = result->content.size();
    result->result.user_data = result;
    return &result->result;
  }

  void ReleaseInclude(shaderc_include_result* data) override
  {
    delete static_cast<include_result_data*>(data->user_data);
  }

 private:
  struct include_result_data {
    std::filesystem::path resolved_path{};
    std::string source_name{};
    std::string content{};
    shaderc_include_result result{};
  };

  std::filesystem::path resolve_include_path(const char* requested_source, const char* requesting_source) const
  {
    const std::filesystem::path requested = requested_source;
    if (requested.is_absolute() && std::filesystem::exists(requested)) {
      return requested;
    }

    const std::filesystem::path requesting = requesting_source ? std::filesystem::path{requesting_source} : std::filesystem::path{};
    if (!requesting.empty()) {
      const std::filesystem::path candidate = requesting.parent_path() / requested;
      if (std::filesystem::exists(candidate)) {
        return candidate;
      }
    }

    const std::filesystem::path rooted = root_ / requested;
    if (std::filesystem::exists(rooted)) {
      return rooted;
    }

    return {};
  }

  std::filesystem::path root_{};
};

constexpr glz::opts k_json_write_opts{.prettify = true};

enum class shader_stage {
  vertex,
  fragment,
  compute,
};

struct shader_source_info {
  shader_stage stage{};
  std::filesystem::path output_name{};
};

auto read_text_file(const std::filesystem::path& path) -> std::string
{
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return {};
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

auto shader_source_info_from_path(const std::filesystem::path& path) -> shader_source_info
{
  const std::string filename = path.filename().string();
  if (filename.ends_with(".vert.glsl")) {
    return {.stage = shader_stage::vertex, .output_name = path.stem()};
  }
  if (filename.ends_with(".frag.glsl")) {
    return {.stage = shader_stage::fragment, .output_name = path.stem()};
  }
  if (filename.ends_with(".comp.glsl")) {
    return {.stage = shader_stage::compute, .output_name = path.stem()};
  }

  oct_log_errorf("Unsupported shader stage for path: %s (expected explicit .vert/.frag/.comp plus .glsl language suffix)",
                 path.string().c_str());
  std::exit(2);
}

auto to_shadercross_stage(shader_stage stage) -> SDL_ShaderCross_ShaderStage
{
  switch (stage) {
    case shader_stage::vertex:
      return SDL_SHADERCROSS_SHADERSTAGE_VERTEX;
    case shader_stage::fragment:
      return SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
    case shader_stage::compute:
      return SDL_SHADERCROSS_SHADERSTAGE_COMPUTE;
  }
  oct_log_errorf("Unknown shader stage");
  std::exit(2);
}

auto to_shaderc_kind(shader_stage stage) -> shaderc_shader_kind
{
  switch (stage) {
    case shader_stage::vertex:
      return shaderc_vertex_shader;
    case shader_stage::fragment:
      return shaderc_fragment_shader;
    case shader_stage::compute:
      return shaderc_compute_shader;
  }
  oct_log_errorf("Unknown shader stage");
  std::exit(2);
}

auto io_type_prefix(SDL_ShaderCross_IOVarType type) -> std::string
{
  switch (type) {
    case SDL_SHADERCROSS_IOVAR_TYPE_INT8:
    case SDL_SHADERCROSS_IOVAR_TYPE_INT16:
    case SDL_SHADERCROSS_IOVAR_TYPE_INT32:
    case SDL_SHADERCROSS_IOVAR_TYPE_INT64:
      return "int";
    case SDL_SHADERCROSS_IOVAR_TYPE_UINT8:
    case SDL_SHADERCROSS_IOVAR_TYPE_UINT16:
    case SDL_SHADERCROSS_IOVAR_TYPE_UINT32:
    case SDL_SHADERCROSS_IOVAR_TYPE_UINT64:
      return "uint";
    case SDL_SHADERCROSS_IOVAR_TYPE_FLOAT16:
      return "half";
    case SDL_SHADERCROSS_IOVAR_TYPE_FLOAT32:
      return "float";
    case SDL_SHADERCROSS_IOVAR_TYPE_FLOAT64:
      return "double";
    case SDL_SHADERCROSS_IOVAR_TYPE_UNKNOWN:
      break;
  }
  return "unknown";
}

auto io_type_name(const SDL_ShaderCross_IOVarMetadata& metadata) -> std::string
{
  std::string name = io_type_prefix(metadata.vector_type);
  if (metadata.vector_size > 1) {
    name += std::to_string(metadata.vector_size);
  }
  return name;
}

auto convert_io(const SDL_ShaderCross_IOVarMetadata& metadata) -> shader_io_desc
{
  shader_io_desc output{};
  output.name = metadata.name ? metadata.name : "";
  output.type = io_type_name(metadata);
  output.location = metadata.location;
  return output;
}

template <typename T>
bool write_json_file(const std::filesystem::path& path, const T& value)
{
  std::string output{};
  const auto error = glz::write<k_json_write_opts>(value, output);
  if (error) {
    oct_log_errorf("Failed to encode json file: %s", path.string().c_str());
    return false;
  }

  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    oct_log_errorf("Failed to open output json file: %s", path.string().c_str());
    return false;
  }

  file.write(output.data(), static_cast<std::streamsize>(output.size()));
  return file.good();
}

bool write_binary_file(const std::filesystem::path& path, std::span<const std::byte> bytes)
{
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    SDL_Log("Failed to open output binary file: %s", path.string().c_str());
    return false;
  }

  file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return file.good();
}

bool write_text_file(const std::filesystem::path& path, std::string_view text)
{
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    oct_log_errorf("Failed to open output text file: %s", path.string().c_str());
    return false;
  }

  file.write(text.data(), static_cast<std::streamsize>(text.size()));
  return file.good();
}

auto compile_spirv_from_source(const std::filesystem::path& path, const shader_source_info& source_info) -> compiled_shader
{
  compiled_shader output{};
  const std::string source = read_text_file(path);
  if (source.empty()) {
    oct_log_errorf("Failed to read shader source: %s", path.string().c_str());
    return output;
  }

  shaderc::Compiler compiler{};
  shaderc::CompileOptions options{};
  options.SetSourceLanguage(shaderc_source_language_glsl);
  options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
  options.SetIncluder(std::make_unique<filesystem_includer>(path.parent_path()));

  const std::string input_name = path.string();
  const auto result = compiler.CompileGlslToSpv(source, to_shaderc_kind(source_info.stage), input_name.c_str(), "main", options);
  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    oct_log_errorf("Failed to compile shader to SPIR-V: %s (%s)", path.string().c_str(), result.GetErrorMessage().c_str());
    output = {};
    return output;
  }

  const std::vector<std::uint32_t> spirv_words(result.cbegin(), result.cend());
  output.size = spirv_words.size() * sizeof(std::uint32_t);
  void* bytes = SDL_malloc(output.size);
  if (!bytes) {
    oct_log_errorf("Failed to allocate SPIR-V buffer for: %s", path.string().c_str());
    return {};
  }
  SDL_memcpy(bytes, spirv_words.data(), output.size);
  output.data.reset(bytes);
  return output;
}

bool validate_spirv(const std::filesystem::path& path, std::span<const std::byte> spirv_bytes)
{
  if (spirv_bytes.size() % sizeof(std::uint32_t) != 0) {
    oct_log_errorf("SPIR-V bytecode size is not word-aligned: %s", path.string().c_str());
    return false;
  }

  spvtools::SpirvTools tools{SPV_ENV_VULKAN_1_0};
  if (!tools.IsValid()) {
    oct_log_errorf("Failed to initialize SPIR-V validator for %s", path.string().c_str());
    return false;
  }

  std::string diagnostic{};
  tools.SetMessageConsumer(
    [&diagnostic](spv_message_level_t, const char*, const spv_position_t&, const char* message) {
      if (!diagnostic.empty()) {
        diagnostic += '\n';
      }
      diagnostic += message ? message : "unknown error";
    });

  spvtools::ValidatorOptions options{};
  options.SetFriendlyNames(true);

  const auto* spirv_words = reinterpret_cast<const std::uint32_t*>(spirv_bytes.data());
  const size_t spirv_word_count = spirv_bytes.size() / sizeof(std::uint32_t);
  const bool valid = tools.Validate(spirv_words, spirv_word_count, options);
  if (!valid) {
    oct_log_errorf("SPIR-V validation failed for %s: %s", path.string().c_str(),
                   diagnostic.empty() ? "unknown error" : diagnostic.c_str());
  }
  return valid;
}

bool write_spirvcross_debug_json(const std::filesystem::path& path, std::span<const std::byte> spirv_bytes)
{
  try {
    spirv_cross::CompilerReflection compiler(
      reinterpret_cast<const std::uint32_t*>(spirv_bytes.data()),
      spirv_bytes.size() / sizeof(std::uint32_t));
    compiler.set_format("json");
    return write_text_file(path, compiler.compile());
  }
  catch (const std::exception& exception) {
    oct_log_errorf("Failed to write SPIRV-Cross reflection for %s: %s", path.string().c_str(), exception.what());
    return false;
  }
}

bool write_msl_file(const std::filesystem::path& path, std::span<const std::byte> spirv_bytes, shader_stage stage)
{
  SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetStringProperty(props, SDL_SHADERCROSS_PROP_SPIRV_MSL_VERSION_STRING, "2.3.0");

  SDL_ShaderCross_SPIRV_Info info{};
  info.bytecode = reinterpret_cast<const Uint8*>(spirv_bytes.data());
  info.bytecode_size = spirv_bytes.size();
  info.entrypoint = "main";
  info.shader_stage = to_shadercross_stage(stage);
  info.props = props;

  sdl_memory_ptr msl{SDL_ShaderCross_TranspileMSLFromSPIRV(&info)};
  SDL_DestroyProperties(props);
  if (!msl) {
    SDL_Log("Failed to transpile MSL from SPIR-V: %s (%s)", path.string().c_str(), SDL_GetError());
    return false;
  }

  return write_text_file(path, static_cast<const char*>(msl.get()));
}

bool write_graphics_metadata(const std::filesystem::path& path, std::span<const std::byte> spirv_bytes)
{
  sdl_memory_ptr raw_metadata{SDL_ShaderCross_ReflectGraphicsSPIRV(
    reinterpret_cast<const Uint8*>(spirv_bytes.data()), spirv_bytes.size(), 0)};
  if (!raw_metadata) {
    oct_log_errorf("Failed to reflect graphics shader metadata: %s (%s)", path.string().c_str(), SDL_GetError());
    return false;
  }

  const auto* metadata = static_cast<const SDL_ShaderCross_GraphicsShaderMetadata*>(raw_metadata.get());
  if (metadata->resource_info.num_uniform_buffers > 4) {
    oct_log_errorf("Graphics shader exceeds SDL GPU uniform buffer limit (4): %s", path.string().c_str());
    return false;
  }

  graphics_shader_metadata output{};
  output.samplers = metadata->resource_info.num_samplers;
  output.storage_textures = metadata->resource_info.num_storage_textures;
  output.storage_buffers = metadata->resource_info.num_storage_buffers;
  output.uniform_buffers = metadata->resource_info.num_uniform_buffers;
  output.inputs.reserve(metadata->num_inputs);
  output.outputs.reserve(metadata->num_outputs);

  for (Uint32 i = 0; i < metadata->num_inputs; i++) {
    output.inputs.push_back(convert_io(metadata->inputs[i]));
  }
  for (Uint32 i = 0; i < metadata->num_outputs; i++) {
    output.outputs.push_back(convert_io(metadata->outputs[i]));
  }

  return write_json_file(path, output);
}

bool write_compute_metadata(const std::filesystem::path& path, std::span<const std::byte> spirv_bytes)
{
  sdl_memory_ptr raw_metadata{SDL_ShaderCross_ReflectComputeSPIRV(
    reinterpret_cast<const Uint8*>(spirv_bytes.data()), spirv_bytes.size(), 0)};
  if (!raw_metadata) {
    oct_log_errorf("Failed to reflect compute shader metadata: %s (%s)", path.string().c_str(), SDL_GetError());
    return false;
  }

  const auto* metadata = static_cast<const SDL_ShaderCross_ComputePipelineMetadata*>(raw_metadata.get());
  if (metadata->num_uniform_buffers > 4) {
    oct_log_errorf("Compute shader exceeds SDL GPU uniform buffer limit (4): %s", path.string().c_str());
    return false;
  }

  compute_shader_metadata output{};
  output.samplers = metadata->num_samplers;
  output.readonly_storage_textures = metadata->num_readonly_storage_textures;
  output.readonly_storage_buffers = metadata->num_readonly_storage_buffers;
  output.readwrite_storage_textures = metadata->num_readwrite_storage_textures;
  output.readwrite_storage_buffers = metadata->num_readwrite_storage_buffers;
  if (output.readonly_storage_textures > 0 && output.readwrite_storage_textures == 0) {
    output.readwrite_storage_textures = output.readonly_storage_textures;
    output.readonly_storage_textures = 0;
  }
  output.uniform_buffers = metadata->num_uniform_buffers;
  output.threadcount_x = metadata->threadcount_x;
  output.threadcount_y = metadata->threadcount_y;
  output.threadcount_z = metadata->threadcount_z;

  return write_json_file(path, output);
}

bool compile_shader_asset(const std::filesystem::path& input_path, const std::filesystem::path& output_dir)
{
  const shader_source_info source_info = shader_source_info_from_path(input_path);
  const compiled_shader spirv = compile_spirv_from_source(input_path, source_info);
  if (!spirv.data || spirv.size == 0) {
    return false;
  }

  const std::span<const std::byte> spirv_bytes{reinterpret_cast<const std::byte*>(spirv.data.get()), spirv.size};

  std::error_code error;
  std::filesystem::create_directories(output_dir, error);
  if (error) {
    oct_log_errorf("Failed to create shader output directory: %s", output_dir.string().c_str());
    return false;
  }

  if (!validate_spirv(input_path, spirv_bytes)) {
    return false;
  }

  const std::filesystem::path base_path = output_dir / source_info.output_name;
  if (!write_binary_file(base_path.string() + ".spv", spirv_bytes)) {
    return false;
  }
  if (!write_spirvcross_debug_json(base_path.string() + ".spvcross.json", spirv_bytes)) {
    return false;
  }
  if (!write_msl_file(base_path.string() + ".msl", spirv_bytes, source_info.stage)) {
    return false;
  }
  if (source_info.stage == shader_stage::compute) {
    if (!write_compute_metadata(base_path.string() + ".json", spirv_bytes)) {
      return false;
    }
  }
  else {
    if (!write_graphics_metadata(base_path.string() + ".json", spirv_bytes)) {
      return false;
    }
  }

  oct_log_infof("Compiled shader asset: %s", input_path.string().c_str());
  return true;
}

}  // namespace

auto main(int argc, char** argv) -> int
{
  oct_log_init("shader-tool");
  oct_memory_init();
  oct_crash_diagnostics_init("shader-tool");
  std::vector<std::filesystem::path> inputs{};
  std::filesystem::path output_dir{};

  for (int i = 1; i < argc; i++) {
    const std::string_view arg = argv[i];
    if (arg == "--input") {
      if (i + 1 >= argc) {
        oct_log_errorf("Missing value for --input");
        return 2;
      }
      inputs.emplace_back(argv[++i]);
    }
    else if (arg == "--output-dir") {
      if (i + 1 >= argc) {
        oct_log_errorf("Missing value for --output-dir");
        return 2;
      }
      output_dir = argv[++i];
    }
    else {
      oct_log_errorf("Unknown argument: %s", argv[i]);
      return 2;
    }
  }

  if (inputs.empty() || output_dir.empty()) {
    oct_log_errorf("usage: octaryn-engine-shader-tool --output-dir <dir> --input <file> [--input <file> ...]");
    return 2;
  }

  if (!SDL_Init(0)) {
    oct_log_errorf("Failed to initialize SDL: %s", SDL_GetError());
    return 1;
  }
  if (!SDL_ShaderCross_Init()) {
    oct_log_errorf("Failed to initialize SDL_shadercross: %s", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  int exit_code = 0;
  for (const auto& input : inputs) {
    if (!compile_shader_asset(input, output_dir)) {
      exit_code = 1;
      break;
    }
  }

  SDL_ShaderCross_Quit();
  SDL_Quit();
  return exit_code;
}
