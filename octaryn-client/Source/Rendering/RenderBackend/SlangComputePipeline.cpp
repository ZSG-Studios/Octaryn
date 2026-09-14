#include "SlangComputePipeline.h"

#include "SlangShaderPath.h"

#if defined(OCTARYN_CLIENT_SLANG_RHI_AVAILABLE)
#include <cstdio>

namespace octaryn::client::rendering {

bool create_slang_compute_pipeline(gfx::IDevice *device, const char *source_path,
                                  Slang::ComPtr<gfx::IPipelineState> &pipeline) {
  const auto shader_path = resolve_slang_shader_path(source_path);
  if (shader_path.empty()) { return false; }
  const char *entry_point = "main";
  gfx::IShaderProgram::CreateDesc2 desc{};
  desc.sourceType = gfx::ShaderModuleSourceType::SlangSourceFile;
  desc.sourceData = const_cast<char *>(shader_path.c_str());
  desc.sourceDataSize = shader_path.size();
  desc.entryPointCount = 1;
  desc.entryPointNames = &entry_point;
  Slang::ComPtr<gfx::IShaderProgram> program;
  Slang::ComPtr<ISlangBlob> diagnostics;
  if (SLANG_FAILED(device->createProgram2(desc, program.writeRef(),
                                         diagnostics.writeRef())) ||
      program == nullptr) {
    std::fprintf(stderr, "Slang compute shader failed: %s\n", shader_path.c_str());
    if (diagnostics != nullptr) {
      std::fwrite(diagnostics->getBufferPointer(), 1,
                  diagnostics->getBufferSize(), stderr);
    }
    return false;
  }
  gfx::ComputePipelineStateDesc pipeline_desc{};
  pipeline_desc.program = program;
  return SLANG_SUCCEEDED(device->createComputePipelineState(
             pipeline_desc, pipeline.writeRef())) && pipeline != nullptr;
}

} // namespace octaryn::client::rendering
#endif
