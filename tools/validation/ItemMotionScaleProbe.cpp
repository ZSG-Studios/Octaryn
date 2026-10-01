#include "ItemMotion.h"
#include "MeshCollisionWorld.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

using namespace octaryn;
int main() {
  const float positions[]{-200,0,-200, -200,0,200, 200,0,200, 200,0,-200};
  const std::uint32_t indices[]{0,1,2,0,2,3};
  const character_motion::MeshCollision mesh{positions,12,indices,6};
  if(!character_motion::acquire_mesh_world(mesh))return 1;
  const auto params=item_motion::default_item_params();
  for(const auto count:{100,1000,10000}) for(int run=0;run<3;++run) {
    std::vector<item_motion::ItemState> items(count);
    std::vector<double> times;
    for(int frame=0;frame<200;++frame) {
      // Reset outside the measured phase so every sample contains the same
      // real Box3D contact-query work, without sleeping shrinking the load.
      for(int i=0;i<count;++i) {
        auto& item=items[i];item={};
        item.x=float(i%100)*2-100;item.z=float(i/100)*2-100;
        item.y=.3f;item.velocity_y=-2;item.velocity_x=1;
      }
      const auto started=std::chrono::steady_clock::now();
      for(auto& item:items)item_motion::step_item_on_mesh(item,1.f/60.f,params,mesh);
      const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
      if(frame>=20)times.push_back(ms);
      for(const auto& item:items)
        if(!std::isfinite(item.y)||item.y<0)throw std::runtime_error("Item contact tunneled through floor");
    }
    std::sort(times.begin(),times.end());
    std::printf("item_motion_scale workload=box3d_two_triangle_contact count=%d run=%d p50_ms=%.6f p95_ms=%.6f p99_ms=%.6f worst_ms=%.6f\n",
        count,run+1,times[90],times[171],times[178],times.back());
  }
  character_motion::release_mesh_collision(mesh);
  std::puts("item_motion_scale=passed");
}
