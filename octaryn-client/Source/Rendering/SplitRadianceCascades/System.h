#pragma once
#include "Config.h"
#include <slang-rhi.h>
#include <slang-com-ptr.h>
#include <array>
namespace octaryn::client::rendering {
class LightingProfile;
struct SrcFrame {
  unsigned width=0,height=0,frame=0,geometry_epoch=0;
  float eye[4]{};
  int origin[4]{};
  rhi::ITextureView* positions=nullptr;
  rhi::ITextureView* normals=nullptr; // Existing packed voxel/face G-buffer.
  // Shared HDDA + material/atlas/light bindings. Invoked for reflected trace shaders.
  bool (*bind_trace)(rhi::IShaderObject*,void*)=nullptr;
  void* trace_context=nullptr;
  LightingProfile* profile=nullptr;
};
enum class SrcPass:unsigned {Reset,Maintain,Decay,Rehash,Visible,Secondary,Parents,Refresh,RayCount,RayPropagate,RayOffsets,Trace,TraceSecondary,Deposit,Links,Merge,Evaluate,Resolve,Count};
struct SplitRadianceCascades {
  SrcConfig config{};SrcLayout layout{};
  std::array<Slang::ComPtr<rhi::IComputePipeline>,unsigned(SrcPass::Count)> pipelines;
  Slang::ComPtr<rhi::IBuffer> probes,hash,freelist,counters,fluence,weights,radiance,previous,rays,links,distribution,oct,oct_prev;
  Slang::ComPtr<rhi::ITexture> output;
  Slang::ComPtr<rhi::ITextureView> output_view;
  unsigned width=0,height=0,epoch=0;
  std::uint64_t gpu_bytes=0;
  bool initialized=false,reset_pending=true,active=false;
};
bool src_create(SplitRadianceCascades&,rhi::IDevice*,const SrcConfig&,std::string& error);
bool src_resize(SplitRadianceCascades&,rhi::IDevice*,unsigned width,unsigned height);
bool src_prepare(SplitRadianceCascades&,rhi::ICommandEncoder*,unsigned geometry_epoch);
bool src_dispatch(SplitRadianceCascades&,rhi::ICommandEncoder*,const SrcFrame&);
bool src_bind(const SplitRadianceCascades&,rhi::IShaderObject*,const char* name="srcIrradiance");
struct WorldRenderer;
bool world_src_initialize(WorldRenderer&);
bool world_src_update(WorldRenderer&,rhi::ICommandEncoder*);
bool world_src_bind(WorldRenderer&,rhi::IShaderObject*);
}
