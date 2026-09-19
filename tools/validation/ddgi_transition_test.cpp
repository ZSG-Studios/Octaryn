#include "DDGITransitionCpu.cpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
constexpr float pi=3.14159265358979323846f;
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
struct Field {
    std::vector<DDGIControl_0> controls=std::vector<DDGIControl_0>(512);
    std::vector<DDGIProbe_0> probes=std::vector<DDGIProbe_0>(512);
    std::vector<Vector<float,4>> irradiance=std::vector<Vector<float,4>>(512*4);
    std::vector<Vector<float,2>> distance=std::vector<Vector<float,2>>(512*16,Vector<float,2>{10,100});
    TransitionCase_0 c{{3.5f,3.25f,3.25f},.1f,0,0,0};
    std::array<Vector<float,4>,3> result{};
    GlobalParams_0 g{};
    unsigned frame=100;
    Field() {
        for(int z=0;z<8;++z)for(int y=0;y<8;++y)for(int x=0;x<8;++x) {
            unsigned i=x+8*(y+8*z);
            controls[i]={{x,y,z},1,0,{0,0,0}};
            probes[i]={{0,0,0,0},{1,99,0,65535}};
        }
        g.ddgiGrid_0={8,8,8,1};g.ddgiParameters_0={1,.94f,64,0};
        g.ddgiFrame_0={frame,176,2,4};g.ddgiFadeOrigin_0={0,0,0,.001f};
        g.ddgiControls_0={controls.data(),controls.size()};g.ddgiProbes_0={probes.data(),probes.size()};
        g.ddgiIrradiance_0={irradiance.data(),irradiance.size()};g.ddgiDistance_0={distance.data(),distance.size()};
        g.cases_0={&c,1};g.results_0={result.data(),result.size()};
    }
    void energy(float value) {for(auto& texel:irradiance)texel={value,value,value,1};}
    void publish(unsigned flags) {
        ++frame;g.ddgiFrame_0.x=frame;
        for(auto& control:controls) {control.refreshFrame_0=frame;control.padding_0.z=flags;}
    }
    void sample() {
        ComputeVaryingInput dispatch{};dispatch.endGroupID={1,1,1};main_0(&dispatch,nullptr,&g);
    }
    float update(unsigned reactive) {
        c.reactive_0=reactive;sample();float value=result[2].x;energy(value);
        for(auto& probe:probes)probe.metadata_0.y=frame;
        ++frame;g.ddgiFrame_0.x=frame;return value;
    }
};
void validity() {
    for(unsigned flags:{5u,6u,7u}) {
        Field f;f.energy(3);f.publish(flags);f.sample();
        require(std::abs(f.result[0].x-3)<.00001f&&f.result[0].w==1,"pending presentation black hole");
        require(f.result[2].z==1,"light-only change unlocked geometry");
        if(flags&2)require(f.result[1].x==0&&f.result[1].w==1,"removed light reentered recursive feedback");
        else require(std::abs(f.result[1].x-3)<.00001f,"addition lost valid recursive history");
        // Only the successful update acknowledgement releases recursive exclusion.
        for(auto& probe:f.probes)probe.metadata_0.y=f.frame;
        f.sample();require(std::abs(f.result[1].x-3)<.00001f,"fresh observation still recursively rejected");
    }
    for(unsigned flags:{0u,1u,2u,3u}) {
        Field f;f.energy(3);f.publish(flags);f.sample();
        require(f.result[2].z==0,"concurrent geometry change was not revalidated");
    }
    for(unsigned invalid=0;invalid<5;++invalid)for(unsigned flags:{0u,5u,6u,7u}) {
        Field f;f.energy(3);f.publish(flags);
        for(unsigned i=0;i<f.probes.size();++i) {
            if(invalid==0)f.controls[i].padding_0.x=1;
            if(invalid==1)f.controls[i].padding_0.y=1;
            if(invalid==2)f.probes[i].offset_0.w=1;
            if(invalid==3)f.probes[i].metadata_0.w=0;
            if(invalid==4)f.probes[i].metadata_0.x=0;
        }
        f.sample();require(f.result[0].x==0&&f.result[1].x==0,"geometry-invalid history became visible");
        if(invalid<3)require(f.result[0].w==1,"occluded geometry leaked fallback coverage");
    }
    Field f;f.energy(3);f.publish(6);f.c.reset_0=1;f.sample();
    require(f.result[2].z==0,"reset retained geometry lock");
}
void transitions() {
    unsigned sequences=0;double avoidedDrop=0,worstRebuildDip=0,worstOvershoot=0;
    for(float dark:{0.f,.001f,.1f})for(float rho:{0.f,.4f,.8f}) {
        Field f;f.c.bounce_0=rho;float previous=pi*dark/(1-rho);f.energy(previous);
        // Repeated add/remove generations include zero and weakly lit cavities.
        for(unsigned event=0;event<12;++event) {
            bool removal=event%2!=0;float source=removal?dark:dark+.5f;
            float target=pi*source/(1-rho),old=previous;
            f.c.source_0=source;f.publish(removal?6:5);f.sample();
            require(std::abs(f.result[0].x-old)<.00003f,"publication changed display before an observation");
            if(removal) {
                require(f.result[1].x==0,"old emitter survived removal generation");
                avoidedDrop=std::max(avoidedDrop,double(old));
            }
            for(unsigned update=0;update<4000;++update) {
                previous=f.update(update<4?4-update:0);
                worstOvershoot=std::max(worstOvershoot,double(previous-std::max(old,target)));
                if(removal)worstRebuildDip=std::max(worstRebuildDip,double(target-previous));
                require(previous>=0&&previous<=std::max(old,target)+.0001f,"source transition overshot physical endpoint");
                if(update>=4)require(std::abs(f.result[2].y-.94f)<.000001f,"steady per-observation retention changed");
            }
            // The shader's normalized weighted average carries a ~1e-6 round-trip
// rounding error that the slow rho=0.8 eigenmode amplifies about 80x, so the
// physical fixed point is reached to ~2e-4 rather than to float precision.
            require(std::abs(previous-target)<5e-4f,"repeated source updates failed to converge");
            ++sequences;
        }
    }
    std::printf("ddgi_transition=passed sequences=%u pending_blackout_before=%.7f after=0 overshoot=%.7f remaining_rebuild_dip=%.7f\n",
        sequences,avoidedDrop,worstOvershoot,worstRebuildDip);
}
}
int main() try {
    validity();transitions();
    std::puts("ddgi_transition_validity=passed lighting_only=3 geometry=4 invalid=20 reset=1");
} catch(const std::exception& error) {
    std::fprintf(stderr,"ddgi_transition=failed reason=%s\n",error.what());return 1;
}
