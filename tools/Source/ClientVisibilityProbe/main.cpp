#include "Camera.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>

namespace {
struct Vec3 { double x,y,z; };
struct Box { float x,y,z,w,h,d; };
struct View {
    Vec3 eye,right,up,forward;
    double focal_x,focal_y;
};
struct Results {
    uint64_t views{},boxes{},corner_visible{},inside{},rejected{},false_culls{};
};
constexpr double near_plane=.1,far_plane=8192;
constexpr double pi=std::numbers::pi;
double dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
// Independent oracle: WorldRaster's right/up/forward dot products and Vulkan clip inequalities.
// It does not use Camera matrices, extracted planes, or its AABB support-point test.
bool corner_in_clip(const View& v,Vec3 point) {
    const Vec3 relative{point.x-v.eye.x,point.y-v.eye.y,point.z-v.eye.z};
    const double w=dot(relative,v.forward);
    if(w<=near_plane+1e-4 || w>=far_plane-1e-2) return false;
    const double x=dot(relative,v.right)*v.focal_x;
    const double y=dot(relative,v.up)*v.focal_y;
    const double z=w*far_plane/(far_plane-near_plane)-near_plane*far_plane/(far_plane-near_plane);
    // Interior margin excludes ambiguous floating-point contacts with an exact clip plane.
    return std::abs(x)<w*(1-2e-5) && std::abs(y)<w*(1-2e-5) && z>0 && z<w;
}
bool has_visible_corner(const View& view,const Box& box) {
    for(int corner=0;corner<8;++corner)
        if(corner_in_clip(view,{double(box.x)+((corner&1)?box.w:0),
                              double(box.y)+((corner&2)?box.h:0),
                              double(box.z)+((corner&4)?box.d:0)})) return true;
    return false;
}
void check(const camera& cam,const View& view,const Box& box,Results& results,bool contains_eye=false) {
    ++results.boxes;
    const bool corner=has_visible_corner(view,box);
    if(corner) ++results.corner_visible;
    if(contains_eye) ++results.inside;
    const bool visible=camera_is_box_visible(&cam,box.x,box.y,box.z,box.w,box.h,box.d)!=0;
    if(!visible) ++results.rejected;
    if((corner || contains_eye) && !visible) {
        if(results.false_culls<8) std::printf("false_cull yaw=%.8f pitch=%.8f fov=%.8f zoom=%d eye=(%.2f,%.2f,%.2f) box=(%.2f,%.2f,%.2f;%.2f,%.2f,%.2f) oracle=%s\n",
            cam.yaw_radians,cam.pitch_radians,cam.vertical_field_of_view_radians,cam.zoom_step,
            cam.position[0],cam.position[1],cam.position[2],box.x,box.y,box.z,box.w,box.h,box.d,
            contains_eye?"camera_inside":"visible_corner");
        ++results.false_culls;
    }
}
View oracle_view(const camera& cam,double tangent_half_fov) {
    const double sy=std::sin(double(cam.yaw_radians)),cy=std::cos(double(cam.yaw_radians));
    const double sp=std::sin(double(cam.pitch_radians)),cp=std::cos(double(cam.pitch_radians));
    return {{cam.position[0],cam.position[1],cam.position[2]},
            {cy,0,sy},{-sy*sp,cp,cy*sp},{sy*cp,sp,-cy*cp},
            double(cam.viewport_height)/cam.viewport_width/tangent_half_fov,1/tangent_half_fov};
}
void exercise(camera& cam,double tangent_half_fov,Results& results) {
    // Build oracle from the caller's unnormalized angles, before invoking the camera owner.
    const View view=oracle_view(cam,tangent_half_fov);
    camera_update(&cam);
    ++results.views;
    const float origin_x=std::floor(cam.position[0]/32)*32;
    const float origin_z=std::floor(cam.position[2]/32)*32;
    for(int x=-4;x<=4;++x) for(int z=-4;z<=4;++z)
        check(cam,view,{origin_x+float(x)*32,-64,origin_z+float(z)*32,32,320,32},results);
    check(cam,view,{cam.position[0]-16,cam.position[1]-128,cam.position[2]-16,32,256,32},results,true);
    // Probe all sides of the frustum at short and long ranges, with world-axis-aligned boxes.
    for(double distance:{.2,2.0,32.0,256.0,2048.0,8190.0})
        for(double side:{-.995,-.8,0.0,.8,.995}) for(double elevation:{-.8,0.0,.8}) {
            const double x=distance*side/view.focal_x,y=distance*elevation/view.focal_y;
            const Vec3 point{view.eye.x+view.forward.x*distance+view.right.x*x+view.up.x*y,
                             view.eye.y+view.forward.y*distance+view.right.y*x+view.up.y*y,
                             view.eye.z+view.forward.z*distance+view.right.z*x+view.up.z*y};
            const float size=distance<2?.02f:1.0f;
            check(cam,view,{float(point.x)-size/2,float(point.y)-size/2,float(point.z)-size/2,size,size,size},results);
        }
    check(cam,view,{float(view.eye.x-view.forward.x*64)-1,
                    float(view.eye.y-view.forward.y*64)-1,
                    float(view.eye.z-view.forward.z*64)-1,2,2,2},results);
}
}
int main() {
    Results results;
    const std::array<Vec3,5> positions{{{0,35.72,0},{-32.25,127.5,-63.75},
        {1032.5,-20.25,-4097.75},{-1000000.25,64,1000000.5},{-16,255.5,16}}};
    for(const auto eye:positions) for(const auto dimensions:{std::array{1280,720},std::array{720,1280},std::array{3440,1440}})
      for(double yaw_degrees:{-540.0,-180.0,-135.0,-90.0,-45.0,0.0,45.0,90.0,135.0,179.9,360.0})
       for(double pitch_degrees:{-88.0,-65.0,-35.0,0.0,35.0,65.0,88.0})
        for(double base_fov_degrees:{60.0,90.0,120.0}) for(int zoom=0;zoom<3;++zoom) {
            const float base=static_cast<float>(base_fov_degrees*pi/180);
            const double tangent=std::tan(double(base)/2)/double(1u<<zoom);
            camera cam{};camera_init(&cam,CAMERA_PROJECTION_PERSPECTIVE);
            cam.position[0]=float(eye.x);cam.position[1]=float(eye.y);cam.position[2]=float(eye.z);
            cam.yaw_radians=float(yaw_degrees*pi/180);cam.pitch_radians=float(pitch_degrees*pi/180);
            cam.far_plane=float(far_plane);camera_resize(&cam,dimensions[0],dimensions[1]);
            // Current renderer supplies effective FOV; also cover the camera owner's native zoom API.
            cam.vertical_field_of_view_radians=float(2*std::atan(tangent));
            exercise(cam,std::tan(double(cam.vertical_field_of_view_radians)/2),results);
            cam.vertical_field_of_view_radians=base;cam.zoom_step=zoom;
            exercise(cam,tangent,results);
        }
    std::printf("visibility_views=%llu boxes=%llu visible_corner_boxes=%llu camera_inside=%llu rejected=%llu false_culls=%llu\n",
        (unsigned long long)results.views,(unsigned long long)results.boxes,
        (unsigned long long)results.corner_visible,(unsigned long long)results.inside,
        (unsigned long long)results.rejected,(unsigned long long)results.false_culls);
    if(results.false_culls || results.corner_visible<100000 || results.rejected<100000) return 1;
    std::puts("camera_culling_independent_worldraster_clip_oracle=passed");
    return 0;
}
