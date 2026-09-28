#pragma once
#include "AnimationAsset.h"
#include <span>
namespace octaryn::client::animation {
struct Pose {std::vector<Matrix> world;std::vector<std::vector<float>> weights;};
struct MatrixRows {Vec4 rows[4];};
struct DeformationPose {
  Matrix world{identity};
  std::vector<MatrixRows> joints;
  std::vector<float> weights;
};
struct Bounds {Vec3 minimum{},maximum{};bool valid{};};
Matrix multiply(const Matrix&,const Matrix&);
bool inverse(const Matrix&,Matrix&);
Vec3 point(const Matrix&,const Vec3&);
Matrix compose(const Transform&);
bool sample_pose(const Asset&,std::int32_t clip,float seconds,Pose&,std::string& error);
bool deformation_pose(const Asset&,const Primitive&,const Pose&,DeformationPose&,std::string& error);
// Exact CPU reference for GPU output qualification and current/previous bound checks.
bool deform_positions(const Primitive&,const DeformationPose&,std::vector<Vec3>&,Bounds&,std::string& error);
Bounds union_bounds(const Bounds&,const Bounds&);
}
