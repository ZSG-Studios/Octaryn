#include "DDGIVisibilityCpu.cpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

int main() {
    constexpr unsigned n=16,count=512;
    std::vector<DDGIControl_0> controls(count);
    std::vector<DDGIProbe_0> probes(count);
    std::vector<Vector<float,4>> irradiance(count*4);
    std::vector<Vector<float,2>> distance(count*n*n,{10,100});
    Vector<float,4> position{3.5f,3.25f,3.25f,0},result{};
    GlobalParams_0 g{};g.ddgiGrid_0={8,8,8,1};g.ddgiParameters_0={1,.94f,64,0};
    g.ddgiFrame_0={100,176,2,n};g.ddgiFadeOrigin_0={0,0,0,.001f};
    g.ddgiControls_0={controls.data(),controls.size()};g.ddgiProbes_0={probes.data(),probes.size()};
    g.ddgiIrradiance_0={irradiance.data(),irradiance.size()};g.ddgiDistance_0={distance.data(),distance.size()};
    g.positions_0={&position,1};g.results_0={&result,1};
    ComputeVaryingInput dispatch{};dispatch.endGroupID={1,1,1};
    for(unsigned mirror=0;mirror<2;++mirror) {
        for(int x=3;x<=4;++x) {
            unsigned index=x+8*(3+8*3);bool bright=x==int(3+mirror);
            controls[index]={{x,3,3},1,0,{0,0,0}};probes[index]={{0,0,0,0},{1,99,0,100}};
            for(unsigned i=0;i<4;++i)irradiance[index*4+i]={bright?1.f:0.f,0,0,1};
            for(unsigned v=0;v<n;++v)for(unsigned u=0;u<n;++u) {
                // Octahedral x has the same sign as its unfolded coordinate.
                bool towardReceiver=mirror==0?u>=n/2:u<n/2;
                float d=bright&&towardReceiver?.1f:10.f;
                distance[index*n*n+v*n+u]={d,d*d};
            }
        }
        main_0(&dispatch,nullptr,&g);
        if(result.x>.005f||result.w<.99f)throw std::runtime_error("opposite-side visibility admitted blocked bright probe");
        std::printf("ddgi_visibility=passed mirror=%u blocked_energy=%.7f coverage=%.3f\n",mirror,result.x,result.w);
    }
}
