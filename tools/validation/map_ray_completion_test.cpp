#include "../../octaryn-client/Source/MapWorld/MapRayCompletion.h"
#include <cstdio>
#include <initializer_list>
using namespace octaryn::client::rendering;
int main() {
    int checks=0,failures=0;
    auto check=[&](bool condition){++checks;if(!condition)++failures;};
    check(map_ray_completion(true,0,0)==MapRayCompletion::Pending);
    check(map_ray_completion(true,0,.999)==MapRayCompletion::Pending);
    check(map_ray_completion(true,0,1)==MapRayCompletion::Pending);
    check(map_ray_completion(true,0,1.001)==MapRayCompletion::Failed);
    check(map_ray_completion(false,1,0)==MapRayCompletion::Failed);
    check(map_ray_completion(true,UINT64_MAX,0)==MapRayCompletion::Failed);
    check(map_ray_completion(true,1,.001)==MapRayCompletion::Complete);
    check(map_ray_completion(true,1,2)==MapRayCompletion::Complete);
    bool ready=false,scratch=true;
    for(unsigned fakeFence:{0u,0u,1u}) {
        if(map_ray_completion(true,fakeFence,.01)==MapRayCompletion::Complete){ready=true;scratch=false;}
        check(ready==(fakeFence==1));check(scratch==(fakeFence==0));
    }
    std::printf("map ray completion: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
