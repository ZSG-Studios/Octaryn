#include "RayGeometry.h"
#include "MapRendererInternal.h"
#include "RhiShader.h"
#include <slang-rhi/shader-cursor.h>
#include <slang-rhi/acceleration-structure-utils.h>
#include <array>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <stdexcept>
using namespace octaryn::client::rendering;
using namespace octaryn::client::rendering::virtual_geometry;
using Slang::ComPtr;
namespace {
void check(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
void checked(SlangResult result,const char* reason) {check(SLANG_SUCCEEDED(result),reason);}
ComPtr<rhi::IBuffer> buffer(rhi::IDevice* device,std::uint64_t size,unsigned stride,rhi::BufferUsage usage,const void* data=nullptr) {
  rhi::BufferDesc desc{};desc.size=size;desc.elementSize=stride;desc.usage=usage|rhi::BufferUsage::CopySource;
  desc.defaultState=rhi::is_set(usage,rhi::BufferUsage::UnorderedAccess)?rhi::ResourceState::UnorderedAccess:
      rhi::is_set(usage,rhi::BufferUsage::AccelerationStructureBuildInput)?rhi::ResourceState::AccelerationStructureBuildInput:
      rhi::ResourceState::ShaderResource;
  auto result=device->createBuffer(desc,data);check(bool(result),"map ray probe buffer");return result;
}
std::uint64_t handle(rhi::IBuffer* buffer,rhi::BufferRange range=rhi::kEntireBuffer) {
  rhi::DescriptorHandle value{};checked(buffer->getDescriptorHandle(rhi::DescriptorHandleAccess::Read,
      rhi::Format::Undefined,range,&value),"map ray bindless descriptor");return value.value;
}
}
bool probe_map_ray_materials(rhi::IDevice* device,rhi::ICommandQueue* queue,const RaySnapshot& source,const char* path) {
  try {
    std::array<MapRayMaterial,3> materials{};
    for(unsigned i=0;i<3;++i) {
      materials[i].roughness=.2f+.3f*i;
      materials[i].padding[0]=100000; // Authored offsets are unrelated to expanded cluster triangles.
    }
    materials[2].alpha_mode=1;materials[2].alpha_cutoff=.5f;materials[2].base_color[3]=0;
    constexpr std::uint64_t material_offset=16*sizeof(MapRayMaterial);
    static_assert(material_offset==4864 && material_offset%256==0);
    std::array<MapRayMaterial,19> padded{};
    for(auto& record:padded) {record.roughness=.99f;record.alpha_mode=1;record.base_color[3]=0;record.alpha_cutoff=.5f;}
    std::copy(materials.begin(),materials.end(),padded.begin()+16);
    auto material_buffer=buffer(device,sizeof(padded),sizeof(MapRayMaterial),rhi::BufferUsage::ShaderResource,padded.data());
    const auto static_count=source.batches.size()+1,count=static_count+1;
    std::vector<MapRayGeometry> records(count);
    std::vector<rhi::AccelerationStructureInstanceDescGeneric> instances(count);
    for(std::size_t i=0;i<count;++i) {
      const auto batch=i<source.batches.size()?i:0;
      records[i]={handle(source.vertices),handle(source.indices),handle(material_buffer,{material_offset,sizeof(materials)}),handle(source.triangle_records),
          source.batches[batch].first_triangle,1};
      records[i].vertex_stride=source.vertex_stride;
      auto& instance=instances[i];instance.transform[0][0]=instance.transform[1][1]=instance.transform[2][2]=1;
      if(i==source.batches.size()) {
        auto& record=records[i];record.clustered|=2;record.orientation=-1;
        record.world={-2,0,0,100,0,3,0,0,.5f,0,1,0};
        record.normal={-.5f,0,.25f,0,0,1.f/3,0,0,0,0,1,0};
        for(unsigned row=0;row<3;++row)for(unsigned column=0;column<4;++column)
          instance.transform[row][column]=record.world[row*4+column];
      } else if(i==static_count)instance.transform[0][3]=200;
      instance.instanceID=0x800000u|static_cast<unsigned>(i);instance.instanceMask=255;
      instance.flags=rhi::AccelerationStructureInstanceFlags::TriangleFacingCullDisable;
      instance.accelerationStructure=source.blas[batch]->getHandle();
    }
    const auto type=rhi::getAccelerationStructureInstanceDescType(device);
    const auto stride=rhi::getAccelerationStructureInstanceDescSize(type);
    std::vector<std::uint8_t> native(count*stride);
    rhi::convertAccelerationStructureInstanceDescs(count,type,native.data(),stride,instances.data(),sizeof(instances[0]));
    auto instance_buffer=buffer(device,native.size(),static_cast<unsigned>(stride),rhi::BufferUsage::AccelerationStructureBuildInput,native.data());
    auto record_buffer=buffer(device,records.size()*sizeof(MapRayGeometry),sizeof(MapRayGeometry),rhi::BufferUsage::ShaderResource,records.data());
    rhi::AccelerationStructureBuildInput input{};input.type=rhi::AccelerationStructureBuildInputType::Instances;
    input.instances.instanceBuffer=instance_buffer;input.instances.instanceStride=static_cast<unsigned>(stride);input.instances.instanceCount=static_cast<unsigned>(count);
    rhi::AccelerationStructureBuildDesc build{};build.inputs=&input;build.inputCount=1;
    rhi::AccelerationStructureSizes sizes{};checked(device->getAccelerationStructureSizes(build,&sizes),"map probe TLAS size");
    rhi::AccelerationStructureDesc desc{};desc.kind=rhi::AccelerationStructureKind::TopLevel;desc.size=sizes.accelerationStructureSize;
    ComPtr<rhi::IAccelerationStructure> scene;checked(device->createAccelerationStructure(desc,scene.writeRef()),"map probe TLAS");
    auto scratch=buffer(device,std::max<std::uint64_t>(4,sizes.scratchSize),4,rhi::BufferUsage::UnorderedAccess);
    auto output=buffer(device,19*16,16,rhi::BufferUsage::UnorderedAccess);
    auto commands=queue->createCommandEncoder();commands->globalBarrier();
    commands->buildAccelerationStructure(build,scene,nullptr,scratch,0,nullptr);commands->globalBarrier();
    ComPtr<rhi::IComputePipeline> pipeline;check(create_rhi_compute_pipeline(device,path,"probeMapRays",pipeline),"production map ray shader");
    auto pass=commands->beginComputePass();auto root=pass->bindPipeline(pipeline);check(root,"map ray bind");rhi::ShaderCursor cursor(root);
    checked(cursor["mapRayMeshes"].setBinding(rhi::Binding(record_buffer)),"map ray record bind");
    checked(cursor["rayScene"].setBinding(rhi::Binding(scene)),"map ray scene bind");
    checked(cursor["mapResults"].setBinding(rhi::Binding(output)),"map ray output bind");
    const float settings[4]{1,100,.002f,0};const auto dynamic=static_cast<unsigned>(static_count);
    checked(cursor["raySettings"].setData(settings,sizeof(settings)),"map ray settings");
    checked(cursor["mapDynamicInstanceStart"].setData(dynamic),"map ray dynamic offset");
    pass->dispatchCompute(1,1,1);pass->end();
    auto submission=commands->finish();auto* command=submission.get();auto fence=device->createFence({});auto* signal=fence.get();std::uint64_t value=1;
    rhi::SubmitDesc submit{};submit.commandBuffers=&command;submit.commandBufferCount=1;submit.signalFences=&signal;submit.signalFenceValues=&value;submit.signalFenceCount=1;
    checked(queue->submit(submit),"map ray probe submit");checked(device->waitForFences(1,&signal,&value,true,30'000'000'000ull),"map ray probe timeout");
    std::array<std::array<unsigned,4>,19> results{};checked(device->readBuffer(output,0,sizeof(results),results.data()),"map ray probe readback");
    for(unsigned i=0;i<19;++i) {
      const auto material=i>=17?0:i%3;
      if(material==2)check(results[i][0]==UINT32_MAX,"production alpha mask failed");
      else check(results[i][0]==material && results[i][1]==std::bit_cast<unsigned>(materials[material].roughness) &&
          results[i][2]==(unsigned(i==18)|6u) && std::abs(std::bit_cast<float>(results[i][3])-2.f)<1e-5f,
          "production grouped material or dynamic classification failed");
    }
    std::printf("map_ray_materials=passed multi_blas=2 authored_materials=3 material_offset=4864 alpha_mask=passed dynamic_offset=3 attributes=exact vertex_stride=%u compact_flat_normal=passed mirrored_nonuniform=passed offscreen=covered\n",source.vertex_stride);return true;
  } catch(const std::exception& error) {std::fprintf(stderr,"map_ray_materials=failed reason=%s\n",error.what());return false;}
}
