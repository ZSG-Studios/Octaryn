#pragma once
#include "MapRendererInternal.h"
#include <glaze/glaze.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
namespace octaryn::client::rendering {
inline bool map_transform_diagnostics_enabled() {
  static const bool enabled=[] {const auto* value=std::getenv("OCTARYN_CLIENT_TRANSFORM_DIAGNOSTICS");return value && std::string_view(value)=="1";}();
  return enabled;
}
inline bool map_transform_diagnostics_selected(const MapPrimitiveSource& source) {
  static const std::string filter=[] {const auto* value=std::getenv("OCTARYN_CLIENT_TRANSFORM_FILTER");return value?std::string(value):std::string();}();
  return filter.empty() || source.mesh_name.find(filter)!=std::string::npos || source.node_name.find(filter)!=std::string::npos;
}
struct MapTransformSourceDiagnostic {
  unsigned version{1};std::uint32_t node{},mesh{},primitive{},instance{UINT32_MAX};
  std::string node_name,mesh_name,route;std::array<float,16> evaluated{};
  std::array<float,12> shader_world{},shader_normal{};std::array<float,6> source_bounds{},render_bounds{},shader_world_bounds{},expected_world_bounds{};
  float orientation{},determinant{},normal_inverse_error{},bounds_escape{},source_transform_error{};
  bool finite{},orientation_valid{},normal_valid{},source_bounds_valid{},source_transform_valid{};
};
inline MapTransformSourceDiagnostic map_transform_source_diagnostic(const MapPrimitiveSource& source,
    const MapPrimitive& draw,const virtual_geometry::GeometryTransform& shader,bool instanced,unsigned instance) {
  MapTransformSourceDiagnostic record;record.node=source.node;record.mesh=source.mesh;record.primitive=draw.source.primitive;
  record.node_name=source.node_name;record.mesh_name=draw.source.mesh_name;record.instance=instance;
  record.route=instanced?"instanced":"flattened";record.evaluated=source.evaluated;record.source_bounds=draw.source.local_bounds;
  record.shader_world=shader.world;record.shader_normal=shader.normal;record.orientation=shader.orientation;
  for(unsigned axis=0;axis<3;++axis) {record.render_bounds[axis]=draw.bounds_min[axis];record.render_bounds[axis+3]=draw.bounds_max[axis];}
  record.finite=std::isfinite(shader.orientation);
  for(float value:source.evaluated)record.finite=record.finite && std::isfinite(value);
  for(float value:record.source_bounds)record.finite=record.finite && std::isfinite(value);
  for(float value:record.render_bounds)record.finite=record.finite && std::isfinite(value);
  for(float value:shader.world)record.finite=record.finite && std::isfinite(value);
  for(float value:shader.normal)record.finite=record.finite && std::isfinite(value);
  const auto& m=shader.world;
  record.determinant=m[0]*(m[5]*m[10]-m[6]*m[9])-m[1]*(m[4]*m[10]-m[6]*m[8])+m[2]*(m[4]*m[9]-m[5]*m[8]);
  record.orientation_valid=record.finite && std::isfinite(record.determinant) && std::abs(record.determinant)>1e-20f && shader.orientation==(record.determinant<0?-1.f:1.f);
  for(unsigned row=0;row<3;++row)for(unsigned column=0;column<3;++column) {
    double value{};for(unsigned k=0;k<3;++k)value+=double(m[k*4+row])*shader.normal[k*4+column];
    record.normal_inverse_error=std::max(record.normal_inverse_error,float(std::abs(value-(row==column?1:0))));
  }
  record.normal_valid=record.finite && record.normal_inverse_error<.001f;
  virtual_geometry::GeometryTransform expected;std::string error;
  record.source_bounds_valid=virtual_geometry::geometry_transform(source.evaluated,expected,error);
  record.source_transform_valid=record.source_bounds_valid;
  const std::array<float,6> draw_bounds{draw.bounds_min[0],draw.bounds_min[1],draw.bounds_min[2],draw.bounds_max[0],draw.bounds_max[1],draw.bounds_max[2]};
  record.shader_world_bounds=virtual_geometry::geometry_transform_bounds(shader,draw_bounds);
  if(record.source_bounds_valid && instanced) {
    record.expected_world_bounds=virtual_geometry::geometry_transform_bounds(expected,draw_bounds);
    for(unsigned lane=0;lane<12;++lane)record.source_transform_error=std::max(record.source_transform_error,std::abs(shader.world[lane]-expected.world[lane]));
    record.source_transform_valid=record.source_transform_error<.001f;
  }
  if(record.source_bounds_valid && !instanced) {
    const virtual_geometry::GeometryTransform identity;
    for(unsigned lane=0;lane<12;++lane)record.source_transform_error=std::max(record.source_transform_error,std::abs(shader.world[lane]-identity.world[lane]));
    record.source_transform_valid=record.finite && record.source_transform_error<.001f;
    const auto enclosure=virtual_geometry::geometry_transform_bounds(expected,draw.source.local_bounds);record.expected_world_bounds=enclosure;
    for(unsigned axis=0;axis<3;++axis)record.bounds_escape=std::max(record.bounds_escape,
        std::max(enclosure[axis]-draw.bounds_min[axis],draw.bounds_max[axis]-enclosure[axis+3]));
    record.source_bounds_valid=record.bounds_escape<.001f;
  }
  for(unsigned axis=0;axis<3;++axis)record.source_bounds_valid=record.source_bounds_valid &&
      record.source_bounds[axis]<=record.source_bounds[axis+3] && record.render_bounds[axis]<=record.render_bounds[axis+3];
  for(float value:record.shader_world_bounds)record.finite=record.finite && std::isfinite(value);
  for(float value:record.expected_world_bounds)record.finite=record.finite && std::isfinite(value);
  return record;
}
inline bool map_transform_diagnostics_snapshot(MapRenderer& map) {
  if(!map_transform_diagnostics_enabled())return true;
  if(map.transform_diagnostics_snapshot)return map.transform_diagnostics_valid;
  map.transform_diagnostics_snapshot=true;unsigned emitted{};std::string json;
  for(const auto& draw:map.model.primitives) {
    const auto count=std::max<std::size_t>(1,map.geometry_instances.size());
    for(std::size_t instance=0;instance<count && emitted<64;++instance) {
      const auto& source=instance<map.instance_sources.size()?map.instance_sources[instance]:draw.source;
      auto identity=source;if(identity.mesh_name.empty())identity.mesh_name=draw.source.mesh_name;
      if(!map_transform_diagnostics_selected(identity))continue;
      const virtual_geometry::GeometryTransform unit;const auto& transform=map.geometry_instances.empty()?unit:map.geometry_instances[instance];
      const auto record=map_transform_source_diagnostic(identity,draw,transform,!map.geometry_instances.empty(),unsigned(instance));
      map.transform_diagnostics_valid=map.transform_diagnostics_valid && record.finite && record.orientation_valid && record.normal_valid && record.source_bounds_valid && record.source_transform_valid;
      if(!glz::write_json(record,json))std::printf("map_transform_source %s\n",json.c_str());++emitted;
    }
    if(emitted==64)break;
  }
  std::printf("map_transform_sources emitted=%u cap=64 instance_revision=%llu valid=%u\n",emitted,static_cast<unsigned long long>(map.geometry_instances_revision),unsigned(map.transform_diagnostics_valid));
  return map.transform_diagnostics_valid;
}
struct MapTransformFrameDiagnostic {
  unsigned version{1};std::uint64_t frame{},instance_revision{};std::string route;
  std::array<float,5> camera{};bool frustum_enabled{};unsigned selected_clusters{};float achieved_error_pixels{},requested_error_pixels{};
  std::size_t instances{},source_primitives{};bool gpu_vertex_readback{};
};
template<class Camera> inline bool map_transform_diagnostics_frame(MapRenderer& map,std::uint64_t frame,const Camera& camera,
    bool culling,unsigned selected,float achieved,float requested,const char* route) {
  if(!map_transform_diagnostics_enabled())return true;
  if(map.transform_diagnostics_frame==frame || frame%120)return map.transform_diagnostics_valid;
  map.transform_diagnostics_frame=frame;map.transform_diagnostics_snapshot=false;
  if(!map_transform_diagnostics_snapshot(map))return false;
  MapTransformFrameDiagnostic record;record.frame=frame;record.instance_revision=map.geometry_instances_revision;record.route=route;
  record.camera={camera.x,camera.y,camera.z,camera.yaw,camera.pitch};record.frustum_enabled=culling;record.selected_clusters=selected;
  for(float value:record.camera)if(!std::isfinite(value)) {map.transform_diagnostics_valid=false;return false;}
  record.achieved_error_pixels=achieved;record.requested_error_pixels=requested;record.instances=map.geometry_instances.size();record.source_primitives=map.model.primitives.size();
  std::string json;if(!glz::write_json(record,json))std::printf("map_transform_frame %s\n",json.c_str());return true;
}
}
