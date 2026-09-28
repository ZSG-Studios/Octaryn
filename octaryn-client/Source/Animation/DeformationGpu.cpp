#include "DeformationGpu.h"
#include "RhiShader.h"
#include <slang-rhi/shader-cursor.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace octaryn::client::animation {
namespace {
void check(bool value,const char* text) {if(!value)throw std::runtime_error(text);}
void checked(SlangResult result,const char* text) {check(SLANG_SUCCEEDED(result),text);}
Slang::ComPtr<rhi::IBuffer> buffer(rhi::IDevice* device,std::size_t size,std::uint32_t stride,const void* data=nullptr,bool geometry=false) {
  rhi::BufferDesc desc{};desc.size=std::max(size,static_cast<std::size_t>(stride));desc.elementSize=stride;
  desc.usage=rhi::BufferUsage::ShaderResource|rhi::BufferUsage::UnorderedAccess|rhi::BufferUsage::CopySource|rhi::BufferUsage::CopyDestination;
  if(geometry)desc.usage|=rhi::BufferUsage::AccelerationStructureBuildInput;
  desc.defaultState=rhi::ResourceState::UnorderedAccess;
  Slang::ComPtr<rhi::IBuffer> result;checked(device->createBuffer(desc,data,result.writeRef()),"cannot create deformation buffer");return result;
}
void bind(rhi::IShaderObject* root,const char* name,rhi::IBuffer* value) {checked(rhi::ShaderCursor(root)[name].setBinding(rhi::Binding(value)),name);}
}
bool DeformationGpu::initialize(rhi::IDevice* device,const Primitive& primitive,const char* path,std::string& error) {
  try {
    check(device&&path&&!primitive.vertices.empty(),"invalid deformation initialization");
    check(primitive.vertices.size()<=UINT32_MAX,"deformation vertex count exceeds GPU limit");
    check(primitive.morphs.size()==primitive.vertices.size()*primitive.morph_count,"deformation morph count mismatch");
    DeformationGpu next;next.vertex_count_=static_cast<std::uint32_t>(primitive.vertices.size());next.morph_count_=primitive.morph_count;next.skinned_=primitive.skin>=0;
    const auto& limits=device->getInfo().limits;
    check(limits.maxComputeThreadsPerGroup>=64&&limits.maxComputeDispatchThreadGroups[0]>0,"deformation compute limits unsupported");
    const auto groups=next.vertex_count_/64+(next.vertex_count_%64!=0);
    next.groups_x_=std::min(groups,limits.maxComputeDispatchThreadGroups[0]);
    next.groups_y_=groups/next.groups_x_+(groups%next.groups_x_!=0);
    check(next.groups_y_<=limits.maxComputeDispatchThreadGroups[1],"deformation dispatch exceeds GPU limits");
    if(next.skinned_)for(const auto& v:primitive.vertices)for(std::size_t i=0;i<8;++i)if(v.weights[i]>0) {
      check(v.joints[i]<4096,"deformation joint index exceeds import limit");next.joint_count_=std::max(next.joint_count_,v.joints[i]+1);
    }
    check(!next.skinned_||next.joint_count_>0,"skinned primitive has no joint influences");
    check(rendering::create_rhi_compute_pipeline(device,path,"clearBounds",next.clear_),"cannot create deformation clear pipeline");
    check(rendering::create_rhi_compute_pipeline(device,path,"deform",next.deform_),"cannot create deformation pipeline");
    next.source_=buffer(device,primitive.vertices.size()*sizeof(SourceVertex),sizeof(SourceVertex),primitive.vertices.data());
    next.morphs_=buffer(device,primitive.morphs.size()*sizeof(MorphDelta),sizeof(MorphDelta),primitive.morphs.empty()?nullptr:primitive.morphs.data());
    const auto joints=static_cast<std::size_t>(std::max(next.joint_count_,1u))*sizeof(MatrixRows);
    next.current_joints_=buffer(device,joints,sizeof(MatrixRows));next.previous_joints_=buffer(device,joints,sizeof(MatrixRows));
    const auto weights=static_cast<std::size_t>(std::max(next.morph_count_,1u))*sizeof(float);
    next.current_weights_=buffer(device,weights,sizeof(float));next.previous_weights_=buffer(device,weights,sizeof(float));
    next.output_=buffer(device,primitive.vertices.size()*sizeof(DeformedVertex),sizeof(DeformedVertex),nullptr,true);next.bounds_=buffer(device,7*sizeof(std::uint32_t),sizeof(std::uint32_t));
    *this=std::move(next);error.clear();return true;
  } catch(const std::exception& e) {error=e.what();return false;}
}
bool DeformationGpu::dispatch(rhi::ICommandEncoder* commands,const DeformationPose& current,const DeformationPose& previous,std::string& error) {
  try {
    check(commands&&deform_&&clear_,"uninitialized deformation GPU");
    check(current.weights.size()==morph_count_&&previous.weights.size()==morph_count_,"deformation GPU morph palette mismatch");
    check(!skinned_||(current.joints.size()>=joint_count_&&previous.joints.size()>=joint_count_),"deformation GPU joint palette mismatch");
    for(const auto* pose:{&current,&previous}) {
      for(float weight:pose->weights)check(std::isfinite(weight),"nonfinite deformation GPU morph weight");
      for(std::uint32_t i=0;i<joint_count_;++i)for(const auto& row:pose->joints[i].rows)for(float value:row)check(std::isfinite(value),"nonfinite deformation GPU joint matrix");
    }
    if(joint_count_) {
      const auto size=static_cast<std::size_t>(joint_count_)*sizeof(MatrixRows);
      checked(commands->uploadBufferData(current_joints_,0,size,current.joints.data()),"upload current skin");
      checked(commands->uploadBufferData(previous_joints_,0,size,previous.joints.data()),"upload previous skin");
    }
    if(morph_count_) {
      checked(commands->uploadBufferData(current_weights_,0,morph_count_*sizeof(float),current.weights.data()),"upload current morph weights");
      checked(commands->uploadBufferData(previous_weights_,0,morph_count_*sizeof(float),previous.weights.data()),"upload previous morph weights");
    }
    auto pass=commands->beginComputePass();auto root=pass->bindPipeline(clear_);check(root,"bind deformation bounds clear");
    bind(root,"deformationBounds",bounds_);pass->dispatchCompute(1,1,1);pass->end();commands->globalBarrier();
    pass=commands->beginComputePass();root=pass->bindPipeline(deform_);check(root,"bind deformation pipeline");
    bind(root,"sourceVertices",source_);bind(root,"morphDeltas",morphs_);bind(root,"currentJoints",current_joints_);bind(root,"previousJoints",previous_joints_);
    bind(root,"currentWeights",current_weights_);bind(root,"previousWeights",previous_weights_);bind(root,"deformedVertices",output_);bind(root,"deformationBounds",bounds_);
    checked(rhi::ShaderCursor(root)["vertexCount"].setData(vertex_count_),"set deformation vertex count");
    checked(rhi::ShaderCursor(root)["morphCount"].setData(morph_count_),"set deformation morph count");
    checked(rhi::ShaderCursor(root)["jointCount"].setData(joint_count_),"set deformation joint count");
    const auto width=groups_x_*64;checked(rhi::ShaderCursor(root)["dispatchWidth"].setData(width),"set deformation dispatch width");
    pass->dispatchCompute(groups_x_,groups_y_,1);pass->end();commands->globalBarrier();error.clear();return true;
  } catch(const std::exception& e) {error=e.what();return false;}
}
}
