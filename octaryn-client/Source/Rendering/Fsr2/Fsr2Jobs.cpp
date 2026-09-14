#include "Fsr2Internal.h"
#include <slang-rhi/shader-cursor.h>
#include <stdexcept>
#include <cstring>

namespace octaryn::client::rendering {
FfxErrorCode fsr_schedule(FfxFsr2Interface* api, const FfxGpuJobDescription* job) {
    auto& context = fsr_backend(api);
    // SDK firstExecution and reset both clear the current lock texture before
    // any consumer. Collapse a clear-only run to its last value per target.
    // This also avoids requiring a transfer-write -> transfer-write barrier
    // from RHI's globalBarrier(), whose contract is write -> read visibility.
    if (job->jobType == FFX_GPU_JOB_CLEAR_FLOAT) {
        for (auto it = context.jobs.rbegin(); it != context.jobs.rend(); ++it) {
            if (it->jobType != FFX_GPU_JOB_CLEAR_FLOAT) break;
            if (it->clearJobDescriptor.target.internalIndex == job->clearJobDescriptor.target.internalIndex) {
                *it = *job;
                return FFX_OK;
            }
        }
    }
    if (context.jobs.size() >= 64) {
        context.error = "FSR2 scheduled job capacity exceeded";
        return FFX_ERROR_BACKEND_API_ERROR;
    }
    context.jobs.push_back(*job); // Includes a deep copy of all SDK constant bytes.
    return FFX_OK;
}
FfxErrorCode fsr_execute(FfxFsr2Interface* api, FfxCommandList command_list) {
    auto& context = fsr_backend(api);
    auto* commands = static_cast<rhi::ICommandEncoder*>(command_list);
    try {
        for (auto& job : context.jobs) {
            commands->globalBarrier();
            if (job.jobType == FFX_GPU_JOB_CLEAR_FLOAT) {
                auto& clear = job.clearJobDescriptor;
                auto& resource = context.resources.at(clear.target.internalIndex);
                if (resource.texture->getDesc().format == rhi::Format::R32Uint) {
                    uint32_t bits[4];
                    std::memcpy(bits, clear.color, sizeof(bits));
                    commands->clearTextureUint(resource.texture, rhi::kEntireTexture, bits);
                } else commands->clearTextureFloat(resource.texture, rhi::kEntireTexture, clear.color);
            } else if (job.jobType == FFX_GPU_JOB_COPY) {
                auto& copy = job.copyJobDescriptor;
                commands->copyTexture(context.resources.at(copy.dst.internalIndex).texture, {}, {},
                    context.resources.at(copy.src.internalIndex).texture, {}, {}, rhi::Extent3D::kWholeTexture);
            } else if (job.jobType == FFX_GPU_JOB_COMPUTE) {
                auto& compute = job.computeJobDescriptor;
                auto& pipeline = *static_cast<Fsr2Pipeline*>(compute.pipeline.pipeline);
                auto* pass = commands->beginComputePass();
                rhi::ShaderCursor root(pass->bindPipeline(pipeline.pipeline));
                auto bind = [&](const std::string& name, rhi::Binding binding) {
                    const auto field = root.getField(name.c_str());
                    if (!field.isValid() || SLANG_FAILED(field.setBinding(binding)))
                        throw std::runtime_error("FSR2 resource binding failed: " + name);
                };
                const auto point = root.getField("s_PointClamp"), linear = root.getField("s_LinearClamp");
                if (point.isValid()) point.setBinding(rhi::Binding(context.point));
                if (linear.isValid()) linear.setBinding(rhi::Binding(context.linear));
                for (uint32_t i = 0; i < compute.pipeline.srvCount; ++i)
                    bind(pipeline.srv[i], rhi::Binding(context.resources.at(compute.srvs[i].internalIndex).srv));
                for (uint32_t i = 0; i < compute.pipeline.uavCount; ++i) {
                    auto& resource = context.resources.at(compute.uavs[i].internalIndex);
                    bind(pipeline.uav[i], rhi::Binding(resource.mips.at(compute.uavMip[i])));
                }
                for (uint32_t i = 0; i < compute.pipeline.constCount; ++i) {
                    auto field = root.getField(pipeline.constants[i].c_str()).getDereferenced();
                    if (!field.isValid() || SLANG_FAILED(field.setData(compute.cbs[i].data, compute.cbs[i].uint32Size * 4)))
                        throw std::runtime_error("FSR2 constant binding failed: " + pipeline.constants[i]);
                }
                pass->dispatchCompute(compute.dimensions[0], compute.dimensions[1], compute.dimensions[2]);
                pass->end();
            } else throw std::runtime_error("Unknown FSR2 GPU job");
        }
        commands->globalBarrier();
        context.jobs.clear();
        return FFX_OK;
    } catch (const std::exception& error) {
        context.jobs.clear();
        context.error = error.what();
        return FFX_ERROR_BACKEND_API_ERROR;
    }
}
}
