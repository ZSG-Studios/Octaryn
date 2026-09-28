#include "CookedAsset.h"
#include "Pose.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <set>
#include <stdexcept>

namespace octaryn::client::animation {
bool validate_animation_correspondence(const CookedAsset&,std::string& error);
namespace {
void require(bool value,const std::string& error) {if(!value)throw std::runtime_error(error);}
template<class Range> void finite(const Range& values) {for(float value:values)require(std::isfinite(value),"nonfinite cooked animation value");}
void materials(const CookedAsset& cooked) {
  std::uint64_t image_bytes=0;
  for(const auto& image:cooked.images) {
    image_bytes+=image.bytes.size();require(!image.bytes.empty() && image.bytes.size()<=86ull*1024*1024 &&
        image_bytes<=512ull*1024*1024,"cooked animation image budget invalid");
    require(image.mime_type=="image/png" || image.mime_type=="image/jpeg","cooked animation image MIME invalid");
  }
  for(const auto& m:cooked.materials) {
    finite(m.base_color);finite(m.emissive);finite(std::array{m.metallic,m.roughness,m.alpha_cutoff,m.normal_scale,m.occlusion_strength});
    require(unsigned(m.alpha_mode)<=2 && m.texture==m.textures[0].image,"cooked animation material invalid");
    for(const auto& t:m.textures) {
      require(t.image>=-1 && (t.image<0 || size_t(t.image)<cooked.images.size()) && t.texcoord<=1,"cooked animation texture reference invalid");
      finite(t.transform);
      for(auto wrap:{t.wrap_s,t.wrap_t})require(wrap==33071 || wrap==33648 || wrap==10497,"cooked texture wrapping invalid");
      require(t.mag_filter==9728 || t.mag_filter==9729,"cooked texture magnification filter invalid");
      require(t.min_filter==9728 || t.min_filter==9729 || (t.min_filter>=9984 && t.min_filter<=9987),"cooked texture minification filter invalid");
    }
  }
}
}
bool validate_cooked_animation(const CookedAsset& cooked,std::string& error) {
  try {
    using namespace rendering::virtual_geometry;const auto& a=cooked.animation;LoadLimits limits;
    require(!a.nodes.empty() && a.nodes.size()<=limits.nodes && a.skins.size()<=limits.nodes &&
        !a.primitives.empty() && a.primitives.size()<=limits.primitives && cooked.images.size()<=1024,"cooked animation counts invalid");
    require(cooked.materials.size()==a.primitives.size(),"cooked animation material table mismatch");materials(cooked);
    for(const auto& n:a.nodes) {
      require(n.parent>=-1 && (n.parent<0 || size_t(n.parent)<a.nodes.size()) && n.weights.size()<=limits.morph_targets,"cooked animation node invalid");
      finite(n.rest.translation);finite(n.rest.rotation);finite(n.rest.scale);finite(n.matrix);finite(n.weights);
      double norm=0;for(float value:n.rest.rotation)norm+=double(value)*value;
      require(norm>1e-20,"cooked rest quaternion invalid");
    }
    for(const auto& s:a.skins) {
      require(!s.joints.empty() && s.joints.size()<=limits.joints && s.joints.size()==s.inverse_bind.size(),"cooked skeleton invalid");
      for(auto joint:s.joints)require(joint<a.nodes.size(),"cooked skeleton joint invalid");
      for(const auto& matrix:s.inverse_bind)finite(matrix);
    }
    size_t vertices=0,morphs=0,keys=0;
    for(const auto& p:a.primitives) {
      vertices+=p.vertices.size();morphs+=p.morphs.size();
      require(p.node<a.nodes.size() && p.skin>=-1 && (p.skin<0 || size_t(p.skin)<a.skins.size()) && p.material>=-1,"cooked primitive owner invalid");
      require(!p.vertices.empty() && vertices<=limits.vertices && !p.indices.empty() && p.indices.size()%3==0 &&
          p.indices.size()<=limits.vertices*3 && p.morph_count<=limits.morph_targets && morphs<=limits.morph_deltas &&
          p.morphs.size()==p.vertices.size()*p.morph_count && p.morph_count==a.nodes[p.node].weights.size(),"cooked primitive payload counts invalid");
      for(auto index:p.indices)require(index<p.vertices.size(),"cooked primitive index invalid");
      for(const auto& v:p.vertices) {
        finite(v.position);finite(v.normal);finite(v.tangent);finite(v.uv);finite(v.color);finite(v.weights);double sum=0;
        for(unsigned i=0;i<8;++i) {
          require(v.weights[i]>=0,"negative cooked skin weight");sum+=v.weights[i];
          if(p.skin>=0)require(v.joints[i]<a.skins[p.skin].joints.size(),"cooked vertex joint invalid");
        }
        if(p.skin>=0)require(std::abs(sum-1)<1e-4,"cooked skin weights are not normalized");
      }
      for(const auto& delta:p.morphs) {finite(delta.position);finite(delta.normal);finite(delta.tangent);}
    }
    for(const auto& c:a.clips) {
      require(c.name.size()<=4096 && std::isfinite(c.duration) && c.duration>=0,"cooked clip invalid");
      std::set<std::pair<unsigned,Path>> targets;float duration=0;
      for(const auto& channel:c.channels) {
        require(channel.node<a.nodes.size() && unsigned(channel.path)<=unsigned(Path::Weights) &&
            unsigned(channel.interpolation)<=unsigned(Interpolation::CubicSpline),"cooked animation channel invalid");
        require(targets.insert({channel.node,channel.path}).second,"cooked duplicate animation channel");
        const auto count=channel.path==Path::Weights?a.nodes[channel.node].weights.size():channel.path==Path::Rotation?4u:3u;
        const size_t factor=channel.interpolation==Interpolation::CubicSpline?3:1;
        keys+=channel.times.size();require(count>0 && channel.components==count && keys<=limits.keys &&
            channel.times.size()>=(factor==3?2u:1u) && channel.values.size()==channel.times.size()*count*factor,"cooked animation sampler counts invalid");
        finite(channel.values);
        for(size_t i=0;i<channel.times.size();++i) {
          require(std::isfinite(channel.times[i]) && channel.times[i]>=0 && (i==0 || channel.times[i]>channel.times[i-1]),"cooked animation timestamps invalid");
          if(channel.path==Path::Rotation) {
            double norm=0;for(unsigned k=0;k<4;++k) {const auto v=channel.values[(i*factor+(factor==3?1:0))*4+k];norm+=double(v)*v;}
            require(norm>1e-20,"cooked animation quaternion invalid");
          }
        }
        duration=std::max(duration,channel.times.back());
      }
      require(c.duration==duration,"cooked clip duration mismatch");
    }
    Pose rest;require(sample_pose(a,-1,0,rest,error),error);
    for(const auto& p:a.primitives) {DeformationPose pose;require(deformation_pose(a,p,rest,pose,error),error);}
    require(cooked.geometry.space==GeometrySpace::Object,"animated geometry must retain object coordinates");
    require(validate_geometry(cooked.geometry,error),error);
    require(cooked.geometry.groups.size()==a.primitives.size() && cooked.geometry.roots.size()==a.primitives.size() &&
        cooked.geometry.material_count==a.primitives.size() && cooked.cluster_sources.size()==cooked.geometry.clusters.size(),"cooked animation geometry table mismatch");
    size_t expected=0;std::vector<std::uint64_t> triangles(a.primitives.size());
    for(size_t c=0;c<cooked.geometry.clusters.size();++c) {
      const auto& cluster=cooked.geometry.clusters[c];const auto& source=cooked.cluster_sources[c];
      require(source.primitive<a.primitives.size() && cluster.material==source.primitive && cluster.group==source.primitive &&
          cluster.refined_group==invalid_id && source.first_vertex==expected,"cooked animated cluster correspondence invalid");
      const auto& primitive=a.primitives[source.primitive];triangles[source.primitive]+=cluster.triangle_count;
      const auto& material=cooked.materials[source.primitive];
      require(cluster.flags==(unsigned(material.alpha_mode)|(material.double_sided?256u:0u)),"cooked animated cluster material flags differ");
      require(cluster.vertex_count<=cooked.source_vertices.size()-std::min(expected,cooked.source_vertices.size()),"cooked animated vertex remap truncated");
      for(unsigned v=0;v<cluster.vertex_count;++v)require(cooked.source_vertices[expected+v]<primitive.vertices.size(),"cooked animated source vertex invalid");
      expected+=cluster.vertex_count;
    }
    require(expected==cooked.source_vertices.size(),"cooked animated source vertex remap excess");
    for(size_t i=0;i<a.primitives.size();++i)require(triangles[i]==a.primitives[i].indices.size()/3 &&
        cooked.geometry.groups[i].depth==0 && cooked.geometry.groups[i].simplified.error==FLT_MAX,"cooked animated geometry simplified without deformation bound");
    require(validate_animation_correspondence(cooked,error),error);
    error.clear();return true;
  }catch(const std::exception& failure) {error=failure.what();return false;}
}
}
