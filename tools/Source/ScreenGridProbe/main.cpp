#include "ScreenGrid.cpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <set>
#include <utility>

bool intersects(double sx,double sy,double ex,double ey,int x,int y) {
  double lo=0,hi=1;
  for(const auto axis: {std::array<double,3>{sx,ex-sx,double(x)}, {sy,ey-sy,double(y)}}) {
    if(std::abs(axis[1])<1e-12) {
      if(axis[0]<axis[2] || axis[0]>axis[2]+1)return false;
    } else {
      double a=(axis[2]-axis[0])/axis[1],b=(axis[2]+1-axis[0])/axis[1];
      if(a>b)std::swap(a,b);lo=std::max(lo,a);hi=std::min(hi,b);
      if(lo>hi+1e-10)return false;
    }
  }
  return true;
}
bool check(float sx,float sy,float ex,float ey,bool expect_complete=true) {
  auto grid=screen_grid_begin_0({sx,sy},{ex,ey});
  std::set<std::pair<int,int>> guarded;bool complete=false;
  for(unsigned step=0;step<96;++step) {
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)guarded.emplace(grid.cell_0.x+x,grid.cell_0.y+y);
    const auto end=screen_grid_exit_0(&grid);
    if(end<grid.entered_0 || !std::isfinite(end))return false;
    if(end>=1){complete=true;break;}
    screen_grid_advance_0(&grid);
  }
  if(complete!=expect_complete)return false;
  if(!complete)return true; // Runtime falls back; it never accepts an incomplete march.
  for(int y=int(std::floor(std::min(sy,ey)))-1;y<=int(std::ceil(std::max(sy,ey)));++y)
    for(int x=int(std::floor(std::min(sx,ex)))-1;x<=int(std::ceil(std::max(sx,ex)));++x)
      if(intersects(sx,sy,ex,ey,x,y) && !guarded.contains({x,y}))return false;
  return true;
}
int main() {
  unsigned checked=0;
  for(float sx: {-8.f,-1.000001f,-1.f,0.f,.000001f,.25f,1.f,8.f})
    for(float sy: {-8.f,-1.f,0.f,.5f,1.f,8.f})
      for(float ex: {-8.f,-1.f,0.f,.000001f,.25f,1.f,1.000001f,8.f})
        for(float ey: {-8.f,-1.f,0.f,.5f,1.f,8.f}) {
          if(!check(sx,sy,ex,ey))return 1;++checked;
        }
  if(!check(.25f,.25f,1000.f,1000.f,false))return 2;
  std::printf("screen_grid_production_cpu passed=1 paths=%u oracle=segment_box_intersection bounded_fallback=1\n",checked);
}
