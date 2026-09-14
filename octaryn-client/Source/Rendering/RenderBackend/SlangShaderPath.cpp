#include "SlangShaderPath.h"

#include "AssetPath.h"

#include <filesystem>
#include <string_view>
#include <system_error>

namespace octaryn::client::rendering {

std::string resolve_slang_shader_path(const char *source_path) {
  constexpr std::string_view source_root = "octaryn-client/Shaders/";
  if (source_path == nullptr) { return {}; }
  const std::string_view source(source_path);
  if (!source.starts_with(source_root) || !source.ends_with(".slang")) {
    return {};
  }
  const std::string relative =
      "Client/Shaders/" + std::string(source.substr(source_root.size()));
  char bundle_path[4096]{};
  if (!bundle_path_build(bundle_path, sizeof(bundle_path), relative.c_str())) {
    return {};
  }
  char shader_root[4096]{};
  if (!bundle_path_build(shader_root, sizeof(shader_root), "Client/Shaders")) {
    return {};
  }
  std::error_code error;
  const auto bundle_root = std::filesystem::path(
      std::u8string_view(reinterpret_cast<const char8_t *>(shader_root)));
  // A staged bundle owns its shaders; missing bundle files must fail visibly.
  if (std::filesystem::is_directory(bundle_root, error)) {
    return bundle_path;
  }
  // Native development probes execute from the repository root.
  return std::string(source);
}

} // namespace octaryn::client::rendering
