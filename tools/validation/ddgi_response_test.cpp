#include "DDGIResponseCpu.cpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

constexpr float pi=3.14159265358979323846f;
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
float evaluate(float old,float radiance,float seconds,unsigned samples=65535,unsigned changed=0,unsigned lighting=48,
    unsigned reactive=0,unsigned reset=0,float* history=nullptr) {
    std::vector<DDGIRay_0> rays(64+lighting);
    for(unsigned i=0;i<rays.size();++i) {
        float y=1-2*(float(i%lighting)+.5f)/lighting;
        float angle=float(i)*2.39996323f,r=std::sqrt(1-y*y);
        rays[i]={{i<64?999.f:radiance,i<64?999.f:radiance,i<64?999.f:radiance,10},
            {std::cos(angle)*r,y,std::sin(angle)*r,0}};
    }
    ResponseCase_0 c{{old,old,old},{.3f,.8f,.51961524f},64,unsigned(rays.size()),samples,
        changed?6u:reactive,reset};
    Vector<float,4> result{};
    GlobalParams_0 globals{};globals.cases_0={&c,1};globals.rays_0={rays.data(),rays.size()};globals.results_0={&result,1};
    ComputeVaryingInput dispatch{};dispatch.endGroupID={1,1,1};main_0(&dispatch,nullptr,&globals);
    if(history)*history=result.w;
    require(std::isfinite(result.x),"nonfinite irradiance");return result.x;
}
void sparse_variance(float seconds) {
    constexpr unsigned count=64000,burn=2000;
    constexpr double expectedMean=pi*.35,rawVariance=pi*pi*4.*.125*.875;
    constexpr double expectedVariance=rawVariance*(1.-.94)/(1.+.94);
    unsigned random=0x139abe87;float state=float(expectedMean);
    double sum=0,squares=0;
    for(unsigned i=0;i<count+burn;++i) {
        random^=random<<13;random^=random>>17;random^=random<<5;
        state=evaluate(state,.1f+(random<0x20000000u?2.f:0.f),seconds);
        if(i>=burn) {sum+=state;squares+=double(state)*state;}
    }
    double mean=sum/count,variance=squares/count-mean*mean;
    require(std::abs(mean-expectedMean)<.035,"stationary sparse-light mean biased");
    require(std::abs(variance-expectedVariance)<expectedVariance*.12,"sparse observation variance exceeds mature filter");
    std::printf("ddgi_sparse seconds=%.6f mean=%.7f expected=%.7f variance=%.7f expected_variance=%.7f\n",
        seconds,mean,expectedMean,variance,expectedVariance);
}
void history_response() {
    for(float seconds:{.016f,.5f,1.f})sparse_variance(seconds);
    for(float seconds:{.016f,.5f,1.f}) {
        for(unsigned samples:{0u,1u,2u,8u,65535u})for(unsigned reactive:{0u,1u,4u,6u}) {
            float history=0;
            evaluate(20,.1f,seconds,samples,0,48,reactive,0,&history);
            float expected=std::fmin(.94f,float(samples)/float(samples+1));
            if(reactive)expected=std::fmin(expected,.5f);
            require(std::abs(history-expected)<.000001f,"warmup/reactive observation weighting changed");
        }
        for(float old:{0.f,1000.f})for(float incoming:{0.f,.001f,200.f})for(unsigned reactive:{0u,4u}) {
            float fresh=evaluate(0,incoming,seconds,0);
            require(evaluate(old,incoming,seconds,65535,0,48,reactive,1)==fresh,"reset must snap exactly");
            // An explicit edit leans on the bounded reactive policy instead of
            // replacing accumulated history with one noisy observation.
            require(std::abs(evaluate(old,incoming,seconds,65535,1,48,reactive)-(fresh*.5f+old*.5f))<.0001f,
                "explicit edit did not adopt bounded reactive blending");
        }
        float state=0,product=1;
        for(unsigned reactive:{4u,3u,2u,1u}) {state=evaluate(state,1,seconds,65535,0,48,reactive);product*=.5f;}
        require(std::abs(state/pi-(1.f-product))<.00001,"bounded reactive updates lost fast response");
        float history=0;evaluate(state,1,seconds,65535,0,48,0,0,&history);
        require(std::abs(history-.94f)<.000001,"mature retention did not resume after reactive updates");
    }
}
int main() try {
    history_response();
    for(unsigned rays:{16u,48u,112u})for(float light:{.001f,.1f,1.f,8.f}) {
        float result=evaluate(999,light,1.f/60,0,0,rays);
        require(std::abs(result-pi*light)<.0001f,"constant environment or fixed-ray exclusion failed");
        for(unsigned i=0;i<1000;++i)result=evaluate(result,light,1.f/60,65535,0,rays);
        require(std::abs(result-pi*light)<.0001f,"constant environment drifted");
    }
    // Same stationary mean, sparse direct-light hits. A positive-only clamp
    // destroys this mean once history matures, even though the scene never changes.
    float state=pi*.35f;double mean=0;
    for(unsigned i=0;i<12000;++i) {
        state=evaluate(state,.1f+(i%8==0?2.f:0.f),1.f/60);
        if(i>=4000)mean+=state/8000.;
    }
    require(std::abs(mean-pi*.35)<.0002,"stationary sparse-light estimator biased dark");
    // Linear diffuse enclosure: E = pi*S + rho*E. Both rising and falling
    // responses have the independently derived eigenvalue h+(1-h)*rho.
    for(float seconds:{1.f/144,1.f/60,1.f/30,.25f,.6f})for(float rho:{0.f,.4f,.8f}) {
        const double h=.94,eigen=h+(1-h)*rho;
        float state=pi*.1f/(1-rho);double expected=state;
        for(unsigned i=0;i<4100;++i) {
            float source=i<100?.6f:.1f;
            double equilibrium=pi*source/(1-rho);
            expected=equilibrium+(expected-equilibrium)*eigen;
            state=evaluate(state,source+rho*state/pi,seconds);
            require(std::abs(state-expected)<.0002,"multi-bounce step response changed energy or history");
        }
        require(std::abs(state-pi*.1f/(1-rho))<.0002,"removed torch did not return to the cold fixed point");
    }
    require(std::abs(evaluate(20,.1f,1.f/144,65535,0,48,0,1)-pi*.1f)<.00001,
        "relocated probe retained stale irradiance");
    std::printf("ddgi_response=passed constant_environment=12 sparse_mean=%.7f expected=%.7f step_sequences=15\n",mean,pi*.35);
} catch(const std::exception& error) {
    std::fprintf(stderr,"ddgi_response=failed reason=%s\n",error.what());return 1;
}
