#include "ShadowReprojection.cpp"
#include <array>
#include <cstdio>

int main() {
  using V=Vector<float,3>;
  const std::array<V,6> normals={V{0,0,1},V{0,0,-1},V{1,0,0},V{-1,0,0},V{0,1,0},V{0,-1,0}};
  unsigned cases=0;
  for(unsigned voxel=0;voxel<normals.size();++voxel) {
    const V normal=normals[voxel];
    const V tangent=voxel<2?V{1,0,0}:V{0,0,1};
    for(float offset:{0.f,128.f,1024.f})for(float depth:{1.f,10.f,100.f,1000.f}) {
      const V eye{offset,-offset,offset};
      const V ray=normal+tangent*V{.2f,.2f,.2f};
      const V world=eye+normal*V{depth,depth,depth}+tangent*V{.1f,.1f,.1f};
      const V old=eye+ray*V{depth,depth,depth};
      if(!shadow_history_position_0(old,world,voxel,eye,ray,.025f))return 1;
      if(shadow_history_position_0(old+normal*V{.5f,.5f,.5f},world,voxel,eye,ray,.025f))return 2;
      cases+=2;
    }
  }
  if(shadow_history_position_0(V{0,0,1},V{0,0,1},0,V{0,0,0},V{1,0,0},.025f))return 3;
  if(shadow_history_position_0(V{0,0,-1},V{0,0,-1},0,V{0,0,0},V{0,0,1},.025f))return 4;
  if(!shadow_history_position_0(V{1,2,3},V{1,2,3},256,V{0,0,0},V{1,0,0},.025f))return 5;
  std::printf("shadow_reprojection_production_cpu passed=1 plane_cases=%u guards=3\n",cases);
}
