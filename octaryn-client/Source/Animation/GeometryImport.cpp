#include "Import.h"
#include <algorithm>
#include <functional>
namespace octaryn::client::animation::importing {
namespace {
using namespace fastgltf;
std::size_t attribute(const fastgltf::Primitive& p,const char* name) {
  const auto found=p.findAttribute(name);check(found!=p.attributes.end(),"missing animated mesh attribute");return found->accessorIndex;
}
template<class T> std::vector<T> optional(const fastgltf::Asset& asset,const fastgltf::Primitive& p,const char* name,AccessorType type,std::size_t count) {
  const auto found=p.findAttribute(name);if(found==p.attributes.end())return {};
  auto data=values<T>(asset,found->accessorIndex,type,count);check(data.size()==count,"animated attribute count mismatch");return data;
}
void add(const fastgltf::Asset& source,const fastgltf::Primitive& p,std::uint32_t node,animation::Asset& result,const LoadLimits& limits,std::size_t& vertex_total,std::size_t& morph_total) {
  check(result.primitives.size()<limits.primitives,"animated primitive limit exceeded");
  check(p.type==PrimitiveType::Triangles,"animated importer requires triangle primitives");
  animation::Primitive dest;dest.node=node;const auto& source_node=source.nodes[node];
  if(source_node.skinIndex) {check(*source_node.skinIndex<result.skins.size(),"mesh skin out of range");dest.skin=static_cast<std::int32_t>(*source_node.skinIndex);}
  if(p.materialIndex)dest.material=static_cast<std::int32_t>(*p.materialIndex);
  const auto positions=values<math::fvec3>(source,attribute(p,"POSITION"),AccessorType::Vec3,limits.vertices-vertex_total);
  const auto count=positions.size();check(count>0,"empty animated primitive");vertex_total+=count;dest.vertices.resize(count);
  const auto normals=optional<math::fvec3>(source,p,"NORMAL",AccessorType::Vec3,count);
  check(!normals.empty(),"animated importer requires authored normals");
  const auto tangents=optional<math::fvec4>(source,p,"TANGENT",AccessorType::Vec4,count);
  const auto uv=optional<math::fvec2>(source,p,"TEXCOORD_0",AccessorType::Vec2,count);
  const auto uv1=optional<math::fvec2>(source,p,"TEXCOORD_1",AccessorType::Vec2,count);
  for(std::size_t i=0;i<count;++i) {
    auto& v=dest.vertices[i];const auto pos=vector<3>(positions[i]);std::copy(pos.begin(),pos.end(),v.position.begin());v.position[3]=1;
    if(!normals.empty()) {const auto n=vector<3>(normals[i]);std::copy(n.begin(),n.end(),v.normal.begin());}
    if(!tangents.empty())v.tangent=vector<4>(tangents[i]);
    if(!uv.empty()) {const auto t=vector<2>(uv[i]);v.uv[0]=t[0];v.uv[1]=t[1];}
    if(!uv1.empty()) {const auto t=vector<2>(uv1[i]);v.uv[2]=t[0];v.uv[3]=t[1];}
  }
  if(const auto color=p.findAttribute("COLOR_0");color!=p.attributes.end()) {
    if(source.accessors[color->accessorIndex].type==AccessorType::Vec3) {
      const auto colors=values<math::fvec3>(source,color->accessorIndex,AccessorType::Vec3,count);check(colors.size()==count,"color count mismatch");
      for(std::size_t i=0;i<count;++i) {auto c=vector<3>(colors[i]);std::copy(c.begin(),c.end(),dest.vertices[i].color.begin());}
    } else {
      const auto colors=values<math::fvec4>(source,color->accessorIndex,AccessorType::Vec4,count);check(colors.size()==count,"color count mismatch");
      for(std::size_t i=0;i<count;++i)dest.vertices[i].color=vector<4>(colors[i]);
    }
  }
  if(dest.skin>=0) {
    check(p.findAttribute("JOINTS_0")!=p.attributes.end()&&p.findAttribute("WEIGHTS_0")!=p.attributes.end(),"skinned primitive lacks joint weights");
    for(int set=0;set<2;++set) {
      const auto joints=optional<math::uvec4>(source,p,set?"JOINTS_1":"JOINTS_0",AccessorType::Vec4,count);
      const auto weights=optional<math::fvec4>(source,p,set?"WEIGHTS_1":"WEIGHTS_0",AccessorType::Vec4,count);
      check(joints.empty()==weights.empty(),"joint/weight attribute mismatch");
      if(joints.empty())continue;
      for(std::size_t i=0;i<count;++i)for(int c=0;c<4;++c) {
        const float w=weights[i][c];check(std::isfinite(w)&&w>=0,"invalid skin weight");
        check(joints[i][c]<result.skins[dest.skin].joints.size(),"vertex joint index out of range");
        dest.vertices[i].joints[set*4+c]=joints[i][c];dest.vertices[i].weights[set*4+c]=w;
      }
    }
    check(p.findAttribute("JOINTS_2")==p.attributes.end(),"more than eight joint influences unsupported");
    for(auto& vertex:dest.vertices) {double sum=0;for(float w:vertex.weights)sum+=w;check(sum>0,"zero total skin weight");for(float& w:vertex.weights)w=static_cast<float>(w/sum);}
  }
  if(p.indicesAccessor)dest.indices=values<std::uint32_t>(source,*p.indicesAccessor,AccessorType::Scalar,limits.vertices*3);
  else {dest.indices.resize(count);for(std::size_t i=0;i<count;++i)dest.indices[i]=static_cast<std::uint32_t>(i);}
  check(dest.indices.size()%3==0,"animated indices are not triangles");for(auto index:dest.indices)check(index<count,"animated vertex index out of range");
  check(p.targets.size()==result.nodes[node].weights.size(),"primitive morph target count differs from mesh");
  check(p.targets.size()<=limits.morph_targets&&(p.targets.empty()||count<=(limits.morph_deltas-morph_total)/p.targets.size()),"morph delta limit exceeded");
  dest.morph_count=static_cast<std::uint32_t>(p.targets.size());dest.morphs.resize(count*p.targets.size());morph_total+=dest.morphs.size();
  for(std::size_t t=0;t<p.targets.size();++t)for(const auto& field:p.targets[t]) {
    check(field.name=="POSITION"||field.name=="NORMAL"||field.name=="TANGENT","unsupported morph attribute");
    const auto data=values<math::fvec3>(source,field.accessorIndex,AccessorType::Vec3,count);check(data.size()==count,"morph attribute count mismatch");
    for(std::size_t i=0;i<count;++i) {
      auto& d=dest.morphs[t*count+i];auto& output=field.name=="POSITION"?d.position:field.name=="NORMAL"?d.normal:d.tangent;
      const auto value=vector<3>(data[i]);std::copy(value.begin(),value.end(),output.begin());
    }
  }
  result.primitives.push_back(std::move(dest));
}
}
void geometry(const fastgltf::Asset& source,animation::Asset& result,const LoadLimits& limits) {
  check(!source.scenes.empty(),"animated glTF has no scene");const auto scene=source.defaultScene.value_or(0);check(scene<source.scenes.size(),"animated scene out of range");
  std::size_t vertex_total=0,morph_total=0;std::vector<bool> seen(source.nodes.size());
  std::function<void(std::size_t,std::size_t)> walk=[&](std::size_t i,std::size_t depth) {
    check(i<source.nodes.size()&&depth<1024,"animated scene hierarchy limit");check(!seen[i],"repeated or cyclic scene node");seen[i]=true;
    const auto& node=source.nodes[i];if(node.meshIndex)for(const auto& p:source.meshes[*node.meshIndex].primitives)add(source,p,static_cast<std::uint32_t>(i),result,limits,vertex_total,morph_total);
    for(auto child:node.children)walk(child,depth+1);
  };
  for(auto root:source.scenes[scene].nodeIndices)walk(root,0);
  check(!limits.require_geometry||!result.primitives.empty(),"animated glTF has no triangle meshes");
}
}
