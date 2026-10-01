#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <memory_resource>
#include <new>

namespace octaryn::client::rendering {
// The retained table needs node and bucket size classes, all warmed at startup.
class ItemHistoryMemory final:public std::pmr::memory_resource {
  struct Block {Block* allocated{};Block* free{};};
  struct Pool {
    std::size_t bytes{},alignment{},prefix{},allocation_alignment{};
    Block* allocated{};Block* free{};
  };
  std::array<Pool,8> pools_{};
  std::pmr::memory_resource* upstream_;
  std::size_t bytes_{},blocks_{};
  void* do_allocate(std::size_t bytes,std::size_t alignment) override {
    if(!alignment || (alignment&(alignment-1)) || alignment>4096)throw std::bad_alloc();
    auto* pool=static_cast<Pool*>(nullptr);
    for(auto& entry:pools_)if(entry.bytes==bytes && entry.alignment==alignment) {pool=&entry;break;}
    if(!pool)for(auto& entry:pools_)if(!entry.alignment) {
      entry.bytes=bytes;entry.alignment=alignment;
      entry.allocation_alignment=std::max(alignment,alignof(Block));
      entry.prefix=(sizeof(Block)+alignment-1)/alignment*alignment;pool=&entry;break;
    }
    if(!pool)throw std::bad_alloc();
    Block* block=pool->free;
    if(block)pool->free=block->free;
    else {
      constexpr std::size_t limit=8*1024*1024;
      if(blocks_>=20008 || bytes>limit || pool->prefix>limit-bytes || bytes_>limit-bytes-pool->prefix)
        throw std::bad_alloc();
      auto* memory=upstream_->allocate(pool->prefix+bytes,pool->allocation_alignment);
      block=new(memory) Block{pool->allocated,nullptr};pool->allocated=block;
      bytes_+=pool->prefix+bytes;++blocks_;
    }
    return reinterpret_cast<std::byte*>(block)+pool->prefix;
  }
  void do_deallocate(void* pointer,std::size_t bytes,std::size_t alignment) override {
    for(auto& pool:pools_)if(pool.bytes==bytes && pool.alignment==alignment) {
      auto* block=reinterpret_cast<Block*>(static_cast<std::byte*>(pointer)-pool.prefix);
      block->free=pool.free;pool.free=block;return;
    }
    std::terminate();
  }
  bool do_is_equal(const std::pmr::memory_resource& other)const noexcept override {return this==&other;}
public:
  explicit ItemHistoryMemory(std::pmr::memory_resource* upstream=std::pmr::new_delete_resource()):upstream_(upstream) {}
  ~ItemHistoryMemory() override {
    for(auto& pool:pools_)for(auto* block=pool.allocated;block;) {
      auto* next=block->allocated;
      upstream_->deallocate(block,pool.prefix+pool.bytes,pool.allocation_alignment);block=next;
    }
  }
  ItemHistoryMemory(const ItemHistoryMemory&)=delete;
  ItemHistoryMemory& operator=(const ItemHistoryMemory&)=delete;
  std::size_t bytes()const {return bytes_;}
};
}
