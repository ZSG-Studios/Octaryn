#include "Import.h"
#include <algorithm>
#include <set>
namespace octaryn::client::animation::importing {
void clips(const fastgltf::Asset& source,Asset& result,const LoadLimits& limits) {
  using namespace fastgltf;std::size_t key_count=0;
  for(const auto& animation:source.animations) {
    Clip clip;clip.name=animation.name;std::set<std::pair<std::size_t,Path>> targets;
    for(const auto& channel:animation.channels) {
      if(!channel.nodeIndex)continue;
      check(*channel.nodeIndex<result.nodes.size()&&channel.samplerIndex<animation.samplers.size(),"invalid animation channel target or sampler");
      const auto& sampler=animation.samplers[channel.samplerIndex];Channel dest;dest.node=static_cast<std::uint32_t>(*channel.nodeIndex);
      switch(channel.path) {
        case AnimationPath::Translation:dest.path=Path::Translation;dest.components=3;break;
        case AnimationPath::Rotation:dest.path=Path::Rotation;dest.components=4;break;
        case AnimationPath::Scale:dest.path=Path::Scale;dest.components=3;break;
        case AnimationPath::Weights:dest.path=Path::Weights;dest.components=static_cast<std::uint32_t>(result.nodes[dest.node].weights.size());break;
        default:check(false,"unsupported animation path");
      }
      check(dest.components>0,"weight channel targets node without morphs");
      check(dest.path==Path::Weights||!result.nodes[dest.node].has_matrix,"TRS animation targets matrix node");
      check(targets.insert({dest.node,dest.path}).second,"duplicate animation channel target");
      switch(sampler.interpolation) {
        case AnimationInterpolation::Step:dest.interpolation=Interpolation::Step;break;
        case AnimationInterpolation::Linear:dest.interpolation=Interpolation::Linear;break;
        case AnimationInterpolation::CubicSpline:dest.interpolation=Interpolation::CubicSpline;break;
        default:check(false,"unsupported animation interpolation");
      }
      dest.times=values<float>(source,sampler.inputAccessor,AccessorType::Scalar,limits.keys-key_count);
      check(!dest.times.empty(),"empty animation sampler");key_count+=dest.times.size();
      for(std::size_t i=0;i<dest.times.size();++i)check(std::isfinite(dest.times[i])&&dest.times[i]>=0&&(i==0||dest.times[i]>dest.times[i-1]),"animation times must increase and be finite");
      clip.duration=std::max(clip.duration,dest.times.back());
      const std::size_t factor=dest.interpolation==Interpolation::CubicSpline?3:1;
      check(factor==1||dest.times.size()>=2,"cubic animation needs two keys");
      const std::size_t vectors=dest.times.size()*factor;
      if(dest.path==Path::Weights)dest.values=values<float>(source,sampler.outputAccessor,AccessorType::Scalar,vectors*dest.components);
      else if(dest.components==3)for(const auto& v:values<math::fvec3>(source,sampler.outputAccessor,AccessorType::Vec3,vectors))for(int c=0;c<3;++c)dest.values.push_back(v[c]);
      else for(const auto& v:values<math::fvec4>(source,sampler.outputAccessor,AccessorType::Vec4,vectors))for(int c=0;c<4;++c)dest.values.push_back(v[c]);
      check(dest.values.size()==vectors*dest.components,"animation sampler output count mismatch");
      for(float value:dest.values)check(std::isfinite(value),"nonfinite animation sampler output");
      clip.channels.push_back(std::move(dest));
    }
    result.clips.push_back(std::move(clip));
  }
}
}
