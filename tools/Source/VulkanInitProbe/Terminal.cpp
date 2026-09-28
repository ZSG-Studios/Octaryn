#include "vulkan/vk-device-queue.h"
#include <cstdio>
#include <memory>
#include <cstdlib>
using namespace rhi;
using namespace rhi::vk;
namespace {
void check(bool value) { if(!value) std::abort(); }
VkResult waitResult=VK_SUCCESS, resetResult=VK_SUCCESS;
VkResult poolResult=VK_SUCCESS, beginResult=VK_SUCCESS, endResult=VK_SUCCESS, submitResult=VK_SUCCESS;
bool resetLossAfterOom=false;
int released=0, resetCalls=0, beginCalls=0, submitCalls=0;
bool heapAlive=true;
VKAPI_ATTR VkResult VKAPI_CALL createPool(VkDevice,const VkCommandPoolCreateInfo*,const VkAllocationCallbacks*,VkCommandPool* out)
{ *out=reinterpret_cast<VkCommandPool>(new int);return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL destroyPool(VkDevice,VkCommandPool value,const VkAllocationCallbacks*)
{ delete reinterpret_cast<int*>(value); }
VKAPI_ATTR VkResult VKAPI_CALL allocate(VkDevice,const VkCommandBufferAllocateInfo* info,VkCommandBuffer* out)
{ *out=reinterpret_cast<VkCommandBuffer>(info->commandPool);return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL createFence(VkDevice,const VkFenceCreateInfo*,const VkAllocationCallbacks*,VkFence* out)
{ *out=reinterpret_cast<VkFence>(new int);return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL destroyFence(VkDevice,VkFence value,const VkAllocationCallbacks*)
{ delete reinterpret_cast<int*>(value); }
VKAPI_ATTR VkResult VKAPI_CALL resetPool(VkDevice,VkCommandPool,VkCommandPoolResetFlags) {return poolResult;}
VKAPI_ATTR VkResult VKAPI_CALL begin(VkCommandBuffer,const VkCommandBufferBeginInfo*) {++beginCalls;return beginResult;}
VKAPI_ATTR VkResult VKAPI_CALL end(VkCommandBuffer) {return endResult;}
VKAPI_ATTR VkResult VKAPI_CALL resetFence(VkDevice,uint32_t,const VkFence*) {++resetCalls;return resetLossAfterOom && resetCalls>1?VK_ERROR_DEVICE_LOST:resetResult;}
VKAPI_ATTR VkResult VKAPI_CALL waitFence(VkDevice,uint32_t,const VkFence*,VkBool32,uint64_t) {return waitResult;}
VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue,uint32_t,const VkSubmitInfo*,VkFence) {++submitCalls;return submitResult;}
VKAPI_ATTR VkResult VKAPI_CALL idle(VkQueue) {return VK_SUCCESS;}
struct HeapHandle : RefObject {
    ~HeapHandle() override {check(heapAlive);++released;}
};
VulkanApi api()
{
    VulkanApi value{};
    value.vkCreateCommandPool=createPool;value.vkDestroyCommandPool=destroyPool;
    value.vkAllocateCommandBuffers=allocate;value.vkCreateFence=createFence;value.vkDestroyFence=destroyFence;
    value.vkResetCommandPool=resetPool;value.vkBeginCommandBuffer=begin;value.vkEndCommandBuffer=end;
    value.vkResetFences=resetFence;value.vkWaitForFences=waitFence;value.vkQueueSubmit=submit;value.vkQueueWaitIdle=idle;
    return value;
}
void terminal(VkResult waited,VkResult reset,VkResult expected,bool completion)
{
    auto functions=api();waitResult=waited;resetResult=reset;resetCalls=0;released=0;beginCalls=0;submitCalls=0;heapAlive=true;
    {
        VulkanDeviceQueue queue;check(queue.init(functions,reinterpret_cast<VkQueue>(1),0)==SLANG_OK);
        {
            VulkanDeviceQueue::Operation operation(queue);check(operation.begin()==VK_SUCCESS);
            operation.retain(new HeapHandle);
            std::unique_ptr<VulkanDeviceQueue::Operation> sibling;
            if(completion) {
                sibling=std::make_unique<VulkanDeviceQueue::Operation>(queue);
                check(sibling->begin()==VK_SUCCESS);
            }
            check(operation.submit(true)==expected);
            if(completion) {
                // Already leased and newly reacquired slots must both refuse poisoned recording/submission.
                check(sibling->submit(true)==expected);
                VulkanDeviceQueue::Operation retry(queue);check(retry.begin()==expected);
                check(beginCalls==2 && submitCalls==1);
            }
            check(released==(completion?1:0));
            check(resetCalls==(waited==VK_SUCCESS?1:0));
        }
        if(!completion) {
            // A timeout/arbitrary error cannot release potentially live GPU staging.
            queue.retireCompletedResources();check(released==0);
            waitResult=VK_SUCCESS;resetResult=VK_SUCCESS;
            queue.retireCompletedResources();check(released==1);
        }
        // Device destruction releases the upload heap before final queue teardown.
        heapAlive=false;
    }
    check(released==1);
}
void recordingLoss(int stage)
{
    auto functions=api();waitResult=resetResult=VK_SUCCESS;
    poolResult=beginResult=endResult=submitResult=VK_SUCCESS;heapAlive=true;
    VulkanDeviceQueue queue;check(queue.init(functions,reinterpret_cast<VkQueue>(1),0)==SLANG_OK);
    {
        VulkanDeviceQueue::Operation operation(queue);
        if(stage==0)poolResult=VK_ERROR_DEVICE_LOST;
        if(stage==1)beginResult=VK_ERROR_DEVICE_LOST;
        if(stage<2)check(operation.begin()==VK_ERROR_DEVICE_LOST);
        else {
            check(operation.begin()==VK_SUCCESS);
            if(stage==2)endResult=VK_ERROR_DEVICE_LOST;
            if(stage==3)submitResult=VK_ERROR_DEVICE_LOST;
            check(operation.submit(true)==VK_ERROR_DEVICE_LOST);
        }
    }
    poolResult=beginResult=endResult=submitResult=VK_SUCCESS;
    const int priorBegins=beginCalls,priorSubmits=submitCalls;
    VulkanDeviceQueue::Operation retry(queue);check(retry.begin()==VK_ERROR_DEVICE_LOST);
    check(beginCalls==priorBegins && submitCalls==priorSubmits);
}
void lossDominatesResetError()
{
    auto functions=api();waitResult=VK_SUCCESS;resetResult=VK_ERROR_OUT_OF_HOST_MEMORY;
    resetCalls=0;resetLossAfterOom=true;heapAlive=true;
    VulkanDeviceQueue queue;check(queue.init(functions,reinterpret_cast<VkQueue>(1),0)==SLANG_OK);
    for(int i=0;i<2;++i) {
        VulkanDeviceQueue::Operation operation(queue);check(operation.begin()==VK_SUCCESS);
        check(operation.submit(false)==VK_SUCCESS);
    }
    queue.retireCompletedResources();check(resetCalls==2);
    VulkanDeviceQueue::Operation retry(queue);check(retry.begin()==VK_ERROR_DEVICE_LOST);
    resetLossAfterOom=false;resetResult=VK_SUCCESS;
}
}
int main()
{
    terminal(VK_ERROR_DEVICE_LOST,VK_SUCCESS,VK_ERROR_DEVICE_LOST,true);
    terminal(VK_SUCCESS,VK_ERROR_OUT_OF_HOST_MEMORY,VK_ERROR_OUT_OF_HOST_MEMORY,true);
    terminal(VK_SUCCESS,VK_ERROR_DEVICE_LOST,VK_ERROR_DEVICE_LOST,true);
    for(int stage=0;stage<4;++stage)recordingLoss(stage);
    lossDominatesResetError();
    terminal(VK_TIMEOUT,VK_SUCCESS,VK_TIMEOUT,false);
    terminal(VK_ERROR_OUT_OF_HOST_MEMORY,VK_SUCCESS,VK_ERROR_OUT_OF_HOST_MEMORY,false);
    std::puts("PASS terminal Vulkan init: device loss releases before heap teardown; completed/reset-failed releases; timeout/other error retains until completion; sticky failure blocks leased and new work; recording loss poisons queue; device loss dominates prior reset OOM; errors propagate");
}
