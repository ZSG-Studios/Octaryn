#include "Probe.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
namespace mesh_probe {
float projected_edge_distance(const Mesh& mesh,const WorldCamera& camera,unsigned px,unsigned py) {
  const double sy=std::sin(camera.yaw),cy=std::cos(camera.yaw),sp=std::sin(camera.pitch),cp=std::cos(camera.pitch);
  const double focal=1/std::tan(camera.vertical_fov/2),x=px+.5,y=py+.5;
  double nearest=std::numeric_limits<double>::infinity();
  for(const auto& face:mesh.faces) {
    const unsigned direction=(face[3]>>16)&15;if(direction>=6)continue;
    const double width=((face[3]>>20)&31)+1,height=((face[3]>>25)&31)+1;
    double points[4][2]{};bool valid=true;
    for(unsigned i=0;i<4;++i) {
      const double u=(i==1 || i==2)?width:0,v=i>=2?height:0;
      double wx=std::bit_cast<int>(face[0]),wy=std::bit_cast<int>(face[1]),wz=std::bit_cast<int>(face[2]);
      if(direction<2){wx+=direction;wy+=v;wz+=u;}
      else if(direction<4){wx+=u;wy+=direction-2;wz+=v;}
      else{wx+=u;wy+=v;wz+=direction-4;}
      wx-=camera.x;wy-=camera.y;wz-=camera.z;
      const double depth=wx*sy*cp+wy*sp-wz*cy*cp;
      if(depth<=.1){valid=false;break;}
      points[i][0]=(focal*(wx*cy+wz*sy)/depth*.5+.5)*Fixture::Size;
      points[i][1]=(.5-focal*(-wx*sy*sp+wy*cp+wz*cy*sp)/depth*.5)*Fixture::Size;
    }
    if(!valid)continue;
    for(unsigned i=0;i<4;++i) {
      const auto& a=points[i];const auto& b=points[(i+1)%4];
      const double dx=b[0]-a[0],dy=b[1]-a[1],length=dx*dx+dy*dy;
      if(length==0)continue;
      const double t=std::clamp(((x-a[0])*dx+(y-a[1])*dy)/length,0.0,1.0);
      nearest=std::min(nearest,std::hypot(x-a[0]-t*dx,y-a[1]-t*dy));
    }
  }
  return float(nearest);
}
}
