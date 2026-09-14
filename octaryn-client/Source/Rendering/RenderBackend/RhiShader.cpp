#include "RhiShader.h"
#include "SlangShaderPath.h"

#include <cstdio>
#include <filesystem>
#include <vector>

namespace octaryn::client::rendering {
namespace {
void print_diagnostics(ISlangBlob* diagnostics) {
  if (diagnostics && diagnostics->getBufferSize())
    std::fwrite(diagnostics->getBufferPointer(), 1,
                diagnostics->getBufferSize(), stderr);
}
}

bool create_rhi_program(rhi::IDevice* device, const char* path,
                        const char* const* entries, uint32_t count,
                        Slang::ComPtr<rhi::IShaderProgram>& program) {
  if (!device || !path || !entries || !count) return false;
  const auto shader_path = std::filesystem::path(path).is_absolute()
      ? std::string(path) : resolve_slang_shader_path(path);
  if (shader_path.empty()) return false;
  Slang::ComPtr<slang::ISession> session;
  if (SLANG_FAILED(device->getSlangSession(session.writeRef()))) return false;
  Slang::ComPtr<ISlangBlob> diagnostics;
  auto* module = session->loadModule(shader_path.c_str(), diagnostics.writeRef());
  print_diagnostics(diagnostics);
  if (!module) {
    std::fprintf(stderr, "Slang module failed: %s\n", shader_path.c_str());
    return false;
  }
  std::vector<Slang::ComPtr<slang::IEntryPoint>> owned_entries(count);
  std::vector<slang::IComponentType*> components{module};
  for (uint32_t index = 0; index < count; ++index) {
    if (SLANG_FAILED(module->findEntryPointByName(
            entries[index], owned_entries[index].writeRef()))) {
      std::fprintf(stderr, "Slang entry missing: %s (%s)\n", entries[index], path);
      return false;
    }
    components.push_back(owned_entries[index]);
  }
  Slang::ComPtr<slang::IComponentType> composed, linked;
  auto result = session->createCompositeComponentType(
      components.data(), static_cast<SlangInt>(components.size()),
      composed.writeRef(), diagnostics.writeRef());
  print_diagnostics(diagnostics);
  if (SLANG_FAILED(result)) return false;
  result = composed->link(linked.writeRef(), diagnostics.writeRef());
  print_diagnostics(diagnostics);
  if (SLANG_FAILED(result)) return false;
  rhi::ShaderProgramDesc description{};
  description.slangGlobalScope = linked;
  result = device->createShaderProgram(description, program.writeRef(), diagnostics.writeRef());
  print_diagnostics(diagnostics);
  return SLANG_SUCCEEDED(result) && program != nullptr;
}

bool create_rhi_compute_pipeline(rhi::IDevice* device, const char* path,
                                 const char* entry,
                                 Slang::ComPtr<rhi::IComputePipeline>& pipeline) {
  Slang::ComPtr<rhi::IShaderProgram> program;
  if (!create_rhi_program(device, path, &entry, 1, program)) return false;
  rhi::ComputePipelineDesc description{};
  description.program = program;
  return SLANG_SUCCEEDED(device->createComputePipeline(description, pipeline.writeRef()))
      && pipeline != nullptr;
}

} // namespace octaryn::client::rendering
