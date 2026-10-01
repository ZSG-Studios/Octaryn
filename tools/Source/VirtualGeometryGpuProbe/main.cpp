#include "RhiShader.h"
#include "RendererCapabilities.h"
#include <slang-rhi/shader-cursor.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
using Slang::ComPtr;
using namespace octaryn::client::rendering;
bool probe_virtual_geometry_selection(rhi::IDevice*,rhi::ICommandQueue*,const char*);
bool probe_instance_selection_gpu(rhi::IDevice*,rhi::ICommandQueue*,const char*);
bool probe_shared_selection(rhi::IDevice*,rhi::ICommandQueue*,const char*);
bool probe_scene_pool(rhi::IDevice*,rhi::ICommandQueue*);
bool probe_scene_raster(rhi::IDevice*,rhi::ICommandQueue*,const char*);
bool probe_hybrid_geometry(rhi::IDevice*,rhi::ICommandQueue*,const char*);
bool probe_animation_gpu(rhi::IDevice*,rhi::ICommandQueue*,const char*);
bool probe_ray_geometry(rhi::IDevice*,rhi::ICommandQueue*,const char*,const char*);
bool probe_ray_size_query(rhi::IDevice*);
namespace {
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void checked(SlangResult result,const char* message) {require(SLANG_SUCCEEDED(result),message);}
struct Debug: rhi::IDebugCallback {
  unsigned errors{};
  SLANG_NO_THROW void SLANG_MCALL handleMessage(rhi::DebugMessageType type,rhi::DebugMessageSource,const char* message) override {
    if(type==rhi::DebugMessageType::Error)++errors;
    std::fprintf(stderr,"rhi: %s\n",message);
  }
};
ComPtr<rhi::IBuffer> buffer(rhi::IDevice* device,size_t size,unsigned stride,const void* initial,bool indirect=false) {
  rhi::BufferDesc desc{};desc.size=size;desc.elementSize=stride;
  desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|
      rhi::BufferUsage::CopySource|rhi::BufferUsage::CopyDestination;
  if(indirect)desc.usage|=rhi::BufferUsage::IndirectArgument;
  desc.defaultState=rhi::ResourceState::UnorderedAccess;
  ComPtr<rhi::IBuffer> result;checked(device->createBuffer(desc,initial,result.writeRef()),"create buffer");return result;
}
void bind(rhi::IShaderObject* root,const char* name,rhi::IBuffer* value) {
  checked(rhi::ShaderCursor(root)[name].setBinding(rhi::Binding(value)),name);
}
void submit(rhi::IDevice* device,rhi::ICommandQueue* queue,rhi::ICommandEncoder* commands) {
  ComPtr<rhi::IFence> fence;checked(device->createFence({},fence.writeRef()),"create fence");
  auto finished=commands->finish();require(bool(finished),"finish commands");
  rhi::ICommandBuffer* cmd=finished.get();rhi::IFence* f=fence.get();uint64_t value=1;
  rhi::SubmitDesc desc{};desc.commandBuffers=&cmd;desc.commandBufferCount=1;
  desc.signalFences=&f;desc.signalFenceValues=&value;desc.signalFenceCount=1;
  checked(queue->submit(desc),"submit");
  checked(device->waitForFences(1,&f,&value,true,30'000'000'000ull),"GPU fence timeout");
}
}
int main(int argc,char** argv) {
  try {
    require((argc==2 || (argc==3 && !std::strcmp(argv[2],"--ray-size-query"))) &&
        (!std::strcmp(argv[1],"dx12") || !std::strcmp(argv[1],"vulkan")),"usage: virtual_geometry_gpu_probe dx12|vulkan [--ray-size-query]");
    Debug debug;rhi::DeviceDesc desc{};
    rhi::DebugLayerOptions validation{};validation.coreValidation=true;validation.required=true;
    checked(rhi::getRHI()->setDebugLayerOptions(validation),"enable GPU core validation");
    desc.deviceType=!std::strcmp(argv[1],"dx12")?rhi::DeviceType::D3D12:rhi::DeviceType::Vulkan;
    desc.slang.targetProfile=desc.deviceType==rhi::DeviceType::D3D12?"sm_6_6":"spirv_1_5";
    rhi::D3D12DeviceExtendedDesc dx12{};dx12.rootParameterShaderAttributeName="root";
    if(desc.deviceType==rhi::DeviceType::D3D12)desc.next=&dx12;
    desc.debugCallback=&debug;desc.enableValidation=true;
    ComPtr<rhi::IDevice> device;checked(rhi::getRHI()->createDevice(desc,device.writeRef()),"create device");
    auto capabilities=renderer_capabilities(device,desc.bindless);print_renderer_capabilities(capabilities);
    require(capabilities.virtual_geometry(),"required virtual geometry capabilities missing");
    std::printf("geometry_probe adapter=%s backend=%s\n",device->getInfo().adapterName,argv[1]);
    if(argc==3)return probe_ray_size_query(device)?0:1;
    const uint64_t initialWinners[]{0,UINT64_MAX};std::array<uint32_t,12> zero{};
    auto winners=buffer(device,16,8,initialWinners);
    auto arguments=buffer(device,48,4,zero.data(),true);
    auto counts=buffer(device,16,4,zero.data(),true);
    auto executions=buffer(device,4,4,zero.data());
    ComPtr<rhi::IComputePipeline> prepare;
    require(create_rhi_compute_pipeline(device,OCTARYN_GEOMETRY_PROBE_SHADER,"prepare",prepare),"prepare pipeline");
    auto queue=device->getQueue(rhi::QueueType::Graphics);require(bool(queue),"graphics queue");
    auto commands=queue->createCommandEncoder();
    auto compute=commands->beginComputePass();auto root=compute->bindPipeline(prepare);
    bind(root,"winners",winners);bind(root,"arguments",arguments);bind(root,"counts",counts);
    compute->dispatchCompute(1,1,1);compute->end();
    ComPtr<rhi::IShaderProgram> program;const char* entries[]{"meshMain","fragmentMain"};
    require(create_rhi_program(device,OCTARYN_GEOMETRY_PROBE_SHADER,entries,2,program),"mesh shader program");
    rhi::RenderPipelineDesc pipelineDesc{};pipelineDesc.program=program;
    rhi::ColorTargetDesc color{};color.format=rhi::Format::RGBA8Unorm;
    pipelineDesc.targetCount=1;pipelineDesc.targets=&color;
    pipelineDesc.rasterizer.cullMode=rhi::CullMode::None;
    ComPtr<rhi::IRenderPipeline> pipeline;checked(device->createRenderPipeline(pipelineDesc,pipeline.writeRef()),"mesh pipeline");
    rhi::TextureDesc textureDesc{};textureDesc.size={16,16,1};textureDesc.format=color.format;
    textureDesc.usage=rhi::TextureUsage::RenderTarget;textureDesc.defaultState=rhi::ResourceState::RenderTarget;
    auto texture=device->createTexture(textureDesc);require(bool(texture),"render target");
    auto view=texture->getDefaultView();require(bool(view),"render target view");
    rhi::RenderPassColorAttachment attachment{};attachment.view=view;attachment.loadOp=rhi::LoadOp::Clear;
    rhi::RenderPassDesc passDesc{};passDesc.colorAttachments=&attachment;passDesc.colorAttachmentCount=1;
    auto pass=commands->beginRenderPass(passDesc);root=pass->bindPipeline(pipeline);bind(root,"executions",executions);
    rhi::RenderState state{};state.viewportCount=state.scissorRectCount=1;
    state.viewports[0]=rhi::Viewport::fromSize(16,16);state.scissorRects[0]=rhi::ScissorRect::fromSize(16,16);
    pass->setRenderState(state);
    pass->drawMeshTasksIndirect(2,{arguments.get(),12},{counts.get(),4}); // 3 groups, count-limited.
    pass->drawMeshTasksIndirect(2,{arguments.get(),12},{counts.get(),8}); // Zero count.
    pass->drawMeshTasksIndirect(1,{arguments.get(),36}); // Zero dispatch dimension.
    pass->drawMeshTasksIndirect(1,{arguments.get(),24}); // 5 groups, no count buffer.
    pass->drawMeshTasksIndirect(1,{arguments.get(),12},{counts.get(),12}); // Max count clamps to 3 groups.
    pass->end();submit(device,queue,commands);
    uint64_t observed[2]{};checked(device->readBuffer(winners,0,sizeof(observed),observed),"atomic readback");
    uint32_t dispatched{};checked(device->readBuffer(executions,0,4,&dispatched),"mesh readback");
    require(observed[0]==(uint64_t(64)<<32) && observed[1]==((uint64_t(1)<<32)|63),"incorrect uint64 atomic winners");
    require(dispatched==11,"incorrect indirect mesh dispatch execution count");
    require(probe_virtual_geometry_selection(device,queue,OCTARYN_GEOMETRY_SHADER_DIR "/Selection.slang"),"GPU selection parity");
    require(probe_instance_selection_gpu(device,queue,OCTARYN_GEOMETRY_SHADER_DIR "/Selection.slang"),"GPU affine instance union");
    require(probe_shared_selection(device,queue,OCTARYN_GEOMETRY_SHADER_DIR "/Selection.slang"),"shared GPU selection owners");
    require(probe_scene_pool(device,queue),"shared packed page pool");
    require(probe_scene_raster(device,queue,OCTARYN_GEOMETRY_SHADER_DIR),"shared scene raster domains");
    require(probe_hybrid_geometry(device,queue,OCTARYN_GEOMETRY_SHADER_DIR),"hybrid visibility rasterization");
    require(probe_animation_gpu(device,queue,OCTARYN_ANIMATION_SHADER),"animation deformation");
    require(probe_ray_geometry(device,queue,OCTARYN_GEOMETRY_SHADER_DIR "/RayExpand.slang",OCTARYN_RAY_PROBE_SHADER),"paged ray geometry");
    require(debug.errors==0,"RHI validation errors");
    std::printf("geometry_gpu_probe passed=1 atomic_min_max=1 gpu_written_indirect=1 count_offset_zero=1 mesh_groups=%u\n",dispatched);
    return 0;
  } catch(const std::exception& error) {std::fprintf(stderr,"geometry_gpu_probe failed: %s\n",error.what());return 1;}
}
