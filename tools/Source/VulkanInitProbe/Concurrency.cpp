#include "vulkan/vk-device-queue.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <thread>
using namespace rhi;
using namespace rhi::vk;
using namespace std::chrono_literals;
namespace {
void check(bool condition) { if (!condition) std::abort(); }
struct Pool { std::atomic<int> state{0}; }; // idle, recording, ended, submitted
struct MockFence { std::atomic<bool> signaled{false}; };
std::mutex pendingMutex;
std::deque<std::pair<Pool*, MockFence*>> pending;
std::atomic<int> queueUsers{0}, livePools{0}, destroyedResources{0}, submissions{0};
std::atomic<bool> stopGpu{false}, failSubmit{false}, pauseGpu{false};
std::atomic<int> pendingCount{0};
struct HostCall {
    HostCall() { check(queueUsers.fetch_add(1) == 0); std::this_thread::yield(); }
    ~HostCall() { check(queueUsers.fetch_sub(1) == 1); }
};
VKAPI_ATTR VkResult VKAPI_CALL createPool(VkDevice, const VkCommandPoolCreateInfo*, const VkAllocationCallbacks*, VkCommandPool* out)
{ *out = reinterpret_cast<VkCommandPool>(new Pool); ++livePools; return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL destroyPool(VkDevice, VkCommandPool pool, const VkAllocationCallbacks*)
{ auto* p = reinterpret_cast<Pool*>(pool); check(p->state != 3); delete p; --livePools; }
VKAPI_ATTR VkResult VKAPI_CALL allocate(VkDevice, const VkCommandBufferAllocateInfo* info, VkCommandBuffer* out)
{ *out = reinterpret_cast<VkCommandBuffer>(info->commandPool); return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL createFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* out)
{ *out = reinterpret_cast<VkFence>(new MockFence); return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL destroyFence(VkDevice, VkFence fence, const VkAllocationCallbacks*)
{ delete reinterpret_cast<MockFence*>(fence); }
VKAPI_ATTR VkResult VKAPI_CALL resetPool(VkDevice, VkCommandPool pool, VkCommandPoolResetFlags)
{ auto* p = reinterpret_cast<Pool*>(pool); check(p->state != 3); p->state = 0; return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL begin(VkCommandBuffer commands, const VkCommandBufferBeginInfo*)
{ check(reinterpret_cast<Pool*>(commands)->state.exchange(1) == 0); return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL end(VkCommandBuffer commands)
{ check(reinterpret_cast<Pool*>(commands)->state.exchange(2) == 1); return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL resetFence(VkDevice, uint32_t count, const VkFence* fences)
{ for (uint32_t i=0;i<count;++i) check(reinterpret_cast<MockFence*>(fences[i])->signaled.exchange(false)); return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL waitFence(VkDevice, uint32_t count, const VkFence* fences, VkBool32, uint64_t timeout)
{
    for (uint32_t i=0;i<count;++i) {
        auto* fence = reinterpret_cast<MockFence*>(fences[i]);
        while (!fence->signaled) { if (!timeout) return VK_TIMEOUT; std::this_thread::yield(); }
    }
    return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue, uint32_t count, const VkSubmitInfo* infos, VkFence fence)
{
    HostCall call;
    if (failSubmit.exchange(false)) return VK_ERROR_OUT_OF_HOST_MEMORY;
    check(count == 1 && infos->commandBufferCount == 1);
    auto* pool = reinterpret_cast<Pool*>(infos->pCommandBuffers[0]);
    check(pool->state.exchange(3) == 2);
    ++submissions; ++pendingCount;
    std::lock_guard lock(pendingMutex);
    pending.emplace_back(pool, reinterpret_cast<MockFence*>(fence));
    return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL present(VkQueue, const VkPresentInfoKHR*) { HostCall call; return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL idle(VkQueue)
{ HostCall call; while(pendingCount) std::this_thread::yield(); return VK_SUCCESS; }
struct ResourceProbe : RefObject {
    VulkanDeviceQueue* queue;
    explicit ResourceProbe(VulkanDeviceQueue* q) : queue(q) {}
    ~ResourceProbe() override { ++destroyedResources; queue->retireCompletedResources(); }
};
VulkanApi api()
{
    VulkanApi value{};
    value.vkCreateCommandPool=createPool; value.vkDestroyCommandPool=destroyPool;
    value.vkAllocateCommandBuffers=allocate; value.vkCreateFence=createFence; value.vkDestroyFence=destroyFence;
    value.vkResetCommandPool=resetPool; value.vkBeginCommandBuffer=begin; value.vkEndCommandBuffer=end;
    value.vkResetFences=resetFence; value.vkWaitForFences=waitFence; value.vkQueueSubmit=submit;
    value.vkQueuePresentKHR=present; value.vkQueueWaitIdle=idle;
    return value;
}
}
int main()
{
    std::thread gpu([] {
        while (!stopGpu || pendingCount) {
            if (pauseGpu) { std::this_thread::yield(); continue; }
            std::pair<Pool*,MockFence*> work{};
            { std::lock_guard lock(pendingMutex); if (!pending.empty()) { work=pending.front(); pending.pop_front(); } }
            if (!work.first) { std::this_thread::yield(); continue; }
            std::this_thread::sleep_for(20us);
            work.first->state=0; work.second->signaled=true; --pendingCount;
        }
    });
    auto functions=api();
    {
        VulkanDeviceQueue queue;
        check(queue.init(functions,reinterpret_cast<VkQueue>(1),0)==SLANG_OK);
        check(livePools == 8);
        // Aborted recording and rejected submit must release the lease and retained resources.
        { VulkanDeviceQueue::Operation operation(queue); check(operation.begin()==VK_SUCCESS); operation.retain(new ResourceProbe(&queue)); }
        { VulkanDeviceQueue::Operation operation(queue); check(operation.begin()==VK_SUCCESS); operation.retain(new ResourceProbe(&queue)); failSubmit=true; check(operation.submit(true)==VK_ERROR_OUT_OF_HOST_MEMORY); }
        check(destroyedResources == 2);
        // Async retained handles cannot retire before their fence; later polling releases them.
        pauseGpu=true;
        { VulkanDeviceQueue::Operation operation(queue); check(operation.begin()==VK_SUCCESS); operation.retain(new ResourceProbe(&queue)); check(operation.submit()==VK_SUCCESS); }
        queue.retireCompletedResources(); check(destroyedResources == 2);
        pauseGpu=false; check(queue.waitForIdle()==VK_SUCCESS); queue.retireCompletedResources();
        check(destroyedResources == 3);
        // More recorders than leases, plus present/idle/retirement, must never share recording or host queue access.
        std::atomic<bool> done{false};
        std::thread maintenance([&] { while(!done) { queue.retireCompletedResources(); check(queue.present(nullptr)==VK_SUCCESS); check(queue.waitForIdle()==VK_SUCCESS); } });
        std::vector<std::thread> workers;
        for (int t=0;t<12;++t) workers.emplace_back([&] {
            for (int i=0;i<40;++i) {
                VulkanDeviceQueue::Operation operation(queue); check(operation.begin()==VK_SUCCESS);
                std::this_thread::yield(); operation.retain(new ResourceProbe(&queue));
                check(operation.submit(true)==VK_SUCCESS);
            }
        });
        for(auto& worker:workers) worker.join();
        done=true; maintenance.join();
        check(destroyedResources==483 && pendingCount==0 && livePools==8);
        // No subsequent renderer frame is required to destroy synchronous init resources.
    }
    check(livePools==0 && queueUsers==0);
    stopGpu=true; gpu.join();
    std::printf("PASS private Vulkan init: 8 slots, 12 workers, %d submissions, 483 releases, failed submit, abandoned record, fence lifetime, reentrant retirement, present/idle exclusion\n", submissions.load());
}
