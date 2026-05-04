#include "ShaderCompilerFileInput.hpp"

#include <fstream>
#include <iterator>
#include <utility>

namespace octaryn::tools::shader_compiler {

struct filesystem_includer::include_result_data {
  std::filesystem::path resolved_path{};
  std::string source_name{};
  std::string content{};
  shaderc_include_result result{};
};

auto read_text_file(const std::filesystem::path& path) -> std::string
{
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return {};
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

filesystem_includer::filesystem_includer(std::filesystem::path root)
    : root_(std::move(root))
{
}

shaderc_include_result* filesystem_includer::GetInclude(
  const char* requested_source,
  shaderc_include_type type,
  const char* requesting_source,
  size_t include_depth)
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

void filesystem_includer::ReleaseInclude(shaderc_include_result* data)
{
  delete static_cast<include_result_data*>(data->user_data);
}

auto filesystem_includer::resolve_include_path(
  const char* requested_source,
  const char* requesting_source) const -> std::filesystem::path
{
  const std::filesystem::path requested = requested_source;
  if (requested.is_absolute() && std::filesystem::exists(requested)) {
    return requested;
  }

  const std::filesystem::path requesting =
    requesting_source ? std::filesystem::path{requesting_source} : std::filesystem::path{};
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

}  // namespace octaryn::tools::shader_compiler
