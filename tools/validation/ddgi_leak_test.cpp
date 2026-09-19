#include "DDGILeakCpu.cpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
constexpr unsigned Count=512,Texels=4;
struct Field {
    std::vector<DDGIControl_0> controls{Count};
    std::vector<DDGIProbe_0> probes{Count};
    std::vector<Vector<float,4>> irradiance=std::vector<Vector<float,4>>(Count*Texels);
    std::vector<Vector<float,2>> distance=std::vector<Vector<float,2>>(Count*Texels);
    void fill(float energy,float depth,unsigned history=100,bool inactive=false) {
        for(int z=0;z<8;++z)for(int y=0;y<8;++y)for(int x=0;x<8;++x) {
            unsigned i=x+8*(y+8*z);
            controls[i]={{x,y,z},1,0,{0,0,0}};
            probes[i]={{0,0,0,inactive?1.f:0.f},{1,99,0,history}};
            for(unsigned t=0;t<Texels;++t) {
                irradiance[i*Texels+t]={energy,energy*.5f,energy*.25f,1};
                distance[i*Texels+t]={depth,depth*depth};
            }
        }
    }
};
void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
Vector<float,4> sample(Field& field,float spacing,Vector<float,4> normal,Field* fine=nullptr) {
    GlobalParams_0 g{};
    Vector<float,4> position{3.5f*spacing,3.5f*spacing,3.5f*spacing,0},result{};
    g.ddgiGrid_0={8,8,8,1};g.ddgiParameters_0={spacing,.94f,1024,0};
    g.ddgiFrame_0={100,176,2,2};g.ddgiFadeOrigin_0={0,0,0,.2f};
    g.ddgiControls_0={field.controls.data(),Count};g.ddgiProbes_0={field.probes.data(),Count};
    g.ddgiIrradiance_0={field.irradiance.data(),Count*Texels};
    g.ddgiDistance_0={field.distance.data(),Count*Texels};
    if(fine) {
        g.ddgiFineGrid_0={8,8,8,1};g.ddgiFineParameters_0={spacing*.99f,.94f,1024,0};
        g.ddgiFineFrame_0=g.ddgiFrame_0;g.ddgiFineFadeOrigin_0=g.ddgiFadeOrigin_0;
        g.ddgiFineControls_0={fine->controls.data(),Count};g.ddgiFineProbes_0={fine->probes.data(),Count};
        g.ddgiFineIrradiance_0={fine->irradiance.data(),Count*Texels};
        g.ddgiFineDistance_0={fine->distance.data(),Count*Texels};
    }
    g.positions_0={&position,1};g.normals_0={&normal,1};g.results_0={&result,1};
    ComputeVaryingInput dispatch{};dispatch.endGroupID={1,1,1};main_0(&dispatch,nullptr,&g);
    require(std::isfinite(result.x)&&std::isfinite(result.w),"non-finite sampled field");
    return result;
}
}
int main() {
    try {
        Field field,fine;
        const Vector<float,4> normals[]={{1,0,0,0},{-1,0,0,0},{0,1,0,0},{0,-1,0,0},{0,0,1,0},{0,0,-1,0}};
        unsigned cases=0;
        for(float spacing:{1.f,4.f,8.f,16.f,64.f})for(auto normal:normals) {
            field.fill(8,1024);
            auto open=sample(field,spacing,normal);
            require(std::abs(open.x-8)<1e-5f&&open.w>.999f,"open constant field lost energy");
            field.fill(8,.1f);
            auto blocked=sample(field,spacing,normal);
            require(blocked.x<.016f&&blocked.w>.999f,"all-blocked cage renormalized outdoor energy");
            // A single mutually visible corner is enough to preserve a uniform field.
            unsigned corner=3+8*(3+8*3);
            for(unsigned t=0;t<Texels;++t)field.distance[corner*Texels+t]={1024,1024*1024};
            auto doorway=sample(field,spacing,normal);
            require(std::abs(doorway.x-8)<1e-5f,"one visible probe lost constant energy");
            field.fill(64,.1f);
            for(unsigned t=0;t<Texels;++t) {
                field.distance[corner*Texels+t]={1024,1024*1024};
                field.irradiance[corner*Texels+t]={0,0,0,1};
            }
            auto mixed=sample(field,spacing,normal);
            require(mixed.x<.0001f,"blocked bright corners contaminated visible dark probe");
            ++cases;
        }
        field.fill(8,1024);fine.fill(64,.1f);
        auto blocked=sample(field,1,normals[2],&fine);
        require(blocked.x<.15f&&blocked.w>.999f,"blocked fine cage fell through to bright coarse field");
        fine.fill(0,1024,0);
        auto pending=sample(field,1,normals[2],&fine);
        require(std::abs(pending.x-8)<1e-5f,"pending fine field did not hand off to coarse");
        fine.fill(64,1024,100,true);
        auto solid=sample(field,1,normals[2],&fine);
        require(solid.x==0&&solid.w>.999f,"solid fine cage lost dark coverage");
        fine.fill(0,1024);
        auto dark=sample(field,1,normals[2],&fine);
        require(dark.x==0&&dark.w>.999f,"valid dark field became missing coverage");
        field.fill(8,1024,0);
        auto absent=sample(field,1,normals[2]);
        require(absent.x==0&&absent.w==0,"pending standalone cage claimed known occlusion");
        std::printf("ddgi_leak=passed directional_scale_cases=%u hierarchy_cases=5 GPU_execution=not_run\n",cases);
        return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
}
