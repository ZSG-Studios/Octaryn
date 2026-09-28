#pragma once
#include "Pose.h"
#include <slang-rhi.h>
namespace octaryn::client::animation {
struct DeformedVertex {Vec4 position,previous_position,normal,tangent,uv,color;};
static_assert(sizeof(DeformedVertex)==96);
// One resident primitive. Submission and retirement remain with the client frame owner.
class DeformationGpu {
public:
  bool initialize(rhi::IDevice*,const Primitive&,const char* shader_path,std::string& error);
  bool dispatch(rhi::ICommandEncoder*,const DeformationPose& current,const DeformationPose& previous,std::string& error);
  rhi::IBuffer* vertices() const {return output_.get();}
  rhi::IBuffer* bounds() const {return bounds_.get();}
  std::uint32_t vertex_count() const {return vertex_count_;}
private:
  std::uint32_t vertex_count_{},morph_count_{},joint_count_{};
  std::uint32_t groups_x_{},groups_y_{};
  bool skinned_{};
  Slang::ComPtr<rhi::IComputePipeline> clear_,deform_;
  Slang::ComPtr<rhi::IBuffer> source_,morphs_,current_joints_,previous_joints_,current_weights_,previous_weights_,output_,bounds_;
};
}
