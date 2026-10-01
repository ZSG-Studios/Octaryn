#include "GeometryTransform.h"
#include <cmath>
#include <stdexcept>

void test_geometry_transform() {
  using namespace octaryn::client::rendering::virtual_geometry;
  const auto require=[](bool valid){if(!valid)throw std::runtime_error("instance transform contract failed");};
  const auto close=[](float a,float b){return std::abs(a-b)<1e-5f;};
  const std::array<float,16> source{-2,0,0,0,1,3,0,0,0,0,.5f,0,10,-4,8,1};
  GeometryTransform transform;std::string error;require(geometry_transform(source,transform,error));
  const std::array<float,3> object{2,3,4};const auto world=geometry_transform_point(transform.world,object);
  const auto returned=geometry_transform_point(transform.inverse,world);
  for(unsigned axis=0;axis<3;++axis)require(close(object[axis],returned[axis]));
  require(transform.orientation==-1 && transform.scale>=3 && transform.inverse_scale>=2);
  const auto bounds=geometry_transform_bounds(transform,{0,0,0,1,1,1});
  require(bounds==std::array<float,6>{8,-4,8,11,-1,8.5f});
  // Inverse-transpose normals stay perpendicular to transformed tangents under shear.
  const auto& m=transform.world;const auto& n=transform.normal;
  require(close(n[1]*m[0]+n[5]*m[4]+n[9]*m[8],0));
  require(close(n[1]*m[2]+n[5]*m[6]+n[9]*m[10],0));
  SelectionView view{{12,5,10},512,1};view.frustum=true;view.planes[0][0]=1;view.planes[0][3]=-3;
  const auto local=geometry_local_view(transform,view);
  const auto projected_eye=geometry_transform_point(transform.world,{local.eye[0],local.eye[1],local.eye[2]});
  for(unsigned axis=0;axis<3;++axis)require(close(projected_eye[axis],view.eye[axis]));
  float local_distance=local.planes[0][3];
  for(unsigned axis=0;axis<3;++axis)local_distance+=local.planes[0][axis]*object[axis];
  require(close(local_distance,world[0]-3) && local.focal_pixels>=view.focal_pixels && local.error_pixels==0);
  auto singular=source;singular[10]=0;require(!geometry_transform(singular,transform,error));
}
