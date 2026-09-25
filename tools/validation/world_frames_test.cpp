// These generated doubles are intentionally not linked to or ABI-compatible with RHI.
#include "FenceTestDoubles.h"
#include "WorldFramesUnderTest.h"
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
unsigned assertions{};
void require(bool condition,const char* message) {
    ++assertions;if(!condition)throw std::runtime_error(message);
}
struct Fence final:rhi::IFence {
    std::uint64_t completed{};
    int result=SLANG_OK;
    unsigned reads{};
    int getCurrentValue(std::uint64_t* out) override {++reads;*out=completed;return result;}
};
struct Device final:rhi::IDevice {
    Fence fence;
    unsigned waits{},creates{};
    int create_result=SLANG_OK,wait_result=SLANG_OK,post_result=SLANG_OK;
    bool advance{};
    std::uint64_t after_wait{},requested{},timeout{};
    int createFence(const rhi::FenceDesc&,rhi::IFence** out) override {
        ++creates;if(SLANG_SUCCEEDED(create_result))*out=&fence;return create_result;
    }
    int waitForFences(unsigned count,rhi::IFence** fences,const std::uint64_t* values,
                      bool all,std::uint64_t ns) override {
        require(count==1 && fences && *fences==&fence && all,"unexpected fence wait arguments");
        ++waits;requested=*values;timeout=ns;
        if(advance)fence.completed=after_wait;
        fence.result=post_result;return wait_result;
    }
};
struct Queue final:rhi::ICommandQueue {
    struct Submission {std::uint64_t value;unsigned commands;rhi::IFence* fence;};
    std::vector<Submission> submissions;
    int result=SLANG_OK;
    int submit(const rhi::SubmitDesc& desc) override {
        require(desc.signalFenceCount==1 && desc.signalFences && desc.signalFenceValues,
                "submission must signal exactly one fence");
        require(desc.commandBufferCount==0 || (desc.commandBufferCount==1 && desc.commandBuffers &&
                *desc.commandBuffers),"invalid command submission");
        submissions.push_back({*desc.signalFenceValues,desc.commandBufferCount,*desc.signalFences});
        return result;
    }
};
struct Fixture {
    Device device;
    Queue queue;
    rhi::ICommandBuffer command;
    octaryn::client::rendering::WorldFrames frames;
    Fixture() {require(frames.initialize(&device,2),"initialization failed");}
    ~Fixture() {
        // Only cleanup: don't let destructor retries obscure each scenario's observations.
        device.fence.result=SLANG_OK;device.fence.completed=UINT64_MAX-1;
    }
    bool submit(unsigned slot) {return frames.submit(&queue,&command,slot);}
    void retained(unsigned slot) {
        auto submissions=queue.submissions.size();
        require(!submit(slot),"incomplete slot was released for reuse");
        require(queue.submissions.size()==submissions,"pending slot reached queue submission");
    }
};

void stale_wake() {
    Fixture f;require(f.submit(0),"initial submit failed");
    require(f.queue.submissions.back().value==1,"initial signal wrong");
    require(!f.frames.wait(0,7),"successful stale event wake counted as completion");
    require(f.device.waits==1 && f.device.fence.reads==2,"wait must query before and after wake");
    require(f.device.requested==1 && f.device.timeout==7000000,"timeout or target conversion wrong");
    f.retained(0);
    f.device.fence.completed=1;
    require(f.frames.wait(0,0),"observed completion not accepted");
    require(f.device.waits==1,"completed fence must not register another event wait");
    require(f.submit(0),"completed slot did not become reusable");
    require(f.queue.submissions.back().value==2,"signal must remain monotonic");
    // Reproduce a count-phase value waking an emit-phase wait on the same fence.
    require(!f.frames.wait(0,0),"older nonzero completed value released newer submission");
    f.retained(0);
    f.device.advance=true;f.device.after_wait=2;
    require(f.frames.wait(0,UINT64_MAX),"real completion after blocking wait rejected");
    require(f.device.timeout==UINT64_MAX,"infinite timeout was multiplied/overflowed");
    require(f.submit(0),"post-wait completion did not release slot");
}

void failures_retain_pending() {
    for(unsigned scenario=0;scenario<5;++scenario) {
        Fixture f;require(f.submit(0),"initial submit failed");
        if(scenario==0)f.device.fence.completed=UINT64_MAX;
        if(scenario==1) {f.device.advance=true;f.device.after_wait=UINT64_MAX;}
        if(scenario==2)f.device.fence.result=SLANG_FAIL;
        if(scenario==3)f.device.post_result=SLANG_FAIL;
        if(scenario==4)f.device.wait_result=SLANG_E_TIME_OUT;
        require(!f.frames.wait(0,1),"removal/query failure/timeout incorrectly completed slot");
        require(f.device.waits==unsigned(scenario!=0 && scenario!=2),"pre-query failure still waited");
        f.retained(0);
        f.device.fence.result=SLANG_OK;f.device.fence.completed=1;
        require(f.frames.wait(0,0),"failed wait discarded pending value instead of retaining it");
        require(f.submit(0),"recovered slot not reusable");
    }
}

void markers_cover_slots() {
    Fixture f;require(f.submit(0) && f.submit(1),"two-slot submission failed");
    f.device.fence.completed=2;
    require(!f.frames.synchronize(&f.queue,3),"old slot completion counted as later marker completion");
    auto marker=f.queue.submissions.back();
    require(marker.value==3 && marker.commands==0 && marker.fence==&f.device.fence,
            "synchronization must submit a fence-only queue marker after both slots");
    require(f.device.requested==3,"synchronization waited for old slot instead of marker");
    f.retained(0);f.retained(1);
    // A later marker covers all earlier submissions, including the failed-wait marker.
    f.device.advance=true;f.device.after_wait=4;
    require(f.frames.synchronize(&f.queue,3),"completed queue marker rejected");
    require(f.queue.submissions.back().value==4,"retry marker not monotonic");
    auto reads=f.device.fence.reads;
    require(f.frames.wait(0,0) && f.frames.wait(1,0),"marker did not clear both pending slots");
    require(f.device.fence.reads==reads,"covered slots still queried their old pending fence");
    require(f.submit(0) && f.submit(1),"marker did not release both slots");
    require(f.queue.submissions.back().value==6,"post-marker signal sequence wrong");
}

void marker_failure_preserves_slots() {
    Fixture f;require(f.submit(0) && f.submit(1),"two-slot submission failed");
    f.queue.result=SLANG_FAIL;
    require(!f.frames.synchronize(&f.queue,1),"failed marker submission counted as completion");
    require(f.device.waits==0,"failed marker submission still waited");
    f.retained(0);f.retained(1);
    f.queue.result=SLANG_OK;
    f.device.advance=true;f.device.after_wait=3;
    require(f.frames.synchronize(&f.queue,1),"marker retry failed");
    require(f.queue.submissions.back().value==3,"failed submit advanced signal sequence");
}

void initialization_and_nulls() {
    Device device;Queue queue;rhi::ICommandBuffer command;
    octaryn::client::rendering::WorldFrames frames;
    require(frames.synchronize(nullptr,0),"empty fence/queue is not an empty synchronization");
    require(!frames.synchronize(&queue,0),"existing queue without fence falsely synchronized");
    require(queue.submissions.empty(),"uninitialized synchronization submitted a marker");
    require(!frames.initialize(&device,0) && !frames.initialize(&device,3),"invalid slot count accepted");
    require(device.creates==0,"invalid slot count created fence");
    device.create_result=SLANG_FAIL;
    require(!frames.initialize(&device,1),"failed fence creation accepted");
    require(!frames.synchronize(&queue,0),"failed fence creation permits false synchronization");
    device.create_result=SLANG_OK;
    require(frames.initialize(&device,1),"initialization retry failed");
    require(frames.count()==1 && frames.slot(123)==0,"one-slot configuration wrong");
    require(!frames.initialize(&device,2),"double initialization accepted");
    require(!frames.synchronize(nullptr,0),"fence without queue synchronized");
    require(!frames.wait(1,0) && !frames.submit(&queue,&command,1),"invalid slot accepted");
    require(!frames.submit(&queue,nullptr,0),"null command accepted");
    require(frames.wait(0,0) && device.fence.reads==0,"unused slot queried fence");
}

void drain_requires_all_slots() {
    Fixture f;require(f.submit(0) && f.submit(1),"two-slot submission failed");
    f.device.fence.completed=1;
    require(!f.frames.drain(2),"drain accepted incomplete second slot");
    require(f.device.requested==2,"drain did not visit second slot");
    f.retained(1);
    require(f.submit(0),"drain did not clear completed first slot");
    f.device.fence.completed=3;
    require(f.frames.drain(0),"completed drain rejected");
    require(f.submit(0) && f.submit(1),"successful drain left slots pending");
}
}

int main() {
    try {
        stale_wake();failures_retain_pending();markers_cover_slots();
        marker_failure_preserves_slots();initialization_and_nulls();drain_requires_all_slots();
        std::printf("world_frames=passed assertions=%u generated_api_doubles=1 "
                    "real_rhi_integration=0 gpu_runtime=0\n",assertions);
        return 0;
    } catch(const std::exception& error) {
        std::fprintf(stderr,"world_frames=failed assertions=%u reason=%s\n",assertions,error.what());
        return 1;
    }
}
