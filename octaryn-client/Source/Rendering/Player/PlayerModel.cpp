#include "PlayerModel.h"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace octaryn::client::rendering {
namespace {
using namespace fastgltf;
using namespace fastgltf::math;
void check(bool value,const char* reason) { if(!value) throw std::runtime_error(reason); }
template<class T> std::vector<T> values(const Asset& asset,size_t index,AccessorType type) {
  check(index<asset.accessors.size(),"accessor index out of range");
  const auto& accessor=asset.accessors[index];
  check(accessor.type==type && accessor.count<=1000000,"unsupported accessor shape or size");
  std::vector<T> result;result.reserve(accessor.count);
  iterateAccessor<T>(asset,accessor,[&](T value) {result.push_back(value);});
  return result;
}
size_t attribute(const Primitive& primitive,const char* name) {
  const auto found=primitive.findAttribute(name);
  check(found!=primitive.attributes.end(),"missing player vertex attribute");
  return found->accessorIndex;
}
void load_vertices(const Asset& asset,PlayerModel& model) {
  const auto& primitives=asset.meshes[0].primitives;
  check(!primitives.empty() && primitives.size()<=32,"unsupported primitive count");
  const auto& first=primitives[0];
  const auto positions=values<fvec3>(asset,attribute(first,"POSITION"),AccessorType::Vec3);
  const auto normals=values<fvec3>(asset,attribute(first,"NORMAL"),AccessorType::Vec3);
  const auto uv=values<fvec2>(asset,attribute(first,"TEXCOORD_0"),AccessorType::Vec2);
  const auto joints=values<uvec4>(asset,attribute(first,"JOINTS_0"),AccessorType::Vec4);
  const auto weights=values<fvec4>(asset,attribute(first,"WEIGHTS_0"),AccessorType::Vec4);
  check(!positions.empty() && normals.size()==positions.size() && uv.size()==positions.size() &&
        joints.size()==positions.size() && weights.size()==positions.size(),"vertex attribute count mismatch");
  model.vertices.resize(positions.size());
  for(size_t i=0;i<positions.size();++i) {
    auto& vertex=model.vertices[i];float total=0;
    vertex.position[3]=1.f;
    for(size_t k=0;k<3;++k) {
      check(std::isfinite(positions[i][k]) && std::isfinite(normals[i][k]),"nonfinite vertex");
      vertex.position[k]=positions[i][k];vertex.normal[k]=normals[i][k];
    }
    for(size_t k=0;k<2;++k) {check(std::isfinite(uv[i][k]),"nonfinite UV");vertex.uv[k]=uv[i][k];}
    for(size_t k=0;k<4;++k) {
      check(joints[i][k]<model.joints.size() && std::isfinite(weights[i][k]) && weights[i][k]>=0,
            "invalid skin influence");
      vertex.joints[k]=joints[i][k];vertex.weights[k]=weights[i][k];total+=weights[i][k];
    }
    check(std::abs(total-1)<0.001f,"skin weights must sum to one");
  }
  for(const auto& primitive:primitives) {
    check(primitive.type==PrimitiveType::Triangles && primitive.indicesAccessor.has_value() &&
          primitive.materialIndex.has_value(),"player requires indexed material triangles");
    for(const char* name:{"POSITION","NORMAL","TEXCOORD_0","JOINTS_0","WEIGHTS_0"})
      check(attribute(primitive,name)==attribute(first,name),"player primitives must share vertex accessors");
    auto indices=values<uint32_t>(asset,*primitive.indicesAccessor,AccessorType::Scalar);
    check(!indices.empty() && indices.size()%3==0 && model.indices.size()+indices.size()<=3000000,
          "invalid triangle indices");
    PlayerPrimitive draw;
    draw.first=static_cast<uint32_t>(model.indices.size());draw.count=static_cast<uint32_t>(indices.size());
    draw.first_person_first=static_cast<uint32_t>(model.first_person_indices.size());
    for(auto index:indices) {
      check(index<model.vertices.size(),"vertex index out of range");
    }
    // The authored head shares a material primitive with both arms.
    // First person includes arms only; third person retains every authored triangle.
    for(size_t triangle=0;triangle<indices.size();triangle+=3) {
      bool arms_only=true;
      for(size_t corner=0;corner<3;++corner) {
        const auto& vertex=model.vertices[indices[triangle+corner]];
        for(size_t k=0;k<4;++k) if(vertex.weights[k]>0) {
          const auto& joint_name=model.nodes[model.joints[vertex.joints[k]]].name;
          if(joint_name!="left_arm" && joint_name!="right_arm") arms_only=false;
        }
      }
      if(arms_only) model.first_person_indices.insert(model.first_person_indices.end(),indices.begin()+static_cast<std::ptrdiff_t>(triangle),
          indices.begin()+static_cast<std::ptrdiff_t>(triangle+3));
    }
    draw.first_person_count=static_cast<uint32_t>(model.first_person_indices.size())-draw.first_person_first;
    check(*primitive.materialIndex<asset.materials.size(),"material index out of range");
    const auto& material=asset.materials[*primitive.materialIndex];
    check(material.doubleSided && material.alphaMode!=AlphaMode::Blend &&
          !material.pbrData.baseColorTexture && !material.pbrData.metallicRoughnessTexture &&
          !material.normalTexture && !material.emissiveTexture,"unsupported player material; textures are not silently discarded");
    for(size_t k=0;k<4;++k) draw.color[k]=static_cast<float>(material.pbrData.baseColorFactor[k]);
    draw.metallic=static_cast<float>(material.pbrData.metallicFactor);
    draw.roughness=static_cast<float>(material.pbrData.roughnessFactor);
    draw.alpha_cutoff=material.alphaMode==AlphaMode::Mask?static_cast<float>(material.alphaCutoff):0;
    model.indices.insert(model.indices.end(),indices.begin(),indices.end());model.primitives.push_back(draw);
  }
}
void load_rig(const Asset& asset,PlayerModel& model) {
  check(!asset.nodes.empty() && asset.nodes.size()<=128,"unsupported node count");
  model.nodes.resize(asset.nodes.size());
  for(size_t i=0;i<asset.nodes.size();++i) {
    const auto& node=asset.nodes[i];
    check(std::holds_alternative<TRS>(node.transform),"player requires TRS node transforms");
    const auto& trs=std::get<TRS>(node.transform);
    auto& target=model.nodes[i];target.translation=trs.translation;target.rotation=trs.rotation;
    target.scale=trs.scale;target.name=std::string(node.name);
    for(auto child:node.children) {
      check(child<model.nodes.size() && model.nodes[child].parent<0 && child!=i,"invalid skeleton hierarchy");
      model.nodes[child].parent=static_cast<int>(i);
    }
  }
  for(size_t i=0;i<model.nodes.size();++i) {
    int parent=static_cast<int>(i);size_t depth=0;
    while(parent>=0) {check(++depth<=model.nodes.size(),"cyclic skeleton");parent=model.nodes[static_cast<size_t>(parent)].parent;}
  }
  const auto& skin=asset.skins[0];
  check(!skin.joints.empty() && skin.joints.size()<=PlayerMaxJoints && skin.inverseBindMatrices.has_value(),"unsupported skin");
  for(auto joint:skin.joints) {
    check(joint<model.nodes.size(),"joint node out of range");model.joints.push_back(static_cast<uint32_t>(joint));
  }
  model.inverse_bind=values<fmat4x4>(asset,*skin.inverseBindMatrices,AccessorType::Mat4);
  check(model.inverse_bind.size()==model.joints.size(),"inverse-bind count mismatch");
}
void load_animations(const Asset& asset,PlayerModel& model) {
  check(asset.animations.size()<=64,"too many player animations");
  for(const auto& animation:asset.animations) {
    PlayerAnimation clip;clip.name=std::string(animation.name);clip.looping=clip.name.ends_with("_loop");
    check(!clip.name.empty() && animation.channels.size()<=384,"invalid animation");
    for(const auto& channel:animation.channels) {
      check(channel.nodeIndex && *channel.nodeIndex<model.nodes.size() && channel.samplerIndex<animation.samplers.size(),"invalid animation channel");
      const auto& sampler=animation.samplers[channel.samplerIndex];
      check(sampler.interpolation==AnimationInterpolation::Linear || sampler.interpolation==AnimationInterpolation::Step,
            "unsupported animation interpolation");
      PlayerTrack track;track.node=static_cast<uint32_t>(*channel.nodeIndex);
      track.step=sampler.interpolation==AnimationInterpolation::Step;
      track.times=values<float>(asset,sampler.inputAccessor,AccessorType::Scalar);
      check(!track.times.empty() && track.times.size()<=10000,"invalid animation key count");
      for(size_t i=0;i<track.times.size();++i) check(std::isfinite(track.times[i]) && track.times[i]>=0 &&
        (i==0 || track.times[i]>track.times[i-1]),"invalid animation key times");
      if(channel.path==AnimationPath::Rotation) {
        track.path=PlayerTrackPath::Rotation;track.values=values<fvec4>(asset,sampler.outputAccessor,AccessorType::Vec4);
      } else {
        check(channel.path==AnimationPath::Translation || channel.path==AnimationPath::Scale,"unsupported animation channel path");
        track.path=channel.path==AnimationPath::Translation?PlayerTrackPath::Translation:PlayerTrackPath::Scale;
        for(auto value:values<fvec3>(asset,sampler.outputAccessor,AccessorType::Vec3)) track.values.emplace_back(value[0],value[1],value[2],0.f);
      }
      check(track.values.size()==track.times.size(),"animation value count mismatch");
      for(const auto& value:track.values) for(size_t k=0;k<4;++k) check(std::isfinite(value[k]),"nonfinite animation value");
      clip.duration=std::max(clip.duration,track.times.back());clip.tracks.push_back(std::move(track));
    }
    check(clip.duration>0,"empty animation duration");model.animations.push_back(std::move(clip));
  }
}
}
bool load_player_model(const std::filesystem::path& path,PlayerModel& output,std::string& error) {
  try {
    check(std::filesystem::file_size(path)<=16*1024*1024,"player asset exceeds size bound");
    auto data=fastgltf::GltfDataBuffer::FromPath(path);check(data.error()==fastgltf::Error::None,"cannot read player asset");
    fastgltf::Parser parser;
    auto loaded=parser.loadGltf(data.get(),path.parent_path(),fastgltf::Options::None);
    check(loaded.error()==fastgltf::Error::None,"cannot parse player glTF");
    const auto& asset=loaded.get();check(fastgltf::validate(asset)==fastgltf::Error::None,"invalid player glTF");
    check(asset.meshes.size()==1 && asset.skins.size()==1 && asset.images.empty(),"expected retained single-mesh untextured skinned player");
    PlayerModel result;load_rig(asset,result);load_vertices(asset,result);load_animations(asset,result);
    output=std::move(result);error.clear();return true;
  } catch(const std::exception& exception) {error=exception.what();return false;}
}
}
