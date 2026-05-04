#pragma once

#include <filesystem>
#include <string>

#include <shaderc/shaderc.hpp>

namespace octaryn::tools::shader_compiler {

auto read_text_file(const std::filesystem::path& path) -> std::string;

class filesystem_includer final : public shaderc::CompileOptions::IncluderInterface {
 public:
  explicit filesystem_includer(std::filesystem::path root);

  shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type,
                                     const char* requesting_source, size_t include_depth) override;
  void ReleaseInclude(shaderc_include_result* data) override;

 private:
  struct include_result_data;

  std::filesystem::path resolve_include_path(const char* requested_source, const char* requesting_source) const;

  std::filesystem::path root_{};
};

}  // namespace octaryn::tools::shader_compiler
