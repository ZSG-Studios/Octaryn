#include "RayAdmission.h"
#include <cstdio>
#include <stdexcept>
using namespace octaryn::client::rendering::virtual_geometry;
namespace {
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
}
int main() {
  try {
    GeometryAsset asset;GeometryCluster cluster;
    cluster.bounds={{0,0,10},2,.02f};asset.clusters.push_back(cluster);
    cluster.bounds={{0,0,-6},1,.03f};asset.clusters.push_back(cluster);
    cluster.bounds={{0,0,0},4,0};asset.clusters.push_back(cluster);
    const std::uint32_t complete[]{0,1,2};SelectionView view{{0,0,0},800,1};
    const auto error=ray_cut_error(asset,complete,view);
    require(std::abs(error-4.8f)<.0001f,"offscreen cluster error omitted from complete ray cut");
    const std::uint32_t exact[]{2};
    require(ray_cut_error(asset,exact,view)==0,"full detail leaf error changed near camera");
    GeometryTransform transform;transform.world={-2,0,0,0,0,2,0,0,0,0,2,0};transform.scale=2;
    const GeometryTransform transforms[]{transform};
    const auto transformed=ray_cut_error(asset,complete,view,transforms);
    require(std::isfinite(transformed) && transformed>=error,
        "mirrored uniform instance projection underestimates world ray error");
    RayAdmission admission;admission.reject(view,error);
    // Repeatedly arriving/refining raster pages and render jitter do not
    // alter the immutable ray snapshot or permit another failed allocation.
    for(unsigned frame=0;frame<10000;++frame) {
      SelectionView jitter=view;jitter.eye[0]=(frame%2?1:-1)*.0001f;
      require(!admission.allows(jitter,ray_cut_error(asset,complete,jitter)),"stationary streaming retried a rejected ray build");
    }
    SelectionView moved=view;moved.eye[0]=1;
    require(admission.allows(moved,ray_cut_error(asset,complete,moved)),"camera travel did not reopen admission");
    SelectionView zoom=view;zoom.focal_pixels=1000;
    require(admission.allows(zoom,ray_cut_error(asset,complete,zoom)),"projection change did not reopen admission");
    require(admission.allows(view,error*1.3f),"changed published error did not reopen admission");
    admission.clear();require(admission.allows(view,error),"successful publication retained admission block");
    std::puts("ray_admission=passed offscreen_error=4.8 stationary_frames=10000 camera_zoom_quality=tracked exact_leaf_error=0");return 0;
  } catch(const std::exception& failure) {std::fprintf(stderr,"ray_admission=failed reason=%s\n",failure.what());return 1;}
}
