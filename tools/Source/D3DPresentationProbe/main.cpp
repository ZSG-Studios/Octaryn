#include <Windows.h>
#include "../../../build/dependencies/slang-rhi/src/d3d/d3d-presentation.h"
#include <d3d12-surface-retirement.h>
#include <d3d12-surface-acquisition.h>
#include <d3d12-surface-completion.h>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
unsigned retirementChecks{};
unsigned acquisitionChecks{};
unsigned completionChecks{};
void completionCheck(bool valid,const char* message) {
    ++completionChecks;if(!valid)throw std::runtime_error(message);
}
void completion() {
    for(std::uint64_t target:{std::uint64_t{0},std::uint64_t{10},std::uint64_t{1}<<33}) {
        unsigned arms{},waits{},aborts{};
        const auto result=rhi::d3d_wait_image_completion(target,[&]{return target;},
            [&](std::uint64_t){++arms;return false;},[&](std::uint32_t){++waits;return DWORD(WAIT_TIMEOUT);},
            []{return std::uint64_t{0};},[&]{++aborts;});
        completionCheck(result.admitted && arms==0 && waits==0 && aborts==0,
            "completed image blocked on lost/reset event");
    }
    for(DWORD notification:{DWORD(WAIT_OBJECT_0),DWORD(WAIT_TIMEOUT)}) {
        std::uint64_t completed=7,clock=100;unsigned arms{},aborts{};
        const auto result=rhi::d3d_wait_image_completion(8,[&]{return completed;},
            [&](std::uint64_t target){++arms;completionCheck(target==8,"wrong image retirement armed");return true;},
            [&](std::uint32_t timeout){completionCheck(timeout>0 && timeout<=20,"unbounded fence notification wait");
                clock+=timeout;if(clock>=160)completed=8;return notification;},
            [&]{return clock;},[&]{++aborts;});
        completionCheck(result.admitted && result.waits==3 && result.after==8 && aborts==0,
            "event notification overrode actual pending fence");
        completionCheck(arms==(notification==WAIT_OBJECT_0?3u:1u),"notification rearming differs");
    }
    for(unsigned failure=0;failure<7;++failure) {
        std::uint64_t clock=100,completed=failure==0?UINT64_MAX:7;unsigned aborts{},arms{};
        const auto result=rhi::d3d_wait_image_completion(8,[&]{return completed;},
            [&](std::uint64_t){++arms;if(failure==6)completed=8;return failure!=1;},
            [&](std::uint32_t timeout){if(failure!=5)clock+=timeout;
                if(failure==3)clock=99;
                if(failure==4)completed=UINT64_MAX;
                return failure==2?DWORD(WAIT_FAILED):DWORD(WAIT_TIMEOUT);},
            [&]{return clock;},[&]{++aborts;});
        completionCheck(result.admitted==(failure==6) && aborts==(failure==6?0u:1u),
            "failed/removed fence retained usable state or completion race failed");
        if(failure==0)completionCheck(arms==0 && result.reason==1,"device removed sentinel admitted");
        if(failure==5)completionCheck(result.waits==4096 && result.reason==6,"nonadvancing clock unbounded");
    }
    std::uint64_t clock=0;unsigned aborts{};
    const auto timed=rhi::d3d_wait_image_completion(4,[]{return std::uint64_t{3};},[](std::uint64_t){return true;},
        [&](std::uint32_t timeout){clock+=timeout;return DWORD(WAIT_TIMEOUT);},[&]{return clock;},[&]{++aborts;});
    completionCheck(!timed.admitted && clock==2000 && timed.waits==100 && timed.reason==3 && aborts==1,
        "pending image exceeded original two-second deadline");
    // Reproduce old event-only admission failing after completion, then verify the
    // exact new helper already covered above admits without touching that event.
    completionCheck(!rhi::d3d_wait_image_event([](std::uint32_t){return DWORD(WAIT_TIMEOUT);},
        []{return true;},[]{}),"lost-notification counterexample no longer exercises old helper");
    std::uint64_t imageTargets[2]={7,9};unsigned waits{};
    for(unsigned image=0;image<2;++image) {
        const auto imageResult=rhi::d3d_wait_image_completion(imageTargets[image],[]{return std::uint64_t{7};},
            [](std::uint64_t){return false;},[&](std::uint32_t){++waits;return DWORD(WAIT_FAILED);},
            []{return std::uint64_t{0};},[]{});
        completionCheck(imageResult.admitted==(image==0),"different image incorrectly used global retirement");
    }
    completionCheck(waits==0,"completed image waited on newer other-image retirement");
    for(auto prior:{UINT64_MAX-1,UINT64_MAX}) {
        unsigned signals{},aborts{};
        const auto retired=rhi::d3d_retire_image(prior,[](std::uint64_t){return 0;},[]{return 0;},
            [&](std::uint64_t){++signals;return 0;},[&]{++aborts;});
        completionCheck(retired<0 && signals==0 && aborts==1,"reserved removed-device sentinel as normal fence target");
    }
}
void acquisition() {
    for (DWORD status : {DWORD(WAIT_OBJECT_0), DWORD(WAIT_TIMEOUT), DWORD(WAIT_ABANDONED), DWORD(WAIT_FAILED)})
        for (bool resetOk : {false, true}) {
            unsigned resets{}, aborts{};std::uint32_t timeout{};bool configured=true;
            const bool acquired=rhi::d3d_wait_image_event(
                [&](std::uint32_t bounded) {timeout=bounded;return status;},
                [&] {++resets;return resetOk;},[&] {++aborts;configured=false;});
            const bool expected=status==WAIT_OBJECT_0 && resetOk;
            if(acquired!=expected || configured!=expected || aborts!=(expected?0u:1u))
                throw std::runtime_error("failed acquisition retained usable presentation state");
            ++acquisitionChecks;
            if(timeout!=2000 || timeout==INFINITE || resets!=(status==WAIT_OBJECT_0?1u:0u))
                throw std::runtime_error("acquisition is unbounded or resets an incomplete image event");
            ++acquisitionChecks;
        }
}
void check(bool valid,const char* message) {
    ++retirementChecks;
    if(!valid)throw std::runtime_error(message);
}
void retirement() {
    for(std::uint64_t prior:{std::uint64_t{0},std::uint64_t{7},std::uint64_t{1}<<32}) {
        auto value=prior;std::uint64_t registered{},signalled{},completed=prior;std::vector<unsigned> order;bool aborted=false;
        const auto result=rhi::d3d_retire_image(value,
            [&](std::uint64_t reserved) {registered=reserved;order.push_back(1);return std::int32_t{0};},
            [&]() {order.push_back(2);check(registered>completed,"image event retired before current work completed");return std::int32_t{0};},
            [&](std::uint64_t reserved) {signalled=reserved;order.push_back(3);return std::int32_t{0};},
            [&]() {aborted=true;});
        check(result==0 && !aborted,"successful retirement was aborted");
        check(order==std::vector<unsigned>{1,2,3},"image retirement ordering differs");
        check(registered==prior+1 && signalled==registered && value==registered,"image event and queue signal watch different fence values");
        check(completed<registered,"queued signal was treated as CPU completion");
        completed=signalled;check(completed>=registered,"completed current work did not retire image event");
    }
    for(unsigned failed=1;failed<=3;++failed) {
        std::uint64_t value=42;unsigned aborts=0;bool configured=true;std::vector<unsigned> order;
        const auto stage=[&](unsigned index) {order.push_back(index);return index==failed?-std::int32_t(index):std::int32_t{0};};
        const auto result=rhi::d3d_retire_image(value,[&](std::uint64_t){return stage(1);},[&](){return stage(2);},
            [&](std::uint64_t){return stage(3);},[&](){++aborts;configured=false;});
        check(result==-std::int32_t(failed),"retirement failure was swallowed");
        check(order.size()==failed && order.back()==failed,"commands issued after failed register/present/signal");
        check(aborts==1 && !configured,"failed presentation permits reacquisition of unsignalled image");
        check(value==(failed==1?42u:43u),"reserved completion value was reused after presentation failure");
    }
    auto value=(std::numeric_limits<std::uint64_t>::max)();unsigned calls=0,aborts=0;
    const auto result=rhi::d3d_retire_image(value,[&](std::uint64_t){++calls;return 0;},[&](){++calls;return 0;},
        [&](std::uint64_t){++calls;return 0;},[&](){++aborts;});
    check(result<0 && calls==0 && aborts==1,"fence overflow wrapped or stranded the image");
    check(value==(std::numeric_limits<std::uint64_t>::max)(),"overflow mutated completion state");
    value=0;std::uint64_t signalled{};
    check(rhi::d3d_retire_image(value,[](std::uint64_t){return 0;},[](){return std::int32_t{DXGI_STATUS_OCCLUDED};},
        [&](std::uint64_t next){signalled=next;return 0;},[](){})==0 && signalled==1,
        "nonfailure Present status stranded its acquired image");
}
}

int main() {
    unsigned checks = 0;
    const DXGI_SWAP_EFFECT effects[] = {DXGI_SWAP_EFFECT_DISCARD, DXGI_SWAP_EFFECT_SEQUENTIAL,
        DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, DXGI_SWAP_EFFECT_FLIP_DISCARD};
    for (const auto effect : effects) for (bool vsync : {false, true})
        for (bool supported : {false, true}) {
            const bool flip = effect == DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL || effect == DXGI_SWAP_EFFECT_FLIP_DISCARD;
            const bool expected = flip && !vsync && supported;
            if (rhi::d3d_allow_tearing(effect, vsync, supported) != expected) return 1;
            ++checks;
        }
    for (bool allowed : {false, true}) for (bool vsync : {false, true})
        for (bool fullscreen : {false, true}) {
            const UINT expected = allowed && !vsync && !fullscreen ? DXGI_PRESENT_ALLOW_TEARING : 0;
            if (rhi::d3d_present_flags(allowed, vsync, fullscreen) != expected) return 2;
            ++checks;
        }
    try {retirement();acquisition();completion();}
    catch(const std::exception& error) {std::fprintf(stderr,"d3d_image_retirement failed=%s\n",error.what());return 3;}
    std::printf("d3d_presentation_policy checks=%u retirement_checks=%u acquisition_checks=%u completion_checks=%u passed=1 gpu=0\n", checks,retirementChecks,acquisitionChecks,completionChecks);
    return 0;
}
