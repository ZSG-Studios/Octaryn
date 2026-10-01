#include "DeformationGpu.h"
#include <bit>
#include <cmath>
#include <cstdio>
#include <stdexcept>
using namespace octaryn::client::animation;
namespace {
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
void checked(SlangResult result,const char* reason) {require(SLANG_SUCCEEDED(result),reason);}
void near(float actual,float expected) {require(std::isfinite(actual)&&std::abs(actual-expected)<1e-4f,"GPU deformation result mismatch");}
float orderedValue(std::uint32_t ordered) {return std::bit_cast<float>((ordered&0x80000000u)?ordered^0x80000000u:~ordered);}
void submit(rhi::IDevice* device,rhi::ICommandQueue* queue,rhi::ICommandEncoder* commands) {
  Slang::ComPtr<rhi::IFence> fence;checked(device->createFence({},fence.writeRef()),"animation fence");
  auto command=commands->finish();require(bool(command),"animation finish");auto* cmd=command.get();auto* f=fence.get();std::uint64_t value=1;
  rhi::SubmitDesc desc{};desc.commandBuffers=&cmd;desc.commandBufferCount=1;desc.signalFences=&f;desc.signalFenceValues=&value;desc.signalFenceCount=1;
  checked(queue->submit(desc),"animation submit");checked(device->waitForFences(1,&f,&value,true,30'000'000'000ull),"animation GPU timeout");
}
}
bool probe_animation_gpu(rhi::IDevice* device,rhi::ICommandQueue* queue,const char* shader_path) {
  try {
    Primitive primitive;primitive.skin=0;primitive.morph_count=1;primitive.vertices.resize(65);primitive.morphs.resize(65);
    for(auto& v:primitive.vertices) {v.position={1,0,0,1};v.normal={1,1,0,0};v.tangent={0,0,1,1};v.weights[0]=.75f;v.weights[4]=.25f;v.joints[4]=1;}
    primitive.vertices.back().position[0]=2;for(auto& delta:primitive.morphs)delta.position={2,0,0,0};
    DeformationPose current,previous;current.weights={.5f};previous.weights={0};
    current.joints.push_back({{Vec4{-2,0,0,0},Vec4{0,3,0,2},Vec4{0,0,4,0},Vec4{0,0,0,1}}});
    previous.joints.push_back({{Vec4{1,0,0,0},Vec4{0,1,0,0},Vec4{0,0,1,0},Vec4{0,0,0,1}}});
    current.joints.push_back(current.joints[0]);previous.joints.push_back(previous.joints[0]);
    DeformationGpu deformation;std::string error;
    if(!deformation.initialize(device,primitive,shader_path,error))throw std::runtime_error(error);
    auto commands=queue->createCommandEncoder();
    if(!deformation.dispatch(commands,current,previous,error))throw std::runtime_error(error);
    submit(device,queue,commands);
    std::vector<DeformedVertex> observed(65);std::array<std::uint32_t,7> bounds{};
    checked(device->readBuffer(deformation.vertices(),0,observed.size()*sizeof(DeformedVertex),observed.data()),"deformation readback");
    checked(device->readBuffer(deformation.bounds(),0,sizeof(bounds),bounds.data()),"deformation bounds readback");
    for(std::size_t i=0;i<observed.size();++i) {
      near(observed[i].position[0],i==64?-6.f:-4.f);near(observed[i].position[1],2);
      near(observed[i].previous_position[0],i==64?2.f:1.f);near(observed[i].previous_position[1],0);
      const float length=std::sqrt(.25f+1.f/9.f);near(observed[i].normal[0],-.5f/length);near(observed[i].normal[1],(1.f/3.f)/length);
      near(observed[i].tangent[2],1);near(observed[i].tangent[3],-1);
    }
    near(orderedValue(bounds[0]),-6);near(orderedValue(bounds[3]),2);near(orderedValue(bounds[1]),0);near(orderedValue(bounds[4]),2);require(bounds[6]==0,"invalid deformation output");
    std::printf("animation_gpu_probe passed=1 vertices=65 current_previous=1 morph=1 skin=1 normal=1 tangent=1 union_bounds=1\n");return true;
  } catch(const std::exception& e) {std::fprintf(stderr,"animation_gpu_probe failed: %s\n",e.what());return false;}
}
