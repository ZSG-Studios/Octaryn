#include "vulkan/vk-descriptor-allocator.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <stdexcept>

namespace {
using namespace rhi::vk;
void require(bool pass,const char* message) {if(!pass)throw std::runtime_error(message);}
using Counts=std::map<VkDescriptorType,uint64_t>;
struct Usage { Counts counts;uint32_t inline_bindings{}; };
Usage usage(const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
  Usage result;
  for(const auto& b:bindings) {
    result.counts[b.descriptorType]+=b.descriptorCount;
    result.inline_bindings+=(b.descriptorType==VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT && b.descriptorCount)?1u:0u;
  }
  return result;
}
struct Mock {
  struct Pool {Usage capacity,remaining;uint32_t maximum{},sets{};};
  struct Set {VkDescriptorPool pool;Usage usage;};
  std::map<VkDescriptorPool,Pool> pools;
  std::map<VkDescriptorSetLayout,std::vector<VkDescriptorSetLayoutBinding>> layouts;
  std::map<VkDescriptorSet,Set> sets;
  uintptr_t serial{};
  unsigned allocations{},creations{},over_budget_calls{},resets{},frees{};
  VkResult force=VK_SUCCESS;
  VulkanApi api{};
  static Mock* active;
  static VKAPI_ATTR VkResult VKAPI_CALL create(VkDevice,const VkDescriptorPoolCreateInfo* info,const VkAllocationCallbacks*,VkDescriptorPool* handle) {
    auto& m=*active;Pool pool;pool.maximum=info->maxSets;
    for(uint32_t i=0;i<info->poolSizeCount;++i)pool.capacity.counts[info->pPoolSizes[i].type]+=info->pPoolSizes[i].descriptorCount;
    if(info->pNext)pool.capacity.inline_bindings=static_cast<const VkDescriptorPoolInlineUniformBlockCreateInfo*>(info->pNext)->maxInlineUniformBlockBindings;
    pool.remaining=pool.capacity;*handle=reinterpret_cast<VkDescriptorPool>(++m.serial);
    m.pools.emplace(*handle,pool);++m.creations;return VK_SUCCESS;
  }
  static VKAPI_ATTR VkResult VKAPI_CALL allocate(VkDevice,const VkDescriptorSetAllocateInfo* info,VkDescriptorSet* handle) {
    auto& m=*active;++m.allocations;
    if(m.force!=VK_SUCCESS) {const auto result=m.force;m.force=VK_SUCCESS;return result;}
    require(info->descriptorSetCount==1,"actual allocator requests one set");
    auto& pool=m.pools.at(info->descriptorPool);
    const auto need=usage(m.layouts.at(info->pSetLayouts[0]));
    bool fits=pool.sets<pool.maximum && need.inline_bindings<=pool.remaining.inline_bindings;
    for(auto [type,count]:need.counts)fits&=count<=pool.remaining.counts[type];
    if(!fits) {++m.over_budget_calls;return VK_ERROR_OUT_OF_POOL_MEMORY;}
    ++pool.sets;pool.remaining.inline_bindings-=need.inline_bindings;
    for(auto [type,count]:need.counts)pool.remaining.counts[type]-=count;
    *handle=reinterpret_cast<VkDescriptorSet>(++m.serial);
    m.sets.emplace(*handle,Set{info->descriptorPool,need});return VK_SUCCESS;
  }
  static VKAPI_ATTR VkResult VKAPI_CALL free_sets(VkDevice,VkDescriptorPool pool_handle,uint32_t count,const VkDescriptorSet* handles) {
    auto& m=*active;
    for(uint32_t i=0;i<count;++i) {
      auto found=m.sets.find(handles[i]);require(found!=m.sets.end() && found->second.pool==pool_handle,"free owns set");
      auto& pool=m.pools.at(pool_handle);--pool.sets;
      for(auto [type,amount]:found->second.usage.counts)pool.remaining.counts[type]+=amount;
      pool.remaining.inline_bindings+=found->second.usage.inline_bindings;m.sets.erase(found);++m.frees;
    }
    return VK_SUCCESS;
  }
  static VKAPI_ATTR VkResult VKAPI_CALL reset(VkDevice,VkDescriptorPool handle,VkDescriptorPoolResetFlags) {
    auto& m=*active;auto& pool=m.pools.at(handle);pool.sets=0;pool.remaining=pool.capacity;
    std::erase_if(m.sets,[&](const auto& pair){return pair.second.pool==handle;});++m.resets;return VK_SUCCESS;
  }
  static VKAPI_ATTR void VKAPI_CALL destroy(VkDevice,VkDescriptorPool handle,const VkAllocationCallbacks*) {
    auto& m=*active;require(m.pools.erase(handle)==1,"destroy owns pool");
    std::erase_if(m.sets,[&](const auto& pair){return pair.second.pool==handle;});
  }
  Mock() {
    active=this;api.vkCreateDescriptorPool=create;api.vkAllocateDescriptorSets=allocate;
    api.vkFreeDescriptorSets=free_sets;api.vkResetDescriptorPool=reset;api.vkDestroyDescriptorPool=destroy;
    api.m_extendedFeatures.inlineUniformBlockFeatures.inlineUniformBlock=1;
    api.m_extendedFeatures.accelerationStructureFeatures.accelerationStructure=1;
  }
  VkDescriptorSetLayout layout(std::initializer_list<std::pair<VkDescriptorType,uint32_t>> counts) {
    const auto handle=reinterpret_cast<VkDescriptorSetLayout>(++serial);
    auto& bindings=layouts[handle];
    for(auto [type,count]:counts)bindings.push_back({uint32_t(bindings.size()),type,count,VK_SHADER_STAGE_ALL,nullptr});
    return handle;
  }
};
Mock* Mock::active{};
void many_draws() {
  Mock mock;DescriptorSetAllocator allocator;allocator.init(&mock.api);
  const auto layout=mock.layout({{VK_DESCRIPTOR_TYPE_SAMPLER,2},{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,4},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1}});
  constexpr unsigned draws=4096*3;
  for(unsigned frame=0;frame<2;++frame) {
    for(unsigned i=0;i<draws;++i)require(allocator.allocate(layout,mock.layouts.at(layout)).handle!=VK_NULL_HANDLE,"allocate draw descriptors");
    require(mock.creations==24,"reset must reuse existing pools from the first pool");
    require(mock.over_budget_calls==0,"must never call Vulkan on a pool without sufficient descriptors");
    allocator.reset();
  }
  require(mock.allocations==draws*2,"exactly one Vulkan allocation per successful set");
  allocator.close();require(mock.pools.empty() && mock.sets.empty(),"all pools released");
  std::printf("descriptor_allocator_mock draws=%u frames=2 pools=24 over_budget_calls=0\n",draws);
}
void edges() {
  Mock mock;DescriptorSetAllocator allocator;allocator.init(&mock.api);
  const auto full=mock.layout({{VK_DESCRIPTOR_TYPE_SAMPLER,1024}});
  const auto a=allocator.allocate(full,mock.layouts.at(full));require(a.handle!=VK_NULL_HANDLE,"full pool set");
  require(allocator.allocate(full,mock.layouts.at(full)).handle!=VK_NULL_HANDLE,"second full pool set");
  require(mock.creations==2,"two full sets need two pools");allocator.free(a);
  require(allocator.allocate(full,mock.layouts.at(full)).pool==a.pool && mock.creations==2,"free restores exact capacity");
  allocator.reset();
  mock.force=VK_ERROR_FRAGMENTED_POOL;
  require(allocator.allocate(full,mock.layouts.at(full)).handle!=VK_NULL_HANDLE,"unexpected fragmentation advances once");
  require(allocator.pools.front().exhausted,"fragmented pool retired until reset/free");
  const auto before=mock.allocations;mock.force=VK_ERROR_OUT_OF_DEVICE_MEMORY;
  require(!allocator.allocate(full,mock.layouts.at(full)).handle && mock.allocations==before+1,"fatal driver failure must not retry all pools");
  allocator.close();
  const auto oversized=mock.layout({{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,5000},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}});
  require(allocator.allocate(oversized,mock.layouts.at(oversized)).handle!=VK_NULL_HANDLE,"oversized layout grows a fresh pool");
  require(mock.pools.begin()->second.capacity.counts[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER]==5001,"duplicate binding types are summed");
  allocator.close();
  const auto inline_layout=mock.layout({{VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT,4}});
  for(unsigned i=0;i<5;++i)require(allocator.allocate(inline_layout,mock.layouts.at(inline_layout)).handle!=VK_NULL_HANDLE,"inline byte budget");
  require(allocator.pools.size()==2,"inline bytes exhaust before binding count");allocator.close();
  const auto empty=mock.layout({});
  for(unsigned i=0;i<4097;++i)require(allocator.allocate(empty,mock.layouts.at(empty)).handle!=VK_NULL_HANDLE,"empty descriptor set maxSets budget");
  require(allocator.pools.size()==2,"maxSets enforced without descriptor counts");
  require(mock.over_budget_calls==0,"all capacity edges checked before Vulkan");allocator.close();
  const auto unsupported=mock.layout({{VK_DESCRIPTOR_TYPE_MAX_ENUM,1}});
  const auto calls=mock.allocations;
  require(!allocator.allocate(unsupported,mock.layouts.at(unsupported)).handle && calls==mock.allocations,"unsupported type fails before Vulkan");
  allocator.close();
}
}
int main() {
  try {many_draws();edges();std::puts("descriptor_allocator=passed mock_vulkan=1 gpu_devices_created=0");return 0;}
  catch(const std::exception& error){std::fprintf(stderr,"descriptor_allocator=failed %s\n",error.what());return 1;}
}
