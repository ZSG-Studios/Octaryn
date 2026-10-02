#include "Import.h"
#include "Pose.h"
#include <algorithm>
namespace octaryn::client::animation {
namespace {
using namespace importing;
void nodes(const fastgltf::Asset& source,Asset& result,const LoadLimits& limits) {
  check(source.nodes.size()<=limits.nodes,"animation node limit exceeded");result.nodes.resize(source.nodes.size());
  for(std::size_t i=0;i<source.nodes.size();++i) {
    const auto& node=source.nodes[i];auto& dest=result.nodes[i];
    if(const auto* trs=std::get_if<fastgltf::TRS>(&node.transform)) {
      dest.rest.translation=vector<3>(trs->translation);dest.rest.rotation=vector<4>(trs->rotation);dest.rest.scale=vector<3>(trs->scale);
      double norm=0;for(float v:dest.rest.rotation)norm+=double(v)*v;check(norm>1e-20,"invalid rest quaternion");
      for(float& v:dest.rest.rotation)v=static_cast<float>(v/std::sqrt(norm));
    } else {
      dest.has_matrix=true;const auto& matrix=std::get<fastgltf::math::fmat4x4>(node.transform);
      for(int c=0;c<4;++c)for(int r=0;r<4;++r) {dest.matrix[c*4+r]=matrix[c][r];check(std::isfinite(dest.matrix[c*4+r]),"nonfinite node matrix");}
    }
    if(node.meshIndex) {
      check(*node.meshIndex<source.meshes.size(),"animation mesh index out of range");const auto& mesh=source.meshes[*node.meshIndex];
      const auto count=mesh.primitives.empty()?0:mesh.primitives.front().targets.size();check(count<=limits.morph_targets,"morph target limit exceeded");
      dest.weights.assign(count,0);
      if(!mesh.weights.empty()) {check(mesh.weights.size()==count,"mesh morph weight count mismatch");std::copy(mesh.weights.begin(),mesh.weights.end(),dest.weights.begin());}
      if(!node.weights.empty()) {check(node.weights.size()==count,"node morph weight count mismatch");std::copy(node.weights.begin(),node.weights.end(),dest.weights.begin());}
      for(float v:dest.weights)check(std::isfinite(v),"nonfinite morph rest weight");
    }
    for(const auto child:node.children) {
      check(child<result.nodes.size()&&child!=i,"invalid child node");check(result.nodes[child].parent<0,"node has multiple parents");result.nodes[child].parent=static_cast<std::int32_t>(i);
    }
  }
}
void skins(const fastgltf::Asset& source,Asset& result,const LoadLimits& limits) {
  check(source.skins.size()<=limits.nodes,"animation skin limit exceeded");
  for(const auto& skin:source.skins) {
    check(!skin.joints.empty()&&skin.joints.size()<=limits.joints,"animation joint count limit");Skin dest;
    for(auto index:skin.joints) {check(index<source.nodes.size(),"skin joint out of range");dest.joints.push_back(static_cast<std::uint32_t>(index));}
    dest.inverse_bind.assign(skin.joints.size(),identity);
    if(skin.inverseBindMatrices) {
      const auto matrices=values<fastgltf::math::fmat4x4>(source,*skin.inverseBindMatrices,fastgltf::AccessorType::Mat4,limits.joints);
      check(matrices.size()==skin.joints.size(),"inverse bind matrix count mismatch");
      for(std::size_t i=0;i<matrices.size();++i)for(int c=0;c<4;++c)for(int r=0;r<4;++r) {
        const float value=matrices[i][c][r];check(std::isfinite(value),"nonfinite inverse bind matrix");dest.inverse_bind[i][c*4+r]=value;
      }
    }
    result.skins.push_back(std::move(dest));
  }
}
}
bool load_asset(const std::filesystem::path& path,Asset& output,std::string& error,const LoadLimits& limits) {
  try {
    importing::check(std::filesystem::file_size(path)<=limits.source_bytes,"animated glTF source exceeds byte limit");
    auto data=fastgltf::MappedGltfFile::FromPath(path);importing::check(data.error()==fastgltf::Error::None,"cannot map animated glTF");
    fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_transform | fastgltf::Extensions::KHR_materials_emissive_strength | fastgltf::Extensions::KHR_materials_unlit);
    auto loaded=parser.loadGltf(data.get(),path.parent_path(),fastgltf::Options::LoadExternalBuffers);
    importing::check(loaded.error()==fastgltf::Error::None,"cannot parse animated glTF");
    const auto& source=loaded.get();importing::check(fastgltf::validate(source)==fastgltf::Error::None,"invalid animated glTF");
    std::uint64_t bytes=0;for(const auto& buffer:source.buffers) {
      importing::check(buffer.byteLength<=limits.source_bytes-bytes,"animated glTF buffers exceed byte limit");bytes+=buffer.byteLength;
    }
    Asset result;nodes(source,result,limits);skins(source,result,limits);importing::geometry(source,result,limits);importing::clips(source,result,limits);
    Pose pose;if(!sample_pose(result,-1,0,pose,error))throw std::runtime_error(error);
    output=std::move(result);error.clear();return true;
  } catch(const std::exception& e) {error=e.what();return false;}
}
}
