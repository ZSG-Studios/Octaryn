#include "Pose.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
namespace octaryn::client::animation {
namespace {
void check(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
void normalize(std::vector<float>& values) {
  double length=0;for(float v:values)length+=double(v)*v;
  check(std::isfinite(length)&&length>1e-20,"invalid animation quaternion");
  for(float& v:values)v=static_cast<float>(v/std::sqrt(length));
}
std::vector<float> sample(const Channel& c,float time) {
  check(!c.times.empty()&&c.components>0,"empty animation channel");
  for(std::size_t i=0;i<c.times.size();++i)check(std::isfinite(c.times[i])&&c.times[i]>=0&&(i==0||c.times[i]>c.times[i-1]),"animation timestamps must strictly increase");
  const auto n=c.components;const bool cubic=c.interpolation==Interpolation::CubicSpline;
  check(c.values.size()==c.times.size()*n*(cubic?3:1),"animation output size mismatch");
  const auto read=[&](std::size_t key,std::size_t part,std::size_t component) {
    return c.values[(key*(cubic?3:1)+(cubic?part:0))*n+component];
  };
  const auto high=std::upper_bound(c.times.begin(),c.times.end(),time);
  const std::size_t a=high==c.times.begin()?0:static_cast<std::size_t>(high-c.times.begin()-1);
  const std::size_t b=std::min(a+1,c.times.size()-1);
  const float span=c.times[b]-c.times[a];
  const float t=span>0?std::clamp((time-c.times[a])/span,0.f,1.f):0;
  std::vector<float> result(n),first(n),last(n);
  for(std::size_t i=0;i<n;++i) {first[i]=read(a,1,i);last[i]=read(b,1,i);}
  if(c.path==Path::Rotation&&!cubic&&c.interpolation!=Interpolation::Step) {
    normalize(first);normalize(last);float dot=0;for(std::size_t i=0;i<n;++i)dot+=first[i]*last[i];
    if(dot<0) {dot=-dot;for(float& v:last)v=-v;}
    float x=1-t,y=t;
    if(dot<0.9995f) {const float angle=std::acos(std::clamp(dot,-1.f,1.f));x=std::sin((1-t)*angle)/std::sin(angle);y=std::sin(t*angle)/std::sin(angle);}
    for(std::size_t i=0;i<n;++i)result[i]=x*first[i]+y*last[i];
  } else for(std::size_t i=0;i<n;++i) {
    if(c.interpolation==Interpolation::Step)result[i]=first[i];
    else if(cubic) {const float t2=t*t,t3=t2*t;result[i]=(2*t3-3*t2+1)*first[i]+span*(t3-2*t2+t)*read(a,2,i)+(-2*t3+3*t2)*last[i]+span*(t3-t2)*read(b,0,i);}
    else result[i]=first[i]*(1-t)+last[i]*t;
  }
  if(c.path==Path::Rotation)normalize(result);
  for(float v:result)check(std::isfinite(v),"nonfinite sampled animation value");
  return result;
}
}
bool sample_pose(const Asset& asset,std::int32_t clip,float seconds,Pose& output,std::string& error) {
  try {
    check(std::isfinite(seconds),"nonfinite animation time");
    check(clip>=-1&&(clip<0||static_cast<std::size_t>(clip)<asset.clips.size()),"animation clip out of range");
    std::vector<Transform> local;local.reserve(asset.nodes.size());
    Pose result;result.world.resize(asset.nodes.size());result.weights.reserve(asset.nodes.size());
    for(const auto& node:asset.nodes) {local.push_back(node.rest);result.weights.push_back(node.weights);}
    if(clip>=0)for(const auto& channel:asset.clips[clip].channels) {
      check(channel.node<asset.nodes.size(),"animation node out of range");
      check(channel.path==Path::Weights||!asset.nodes[channel.node].has_matrix,"animation targets a matrix node");
      const auto values=sample(channel,seconds);auto& transform=local[channel.node];
      switch(channel.path) {
        case Path::Translation:check(values.size()==3,"invalid translation size");std::copy(values.begin(),values.end(),transform.translation.begin());break;
        case Path::Rotation:check(values.size()==4,"invalid rotation size");std::copy(values.begin(),values.end(),transform.rotation.begin());break;
        case Path::Scale:check(values.size()==3,"invalid scale size");std::copy(values.begin(),values.end(),transform.scale.begin());break;
        case Path::Weights:check(values.size()==result.weights[channel.node].size(),"invalid morph weight size");result.weights[channel.node]=values;break;
      }
    }
    std::vector<std::uint8_t> states(asset.nodes.size());
    std::function<void(std::size_t,std::size_t)> visit=[&](std::size_t i,std::size_t depth) {
      check(depth<1024,"animation hierarchy depth limit");check(states[i]!=1,"animation hierarchy cycle");if(states[i]==2)return;
      states[i]=1;const auto& node=asset.nodes[i];Matrix world=node.has_matrix?node.matrix:compose(local[i]);
      if(node.parent>=0) {check(static_cast<std::size_t>(node.parent)<asset.nodes.size(),"animation parent out of range");visit(node.parent,depth+1);world=multiply(result.world[node.parent],world);}
      for(float v:world)check(std::isfinite(v),"nonfinite animation world matrix");
      result.world[i]=world;states[i]=2;
    };
    for(std::size_t i=0;i<asset.nodes.size();++i)visit(i,0);
    output=std::move(result);error.clear();return true;
  } catch(const std::exception& e) {error=e.what();return false;}
}
bool deformation_pose(const Asset& asset,const Primitive& primitive,const Pose& pose,DeformationPose& output,std::string& error) {
  try {
    check(primitive.node<pose.world.size()&&primitive.node<pose.weights.size(),"deformation node out of range");
    DeformationPose result;result.world=pose.world[primitive.node];result.weights=pose.weights[primitive.node];
    check(result.weights.size()==primitive.morph_count,"deformation morph count mismatch");
    if(primitive.skin>=0) {
      check(static_cast<std::size_t>(primitive.skin)<asset.skins.size(),"deformation skin out of range");const auto& skin=asset.skins[primitive.skin];
      check(skin.joints.size()==skin.inverse_bind.size(),"inverse bind count mismatch");
      Matrix inverse_mesh;check(inverse(result.world,inverse_mesh),"singular skinned mesh transform");
      for(std::size_t i=0;i<skin.joints.size();++i) {
        check(skin.joints[i]<pose.world.size(),"deformation joint out of range");
        const Matrix m=multiply(multiply(inverse_mesh,pose.world[skin.joints[i]]),skin.inverse_bind[i]);
        MatrixRows rows{};for(int r=0;r<4;++r)for(int c=0;c<4;++c)rows.rows[r][c]=m[c*4+r];result.joints.push_back(rows);
      }
    }
    output=std::move(result);error.clear();return true;
  } catch(const std::exception& e) {error=e.what();return false;}
}
bool deform_positions(const Primitive& primitive,const DeformationPose& pose,std::vector<Vec3>& output,Bounds& bounds,std::string& error) {
  try {
    check(pose.weights.size()==primitive.morph_count,"morph weight count mismatch");
    check(primitive.morphs.size()==primitive.vertices.size()*primitive.morph_count,"morph delta count mismatch");
    std::vector<Vec3> positions;positions.reserve(primitive.vertices.size());Bounds result;
    for(std::size_t i=0;i<primitive.vertices.size();++i) {
      const auto& vertex=primitive.vertices[i];Vec3 p{vertex.position[0],vertex.position[1],vertex.position[2]};
      for(std::size_t target=0;target<primitive.morph_count;++target)for(int c=0;c<3;++c)p[c]+=primitive.morphs[target*primitive.vertices.size()+i].position[c]*pose.weights[target];
      if(primitive.skin>=0) {
        Vec3 skinned{};float total=0;
        for(std::size_t j=0;j<8;++j)if(vertex.weights[j]>0) {
          check(vertex.joints[j]<pose.joints.size(),"vertex joint out of range");const auto& m=pose.joints[vertex.joints[j]];
          for(int c=0;c<3;++c)skinned[c]+=vertex.weights[j]*(m.rows[c][0]*p[0]+m.rows[c][1]*p[1]+m.rows[c][2]*p[2]+m.rows[c][3]);total+=vertex.weights[j];
        }
        check(total>0,"vertex has no skin weight");for(int c=0;c<3;++c)p[c]=skinned[c]/total;
      }
      for(float v:p)check(std::isfinite(v),"nonfinite deformed position");
      if(!result.valid) {result.minimum=p;result.maximum=p;result.valid=true;}
      else for(int c=0;c<3;++c) {result.minimum[c]=std::min(result.minimum[c],p[c]);result.maximum[c]=std::max(result.maximum[c],p[c]);}
      positions.push_back(p);
    }
    output=std::move(positions);bounds=result;error.clear();return true;
  } catch(const std::exception& e) {error=e.what();return false;}
}
}
