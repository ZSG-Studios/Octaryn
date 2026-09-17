#include "WorldRendererInternal.h"
#include "../SplitRadianceCascades/System.h"
#include "../SplitRadianceCascades/SrcProfile.h"

namespace octaryn::client::rendering {
namespace {
bool bind_trace(rhi::IShaderObject* root,void* context) {
  auto& r=*static_cast<WorldRenderer*>(context);
  return r.trace_upload.bind(root,r.active_frame) && bind_world_atlas(r.atlas,root);
}
}
bool world_src_initialize(WorldRenderer& r) {
  SrcConfig config;
  std::string error;
  if(!src_environment_config(config,error)) {
    std::fprintf(stderr,"world_src_config_failed %s\n",error.c_str());
    return false;
  }
  if(!src_create(r.src,r.device.get(),config,error)) {
    std::fprintf(stderr,"world_src_create_failed %s\n",error.c_str());
    return false;
  }
  return true;
}
bool world_src_update(WorldRenderer& r,rhi::ICommandEncoder* commands) {
  if(!r.src.initialized || !commands)return true;
  const auto width=unsigned(r.render_width()),height=unsigned(r.render_height());
  if(!src_resize(r.src,r.device.get(),width,height)) {
    std::fprintf(stderr,"src_update_failed stage=resize\n");return false;
  }
  if(!src_prepare(r.src,commands,unsigned(r.trace_world.epoch()))) {
    std::fprintf(stderr,"src_update_failed stage=prepare\n");return false;
  }
  SrcFrame frame{};
  frame.width=width;frame.height=height;frame.frame=unsigned(r.frames);
  frame.geometry_epoch=unsigned(r.trace_upload.stats().generation);
  const float eye[3]={r.draw_uniforms[0],r.draw_uniforms[1],r.draw_uniforms[2]};
  const int anchor[3]={int(std::floor(eye[0])),int(std::floor(eye[1])),int(std::floor(eye[2]))};
  frame.eye[0]=eye[0]-float(anchor[0]);frame.eye[1]=eye[1]-float(anchor[1]);
  frame.eye[2]=eye[2]-float(anchor[2]);
  frame.origin[0]=anchor[0];frame.origin[1]=anchor[1];frame.origin[2]=anchor[2];
  frame.positions=r.target().hdr.views[1];
  frame.normals=r.target().hdr.views[2];
  frame.bind_trace=&bind_trace;frame.trace_context=&r;
  frame.profile=&r.lighting_profile;
  // SRC scratch state is single-instance; two frames in flight would let the next
  // frame's reset/decay clobber buffers the previous frame still reads on the GPU.
  if(!r.frame_queue.wait(1u-r.active_frame))return false;
  if(src_profile_enabled() && !src_profile_poll(r.src,r.device.get(),r.frames))return false;
  if(!src_dispatch(r.src,commands,frame)) {
    std::fprintf(stderr,"src_update_failed stage=dispatch\n");return false;
  }
  return true;
}
bool world_src_bind(WorldRenderer& r,rhi::IShaderObject* root) {
  return src_bind(r.src,root);
}
}
