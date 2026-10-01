#include "CloudOcclusion.cpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

int main() {
  unsigned checked=0,rejected=0;
  for(float near_plane:{.01f,.1f,1.f})for(float far_plane:{1000.f,8192.f,30000.f})
    for(float cosine:{.001f,.03f,.25f,.7f,1.f})for(float entry:{.1f,10.f,200.f,1000.f,14000.f})
      for(float ratio:{.01f,.5f,.99f,1.f,1.000001f,2.f}) {
        const float z=far_plane/(far_plane-near_plane),w=near_plane*z;
        const float opaque=entry*cosine*ratio;
        const bool skip=cloud_occluded_before_entry_0({0,0,opaque,opaque},entry,cosine,{z,w});
        ++checked;if(!skip)continue;++rejected;
        // Independent hardware-depth oracle: every possible first cloud sample
        // after entry must lose the depth test. Include all reference Bayer phases.
        const double scene_z=std::clamp(double(z)-double(w)/double(opaque),0.0,1.0);
        for(unsigned phase=0;phase<16;++phase)for(unsigned step=0;step<44;++step) {
          const double t=double(entry)+(double(step)+(phase+.5)/16)*240/44;
          const double sample_z=std::clamp(double(z)-double(w)/(t*cosine),0.0,1.0);
          if(!(scene_z>=0 && scene_z<1 && sample_z>scene_z)) {
            std::printf("cloud_occlusion_oracle_failed near=%g far=%g cosine=%g entry=%g opaque=%g scene_z=%.12g sample_z=%.12g\n",
                near_plane,far_plane,cosine,entry,opaque,scene_z,sample_z);return 1;
          }
        }
      }
  const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
  if(!rejected || !cloud_occluded_before_entry_0({0,0,10,10},200,1,{1.0000122f,.10000122f}))return 2;
  for(float invalid:{0.f,-1.f,nan,inf}) {
    if(cloud_occluded_before_entry_0({0,0,10,invalid},200,1,{1.0000122f,.10000122f}))return 3;
    if(cloud_occluded_before_entry_0({0,0,10,10},invalid,1,{1.0000122f,.10000122f}))return 4;
    if(cloud_occluded_before_entry_0({0,0,10,10},200,invalid,{1.0000122f,.10000122f}))return 5;
  }
  if(cloud_occluded_before_entry_0({nan,0,10,10},200,1,{1.0000122f,.10000122f}) ||
      cloud_occluded_before_entry_0({0,0,10,10},200,1,{nan,.1f}) ||
      cloud_occluded_before_entry_0({0,0,10,10},200,1,{1.f,-.1f}))return 6;
  std::printf("cloud_occlusion_production_cpu passed=1 cases=%u rejected=%u reference_sample_depths=%u invalid_depth_preserved=1\n",
      checked,rejected,rejected*16*44);
}
