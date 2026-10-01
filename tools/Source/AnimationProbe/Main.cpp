#include "Pose.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
using namespace octaryn::client::animation;
namespace {
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
void near(float actual,float expected) {require(std::isfinite(actual)&&std::abs(actual-expected)<1e-4f,"animation value mismatch");}
Pose sample(const Asset& asset,float time,std::int32_t clip=0) {Pose p;std::string error;if(!sample_pose(asset,clip,time,p,error))throw std::runtime_error(error);return p;}
Asset fixture() {
  Asset a;a.nodes.resize(3);a.nodes[0].rest.translation={3,0,0};a.nodes[1].parent=0;a.nodes[2].parent=0;
  a.nodes[1].weights={0};a.skins.push_back({{2},{identity}});
  Primitive p;p.node=1;p.skin=0;p.morph_count=1;SourceVertex v;v.position={1,0,0,1};v.weights[0]=1;
  p.vertices.push_back(v);MorphDelta delta;delta.position={2,0,0,0};p.morphs.push_back(delta);a.primitives.push_back(p);
  Channel c;c.node=2;c.components=3;c.path=Path::Translation;c.interpolation=Interpolation::Linear;c.times={0,2};c.values={0,0,0,0,4,0};
  Channel w;w.node=1;w.components=1;w.path=Path::Weights;w.interpolation=Interpolation::Linear;w.times={0,2};w.values={0,1};
  a.clips.push_back({"test",2,{c,w}});return a;
}
void run() {
  auto asset=fixture();auto p=sample(asset,1);near(p.world[2][12],3);near(p.world[2][13],2);
  near(sample(asset,-1).world[2][13],0);near(sample(asset,3).world[2][13],4);
  DeformationPose current,previous;std::string error;
  if(!deformation_pose(asset,asset.primitives[0],p,current,error))throw std::runtime_error(error);
  if(!deformation_pose(asset,asset.primitives[0],sample(asset,0),previous,error))throw std::runtime_error(error);
  std::vector<Vec3> vertices;Bounds now,before;
  if(!deform_positions(asset.primitives[0],current,vertices,now,error))throw std::runtime_error(error);near(vertices[0][0],2);near(vertices[0][1],2);
  if(!deform_positions(asset.primitives[0],previous,vertices,before,error))throw std::runtime_error(error);near(vertices[0][0],1);near(vertices[0][1],0);
  const auto bounds=union_bounds(now,before);near(bounds.minimum[0],1);near(bounds.maximum[1],2);
  asset.skins[0].inverse_bind[0][12]=.5f;
  if(!deformation_pose(asset,asset.primitives[0],p,current,error)||!deform_positions(asset.primitives[0],current,vertices,now,error))throw std::runtime_error(error);
  near(vertices[0][0],2.5f);asset.skins[0].inverse_bind[0]=identity;
  asset.clips[0].channels[0].interpolation=Interpolation::Step;near(sample(asset,1.9f).world[2][13],0);near(sample(asset,2).world[2][13],4);
  auto& channel=asset.clips[0].channels[0];channel.interpolation=Interpolation::CubicSpline;
  channel.values={0,0,0, 0,0,0, 0,2,0, 0,2,0, 0,4,0, 0,0,0};near(sample(asset,1).world[2][13],2);near(sample(asset,.5f).world[2][13],1);
  channel.path=Path::Rotation;channel.components=4;channel.interpolation=Interpolation::Linear;
  channel.values={0,0,0,1,0,0,1,0};const auto rotation=sample(asset,1);near(rotation.world[2][0],0);near(rotation.world[2][1],1);
  channel.values={0,0,0,1,0,0,0,-1};near(sample(asset,1).world[2][0],1);
  channel.interpolation=Interpolation::CubicSpline;channel.values={0,0,0,0,0,0,0,1,0,0,0,0, 0,0,0,0,0,0,1,0,0,0,0,0};
  near(sample(asset,1).world[2][1],1);
  asset.nodes[0].parent=2;Pose invalid;require(!sample_pose(asset,-1,0,invalid,error),"cycle accepted");
  asset.nodes[0].parent=-1;asset.nodes[1].rest.scale={0,1,1};
  require(!deformation_pose(asset,asset.primitives[0],sample(asset,0,-1),current,error),"singular mesh accepted");
  Matrix scale=identity;scale[0]=-2;scale[5]=3;scale[10]=4;scale[12]=7;Matrix inv;
  require(inverse(scale,inv),"matrix inversion failed");const auto product=multiply(scale,inv);for(int i=0;i<16;++i)near(product[i],identity[i]);
}
}
int main(int argc,char** argv) {
  try {
    run();
    if(argc>1) {
      Asset asset;std::string error;if(!load_asset(argv[1],asset,error))throw std::runtime_error(error);
      std::printf("animation_import nodes=%zu skins=%zu clips=%zu primitives=%zu\n",asset.nodes.size(),asset.skins.size(),asset.clips.size(),asset.primitives.size());
      for(std::size_t clip=0;clip<asset.clips.size();++clip) {
        auto pose=sample(asset,asset.clips[clip].duration*.5f,static_cast<std::int32_t>(clip));
        for(const auto& primitive:asset.primitives) {
          DeformationPose d;std::vector<Vec3> positions;Bounds bounds;
          if(!deformation_pose(asset,primitive,pose,d,error)||!deform_positions(primitive,d,positions,bounds,error))throw std::runtime_error(error);
          if(argc>2&&!std::strcmp(argv[2],"--fixture")) {near(positions.at(0)[0],2);near(positions.at(0)[1],2);near(pose.world.at(primitive.node)[12],3);}
        }
      }
    }
    std::puts("ANIMATION_PROBE PASS hierarchy inverse-bind STEP LINEAR CUBICSPLINE quaternion morph previous-bounds");return 0;
  } catch(const std::exception& e) {std::fprintf(stderr,"ANIMATION_PROBE FAIL %s\n",e.what());return 1;}
}
