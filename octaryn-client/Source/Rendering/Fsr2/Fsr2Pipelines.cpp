#include "Fsr2Internal.h"
#include "SlangShaderPath.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>

namespace octaryn::client::rendering {
namespace {
void require(SlangResult result, ISlangBlob* diagnostic = nullptr) {
    if (SLANG_FAILED(result)) throw std::runtime_error(diagnostic
        ? std::string(static_cast<const char*>(diagnostic->getBufferPointer()), diagnostic->getBufferSize())
        : "FSR2 Slang/RHI pipeline operation failed");
}
void name_binding(FfxResourceBinding& binding, const char* name, uint32_t slot) {
    const size_t length = std::strlen(name);
    if (length >= 64) throw std::runtime_error("FSR2 shader binding name too long");
    for (size_t i = 0; i < length; ++i) binding.name[i] = static_cast<wchar_t>(name[i]);
    binding.name[length] = 0;
    binding.slotIndex = slot;
}
std::string shader_source(const std::filesystem::path& path, uint32_t flags, bool sharpen) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Missing FSR2 shader wrapper: " + path.string());
    std::string source((std::istreambuf_iterator<char>(file)), {});
    auto vendor = path.parent_path() / "Vendor";
    if (!std::filesystem::exists(vendor)) {
        if (path.generic_string().find("/octaryn-client/Shaders/") == std::string::npos)
            throw std::runtime_error("Packaged FSR2 vendor shaders are missing");
        vendor = OCTARYN_FSR2_SHADER_VENDOR;
    }
    const std::string needle = "#include \"ffx_";
    auto begin = source.find(needle);
    if (begin == std::string::npos) throw std::runtime_error("FSR2 wrapper has no vendor pass include");
    begin += std::strlen("#include \"");
    source.insert(begin, vendor.generic_string() + "/");
    return "#define FFX_FSR2_OPTION_HDR_COLOR_INPUT " + std::to_string(bool(flags & FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE)) +
        "\n#define FFX_FSR2_OPTION_INVERTED_DEPTH " + std::to_string(bool(flags & FFX_FSR2_ENABLE_DEPTH_INVERTED)) +
        "\n#define FFX_FSR2_OPTION_APPLY_SHARPENING " + std::to_string(sharpen) + "\n" + source;
}
}
FfxErrorCode fsr_create_pipeline(FfxFsr2Interface* api, FfxFsr2Pass pass,
    const FfxPipelineDescription* description, FfxPipelineState* out) {
    auto& context = fsr_backend(api);
    try {
        static constexpr const char* files[] = {
            "octaryn-client/Shaders/Fsr2/DepthClip.slang",
            "octaryn-client/Shaders/Fsr2/Reconstruct.slang",
            "octaryn-client/Shaders/Fsr2/Lock.slang",
            "octaryn-client/Shaders/Fsr2/Accumulate.slang",
            "octaryn-client/Shaders/Fsr2/Accumulate.slang",
            "octaryn-client/Shaders/Fsr2/Rcas.slang",
            "octaryn-client/Shaders/Fsr2/Luminance.slang",
            "octaryn-client/Shaders/Fsr2/Reactive.slang",
            "octaryn-client/Shaders/Fsr2/Tcr.slang"};
        if (pass < 0 || pass >= FFX_FSR2_PASS_COUNT) throw std::runtime_error("Unknown FSR2 pass");
        const auto path = std::filesystem::absolute(resolve_slang_shader_path(files[pass]));
        auto source = shader_source(path, description->contextFlags, pass == FFX_FSR2_PASS_ACCUMULATE_SHARPEN);
        Slang::ComPtr<slang::ISession> session;
        require(context.device->getSlangSession(session.writeRef()));
        Slang::ComPtr<ISlangBlob> diagnostic;
        const std::string module_name = "OctarynFsr2_" + std::to_string(description->contextFlags) + "_" + std::to_string(pass);
        // Slang indexes both module name and source path. Accumulate/sharpen
        // must not register two differently specialized modules at one path.
        const auto module_path = path.parent_path() / (module_name + path.extension().string());
        auto module = session->loadModuleFromSourceString(module_name.c_str(), module_path.string().c_str(),
            source.c_str(), diagnostic.writeRef());
        if (!module) require(SLANG_FAIL, diagnostic);
        Slang::ComPtr<slang::IEntryPoint> entry;
        require(module->findEntryPointByName("CS", entry.writeRef()));
        slang::IComponentType* components[] = {module, entry};
        Slang::ComPtr<slang::IComponentType> composed, linked;
        require(session->createCompositeComponentType(components, 2, composed.writeRef(), diagnostic.writeRef()), diagnostic);
        require(composed->link(linked.writeRef(), diagnostic.writeRef()), diagnostic);
        Slang::ComPtr<rhi::IShaderProgram> program;
        rhi::ShaderProgramDesc program_desc{};
        program_desc.slangGlobalScope = linked;
        require(context.device->createShaderProgram(program_desc, program.writeRef(), diagnostic.writeRef()), diagnostic);
        auto pipeline = std::make_unique<Fsr2Pipeline>();
        rhi::ComputePipelineDesc compute{};
        compute.program = program;
        require(context.device->createComputePipeline(compute, pipeline->pipeline.writeRef()));
        *out = {};
        auto* reflection = linked->getLayout();
        for (uint32_t i = 0; i < reflection->getParameterCount(); ++i) {
            const char* name = reflection->getParameterByIndex(i)->getName();
            if (!name) continue;
            if (std::strncmp(name, "rw_", 3) == 0) {
                if (out->uavCount >= FFX_MAX_NUM_UAVS) throw std::runtime_error("FSR2 UAV capacity exceeded");
                name_binding(out->uavResourceBindings[out->uavCount], name, out->uavCount);
                ++out->uavCount; pipeline->uav.emplace_back(name);
            } else if (std::strncmp(name, "r_", 2) == 0) {
                if (out->srvCount >= FFX_MAX_NUM_SRVS) throw std::runtime_error("FSR2 SRV capacity exceeded");
                name_binding(out->srvResourceBindings[out->srvCount], name, out->srvCount);
                ++out->srvCount; pipeline->srv.emplace_back(name);
            } else if (std::strncmp(name, "cb", 2) == 0) {
                if (out->constCount >= FFX_MAX_NUM_CONST_BUFFERS) throw std::runtime_error("FSR2 constants capacity exceeded");
                name_binding(out->cbResourceBindings[out->constCount], name, out->constCount);
                ++out->constCount; pipeline->constants.emplace_back(name);
            }
        }
        out->pipeline = pipeline.get();
        context.pipelines.push_back(std::move(pipeline));
        return FFX_OK;
    } catch (const std::exception& error) {
        context.error = error.what();
        return FFX_ERROR_BACKEND_API_ERROR;
    }
}
FfxErrorCode fsr_destroy_pipeline(FfxFsr2Interface*, FfxPipelineState* pipeline) {
    if (pipeline->pipeline) static_cast<Fsr2Pipeline*>(pipeline->pipeline)->pipeline.setNull();
    pipeline->pipeline = nullptr;
    return FFX_OK;
}
}
