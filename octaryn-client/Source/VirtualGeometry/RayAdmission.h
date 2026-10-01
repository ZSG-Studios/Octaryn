#pragma once
#include "Selection.h"
#include "InstanceSelection.h"
#include <algorithm>
#include <cmath>

namespace octaryn::client::rendering::virtual_geometry {
inline float ray_cut_error(const GeometryAsset& asset,std::span<const std::uint32_t> clusters,const SelectionView& view,
    std::span<const GeometryTransform> transforms={}) {
  double maximum{};
  if(!transforms.empty()) {
    for(const auto& transform:transforms) {
      const auto instance=instance_selection_view(view,transform);
      for(const auto id:clusters)maximum=std::max(maximum,double(instance_selection_error(instance,asset.clusters[id].bounds)));
    }
    return float(maximum);
  }
  for(const auto id:clusters) {
    const auto& bounds=asset.clusters[id].bounds;
    if(bounds.error==0)continue;
    double squared{};
    for(unsigned axis=0;axis<3;++axis) {
      const auto delta=double(bounds.center[axis])-view.eye[axis];squared+=delta*delta;
    }
    const auto distance=std::max(.001,std::sqrt(squared)-bounds.radius);
    maximum=std::max(maximum,double(bounds.error)*view.focal_pixels/distance);
  }
  if(maximum==0)return 0;
  if(maximum>=std::numeric_limits<float>::max())return std::numeric_limits<float>::max();
  return std::nextafter(float(maximum),std::numeric_limits<float>::infinity());
}
// Failed replacements retain the complete immutable cut. Streaming another
// child page cannot make that same camera's rejected build fit the budget.
class RayAdmission {
  SelectionView rejected_view_{};
  float rejected_error_{};
  bool limited_{};
public:
  void reject(const SelectionView& view,float error) {rejected_view_=view;rejected_error_=error;limited_=true;}
  void clear() {limited_=false;}
  bool limited() const {return limited_;}
  bool allows(const SelectionView& view,float error) const {
    if(!limited_)return true;
    double distance{};
    for(unsigned axis=0;axis<3;++axis) {
      const auto delta=double(view.eye[axis])-rejected_view_.eye[axis];distance+=delta*delta;
    }
    // One metre of travel, a 10% focal change, or a 25% change in the
    // published cut's projected error warrants a new admission decision.
    return distance>=1 || view.focal_pixels>rejected_view_.focal_pixels*1.1f ||
        view.focal_pixels<rejected_view_.focal_pixels/1.1f ||
        error>std::max(1.f,rejected_error_)*1.25f || rejected_error_>std::max(1.f,error)*1.25f;
  }
};
}
