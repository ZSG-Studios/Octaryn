#include "Camera.h"
#include "WorldRendererInternal.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace {
unsigned checks{};
void expect(bool condition,const char* reason) {
  ++checks;if(!condition) throw std::runtime_error(reason);
}
// Independent half-space oracle: project every box corner using the camera
// basis and the Vulkan 0 <= Z <= W contract, without extracted planes.
bool projected_box(const camera& eye,const std::array<float,6>& box,float jitter_x=0,float jitter_y=0) {
  const double sy=std::sin(double(eye.yaw_radians)),cy=std::cos(double(eye.yaw_radians));
  const double sp=std::sin(double(eye.pitch_radians)),cp=std::cos(double(eye.pitch_radians));
  const double focal=1/std::tan(double(eye.vertical_field_of_view_radians)*.5);
  const double aspect=double(eye.viewport_width)/eye.viewport_height;
  std::array<bool,6> any{};
  for(unsigned corner=0;corner<8;++corner) {
    const double x=double(box[0])-eye.position[0]+((corner&1)?box[3]:0);
    const double y=double(box[1])-eye.position[1]+((corner&2)?box[4]:0);
    const double z=double(box[2])-eye.position[2]+((corner&4)?box[5]:0);
    const double right=x*cy+z*sy,up=-x*sy*sp+y*cp+z*cy*sp;
    const double forward=x*sy*cp+y*sp-z*cy*cp;
    double cx,cy_clip,w;
    if(eye.projection_mode==CAMERA_PROJECTION_PERSPECTIVE) {
      cx=right*focal/aspect;cy_clip=up*focal;w=forward;
    } else {
      cx=right/(eye.orthographic_size*aspect);cy_clip=up/eye.orthographic_size;w=1;
    }
    cx+=jitter_x*w;cy_clip+=jitter_y*w;
    const std::array<double,6> planes{w+cx,w-cx,w+cy_clip,w-cy_clip,
      forward-eye.near_plane,eye.far_plane-forward};
    for(unsigned p=0;p<6;++p) any[p]=any[p] || planes[p]>=0;
  }
  for(bool visible:any) if(!visible) return false;
  return true;
}
void temporal_edges() {
  using namespace octaryn::client::rendering;
  WorldRenderer renderer;renderer.width=1280;renderer.height=720;
  renderer.temporal.mode=5;renderer.temporal.width=427;renderer.temporal.height=240;
  auto& column=renderer.columns[{-36,64}];column.face_count=1;column.min_y=0;column.height=32;
  WorldCamera eye{-3200,16,3199.5f,0,0,1.57079632679f};
  const std::array<float,6> box{-1152,0,2048,32,32,32};
  const auto oracle=[&](int width,int height,float jx,float jy) {
    camera projected{};camera_init(&projected,CAMERA_PROJECTION_PERSPECTIVE);
    projected.position[0]=eye.x;projected.position[1]=eye.y;projected.position[2]=eye.z;
    projected.vertical_field_of_view_radians=eye.vertical_fov;projected.far_plane=8192;
    camera_resize(&projected,width,height);
    return projected_box(projected,box,jx,jy);
  };
  expect(oracle(427,240,0,0) && !oracle(1280,720,0,0),
      "rounded Ultra aspect fixture must expose a visible wedge outside display-aspect frustum");
  world_renderer_prepare_draw(renderer,eye);
  expect(renderer.draw_list.visible.size()==1,"actual preparation falsely culled rounded Ultra edge");
  renderer.temporal.mode=0;world_renderer_prepare_draw(renderer,eye);
  expect(renderer.draw_list.visible.empty(),"Off culling must keep its original display projection");
  renderer.temporal.mode=5;
  // Each signed jitter case admits a box that the unjittered projection rejects.
  // Align the farthest corner to a side plane; retain actual signed column bounds.
  constexpr float distance=1152;
  const float focal=1/std::tan(eye.vertical_fov*.5f);
  for(int axis=0;axis<2;++axis)for(int sign:{-1,1}) {
    eye.x=-1136;eye.y=16;eye.z=3200;eye.jitter_x=eye.jitter_y=0;
    const float limit=distance/focal*(axis==0?427.f/240.f:1.f);
    if(axis==0) {
      eye.x=sign>0?box[0]-limit-.25f:box[0]+box[3]+limit+.25f;
      eye.jitter_x=-float(sign)/427;
    } else {
      eye.y=sign>0?box[1]-limit-.25f:box[1]+box[4]+limit+.25f;
      eye.jitter_y=-float(sign)/240;
    }
    expect(!oracle(427,240,0,0) && oracle(427,240,eye.jitter_x,eye.jitter_y),
        "signed jitter fixture must cross the actual raster side plane");
    world_renderer_prepare_draw(renderer,eye);
    expect(renderer.draw_list.visible.size()==1,"jittered visible edge column was falsely culled");
  }
}
}

void check_camera_culling() {
  temporal_edges();
  for(auto mode:{CAMERA_PROJECTION_PERSPECTIVE,CAMERA_PROJECTION_ORTHOGRAPHIC}) {
    camera eye{};camera_init(&eye,mode);eye.far_plane=8192;
    camera_resize(&eye,1280,720);camera_update(&eye);
    expect(!camera_is_box_visible(&eye,-1,-1,-9000,2,2,32),
      "box entirely beyond far plane must be culled");
    expect(camera_is_box_visible(&eye,-1,-1,-8193,2,2,2),
      "box crossing far plane must remain visible");
    expect(!camera_is_box_visible(&eye,-.001f,-.001f,-.05f,.002f,.002f,.01f),
      "box entirely before near plane must be culled");
    for(float yaw:{-2.7f,-1.2f,0.f,.8f,2.1f}) for(float pitch:{-1.4f,-.5f,0.f,.7f,1.4f}) {
      eye.yaw_radians=yaw;eye.pitch_radians=pitch;
      eye.position[0]=-32768;eye.position[1]=63.5f;eye.position[2]=65536;
      camera_update(&eye);
      for(int i=0;i<240;++i) {
        const float dx=float((i*431)%17001-8500),dy=float((i*137)%1801-900);
        const float dz=float((i*797)%19001-9500);
        const std::array<float,6> box{eye.position[0]+dx,eye.position[1]+dy,
          eye.position[2]+dz,32,float(1+(i*19)%512),32};
        const bool actual=camera_is_box_visible(&eye,box[0],box[1],box[2],box[3],box[4],box[5])!=0;
        expect(actual==projected_box(eye,box),"frustum differs from independent projected box oracle");
      }
    }
  }
  std::printf("camera_culling=passed checks=%u\n",checks);
}
