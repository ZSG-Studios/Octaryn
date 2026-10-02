#include "Pose.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <fstream>
#include <iomanip>
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
      Asset asset;std::string error;LoadLimits limits;
      const bool clips_only=argc>2&&!std::strcmp(argv[2],"--clips-only");
      const bool source_skin=argc>2&&!std::strcmp(argv[2],"--skin-source");
      if(clips_only) {
        require(!load_asset(argv[1],asset,error),"scene default accepted meshless animation library");
        limits.require_geometry=false;
      }
      if(!load_asset(argv[1],asset,error,limits))throw std::runtime_error(error);
      std::printf("animation_import nodes=%zu skins=%zu clips=%zu primitives=%zu\n",asset.nodes.size(),asset.skins.size(),asset.clips.size(),asset.primitives.size());
      std::ofstream trace;
      if(source_skin) {
        require(argc==4&&!asset.skins.empty()&&!asset.primitives.empty(),"skin source probe needs real skinned asset and trace path");
        trace.open(argv[3]);require(bool(trace),"cannot create source skin pose trace");trace<<std::setprecision(9)<<"primitive,vertex,clip,x,y,z\n";
        const auto rest=sample(asset,0,-1);
        for(std::size_t p=0;p<asset.primitives.size();++p) {
          DeformationPose d;std::vector<Vec3> positions;Bounds bounds;
          require(asset.primitives[p].skin>=0,"source skin primitive is unskinned");
          if(!deformation_pose(asset,asset.primitives[p],rest,d,error)||!deform_positions(asset.primitives[p],d,positions,bounds,error))throw std::runtime_error(error);
          for(std::size_t v=0;v<positions.size();++v) {
            const auto& m=rest.world[asset.primitives[p].node];const auto& local=positions[v];
            trace<<p<<','<<v<<",-1";for(int c=0;c<3;++c)trace<<','<<(m[c]*local[0]+m[4+c]*local[1]+m[8+c]*local[2]+m[12+c]);trace<<'\n';
          }
        }
      }
      for(std::size_t clip=0;clip<asset.clips.size();++clip) {
        auto pose=sample(asset,asset.clips[clip].duration*.5f,static_cast<std::int32_t>(clip));
        if(clips_only) {
          require(asset.primitives.empty()&&!asset.clips[clip].channels.empty(),"invalid meshless clip library");
          const auto initial=sample(asset,0,static_cast<std::int32_t>(clip));std::size_t moving=0;
          for(std::size_t n=0;n<pose.world.size();++n) {
            bool changed=false;for(int c=0;c<16;++c)changed|=std::abs(pose.world[n][c]-initial.world[n][c])>1e-5f;
            moving+=changed;
          }
          require(moving>0,"authored animation clip has no changing source bone poses");
          std::printf("animation_clip_pose name=%s channels=%zu moving_nodes=%zu seconds=%.6f\n",asset.clips[clip].name.c_str(),asset.clips[clip].channels.size(),moving,asset.clips[clip].duration*.5f);
        }
        for(const auto& primitive:asset.primitives) {
          DeformationPose d;std::vector<Vec3> positions;Bounds bounds;
          if(!deformation_pose(asset,primitive,pose,d,error)||!deform_positions(primitive,d,positions,bounds,error))throw std::runtime_error(error);
          if(source_skin) {
            const auto initial=sample(asset,0,static_cast<std::int32_t>(clip));DeformationPose initial_pose;
            std::vector<Vec3> initial_positions;Bounds initial_bounds;
            if(!deformation_pose(asset,primitive,initial,initial_pose,error)||!deform_positions(primitive,initial_pose,initial_positions,initial_bounds,error))throw std::runtime_error(error);
            std::size_t moving=0;for(std::size_t v=0;v<positions.size();++v) {
              bool changed=false;for(int c=0;c<3;++c)changed|=std::abs(positions[v][c]-initial_positions[v][c])>1e-5f;moving+=changed;
              const auto& m=pose.world[primitive.node];const auto& local=positions[v];
              trace<<(&primitive-asset.primitives.data())<<','<<v<<','<<clip;for(int c=0;c<3;++c)trace<<','<<(m[c]*local[0]+m[4+c]*local[1]+m[8+c]*local[2]+m[12+c]);trace<<'\n';
            }
            require(moving>0,"original source skin clip has no changing deformed vertices");
            std::printf("animation_skin_pose clip=%zu primitive=%zu vertices=%zu joints=%zu moving_vertices=%zu\n",clip,std::size_t(&primitive-asset.primitives.data()),positions.size(),d.joints.size(),moving);
          }
          if(argc>2&&!std::strcmp(argv[2],"--fixture")) {near(positions.at(0)[0],2);near(positions.at(0)[1],2);near(pose.world.at(primitive.node)[12],3);}
        }
      }
    }
    std::puts("ANIMATION_PROBE PASS hierarchy inverse-bind STEP LINEAR CUBICSPLINE quaternion morph previous-bounds");return 0;
  } catch(const std::exception& e) {std::fprintf(stderr,"ANIMATION_PROBE FAIL %s\n",e.what());return 1;}
}
