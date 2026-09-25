#include "ShadowReprojection.cpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

using Point=std::array<double,3>;
Vector<float,3> vector(Point p) {return {float(p[0]),float(p[1]),float(p[2])};}
int main() {
    std::vector<Case_0> cases;
    std::vector<unsigned> expected;
    unsigned legacyRejected=0;
    auto append=[&](Point old,Point world,Point eye,Point ray,unsigned voxel,bool accepted) {
        cases.push_back({vector(old),vector(world),vector(eye),vector(ray),voxel,.025f});
        expected.push_back(accepted?1:0);
    };
    // Independent geometry: same plane, two rays through different subpixel
    // centers. Their tangential separation is not a disocclusion.
    const unsigned axes[]={2,2,0,0,1,1};
    for(unsigned face=0;face<6;++face)for(double depth:{4.,16.,64.,128.})
    for(double jitter:{-.875,-.375,.125,.625}) {
        const auto axis=axes[face],tangent=(axis+1)%3;
        Point eye={1024.25,164.5,-1023.75},ray={.15,.2,.3};
        ray[axis]=face%2==0?-1.:1.;
        Point old,world;
        for(unsigned i=0;i<3;++i)old[i]=world[i]=eye[i]+ray[i]*depth;
        world[tangent]+=jitter*2*depth/720.;
        if(std::abs(world[tangent]-old[tangent])>=.025)++legacyRejected;
        append(old,world,eye,ray,face,true);
        Point obstructed=old;obstructed[axis]+=.125;
        append(obstructed,world,eye,ray,face,false);
        Point wrongTap=old;wrongTap[tangent]+=.125;
        append(wrongTap,world,eye,ray,face,false);
        append(old,world,eye,ray,face|256u,std::abs(world[tangent]-old[tangent])<.025);
        Point parallel=ray;parallel[axis]=0;
        append(old,world,eye,parallel,face,false);
        Point behind=eye;behind[axis]-=ray[axis]*depth;
        append(old,behind,eye,ray,face,false);
    }
    std::vector<unsigned> results(cases.size());
    GlobalParams_0 globals{};
    globals.cases_0={cases.data(),cases.size()};globals.results_0={results.data(),results.size()};
    ComputeVaryingInput dispatch{};dispatch.endGroupID={unsigned(cases.size()),1,1};
    main_0(&dispatch,nullptr,&globals);
    for(unsigned i=0;i<results.size();++i)if(results[i]!=expected[i]) {
        std::printf("case=%u expected=%u actual=%u\n",i,expected[i],results[i]);
        throw std::runtime_error("shadow reprojection mismatch");
    }
    if(legacyRejected<30)throw std::runtime_error("jitter regression not exercised");
    std::printf("shadow_reprojection=passed cases=%zu legacy_static_rejections=%u\n",cases.size(),legacyRejected);
}
