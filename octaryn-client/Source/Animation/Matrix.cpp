#include "Pose.h"
#include <algorithm>
#include <cmath>
namespace octaryn::client::animation {
Matrix multiply(const Matrix& a,const Matrix& b) {
  Matrix result{};
  for(int c=0;c<4;++c)for(int r=0;r<4;++r)for(int k=0;k<4;++k)result[c*4+r]+=a[k*4+r]*b[c*4+k];
  return result;
}
bool inverse(const Matrix& value,Matrix& result) {
  double rows[4][8]{};
  for(int r=0;r<4;++r)for(int c=0;c<4;++c) {rows[r][c]=value[c*4+r];rows[r][c+4]=r==c?1:0;}
  for(int c=0;c<4;++c) {
    int pivot=c;for(int r=c+1;r<4;++r)if(std::abs(rows[r][c])>std::abs(rows[pivot][c]))pivot=r;
    if(!std::isfinite(rows[pivot][c])||std::abs(rows[pivot][c])<1e-15)return false;
    for(int k=0;k<8;++k)std::swap(rows[c][k],rows[pivot][k]);
    const double factor=rows[c][c];for(double& v:rows[c])v/=factor;
    for(int r=0;r<4;++r)if(r!=c) {const double f=rows[r][c];for(int k=0;k<8;++k)rows[r][k]-=f*rows[c][k];}
  }
  for(int r=0;r<4;++r)for(int c=0;c<4;++c) {
    result[c*4+r]=static_cast<float>(rows[r][c+4]);if(!std::isfinite(result[c*4+r]))return false;
  }
  return true;
}
Vec3 point(const Matrix& m,const Vec3& p) {
  Vec3 result{};for(int r=0;r<3;++r)result[r]=m[r]*p[0]+m[4+r]*p[1]+m[8+r]*p[2]+m[12+r];return result;
}
Matrix compose(const Transform& t) {
  const auto& q=t.rotation;const float x=q[0],y=q[1],z=q[2],w=q[3];
  Matrix m{1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w),0,
    2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w),0,
    2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y),0,t.translation[0],t.translation[1],t.translation[2],1};
  for(int c=0;c<3;++c)for(int r=0;r<3;++r)m[c*4+r]*=t.scale[c];return m;
}
Bounds union_bounds(const Bounds& a,const Bounds& b) {
  if(!a.valid)return b;if(!b.valid)return a;Bounds out=a;
  for(int i=0;i<3;++i) {out.minimum[i]=std::min(a.minimum[i],b.minimum[i]);out.maximum[i]=std::max(a.maximum[i],b.maximum[i]);}
  return out;
}
}
