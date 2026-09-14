#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace octaryn::client::world_presentation {
// Generation edits a dense working copy. compact() publishes a lossless paged
// representation shared by query and renderer snapshots; reads never expand it.
class ColumnBlocks {
  static constexpr std::size_t PageSize = 4096;
  struct Page {
    std::size_t words{}, palette{};
    unsigned bits{}, word_shift{};
  };
  struct Storage {
    std::size_t size{};
    bool packed{};
    std::vector<std::uint16_t> dense, palette;
    std::vector<Page> pages;
    std::vector<std::uint64_t> words;
  };
  std::shared_ptr<Storage> storage_;

  std::uint16_t read(std::size_t index) const {
    if (!storage_->packed) return storage_->dense[index];
    const auto& page = storage_->pages[index / PageSize];
    if (!page.bits) return storage_->palette[page.palette];
    const auto local = index % PageSize;
    const auto code = static_cast<std::uint16_t>((storage_->words[page.words + (local >> page.word_shift)] >>
        ((local * page.bits) & 63u)) & ((1u << page.bits) - 1u));
    return page.bits == 16 ? code : storage_->palette[page.palette + code];
  }
  std::vector<std::uint16_t>& writable() {
    if (!storage_ || storage_->packed || storage_.use_count()!=1) {
      auto next = std::make_shared<Storage>();
      next->dense = expand();
      next->size = next->dense.size();
      storage_ = std::move(next);
    }
    return storage_->dense;
  }

public:
  class Reference {
    ColumnBlocks& owner_;
    std::size_t index_;
  public:
    Reference(ColumnBlocks& owner, std::size_t index) : owner_(owner), index_(index) {}
    operator std::uint16_t() const { return owner_.read(index_); }
    Reference& operator=(std::uint16_t value) { owner_.writable()[index_] = value; return *this; }
    Reference& operator=(const Reference& value) { return *this = static_cast<std::uint16_t>(value); }
  };
  class ConstIterator {
    const ColumnBlocks* owner_{};
    std::size_t index_{};
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = std::uint16_t;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = value_type;
    ConstIterator() = default;
    ConstIterator(const ColumnBlocks* owner, std::size_t index) : owner_(owner), index_(index) {}
    value_type operator*() const { return (*owner_)[index_]; }
    ConstIterator& operator++() { ++index_; return *this; }
    ConstIterator operator++(int) { auto previous = *this; ++*this; return previous; }
    friend bool operator==(const ConstIterator&, const ConstIterator&) = default;
  };
  ColumnBlocks() = default;
  ColumnBlocks(std::initializer_list<std::uint16_t> values) {
    storage_ = std::make_shared<Storage>();
    storage_->dense = values; storage_->size = values.size();
  }
  std::size_t size() const { return storage_ ? storage_->size : 0; }
  bool empty() const { return size() == 0; }
  std::uint16_t operator[](std::size_t index) const { return read(index); }
  Reference operator[](std::size_t index) { return {*this, index}; }
  void read_range(std::size_t first,std::span<std::uint32_t> output) const {
    assert(first<=size() && output.size()<=size()-first);
    if(output.empty()) return;
    if(!storage_->packed) {
      std::copy_n(storage_->dense.data()+first,output.size(),output.data());return;
    }
    while(!output.empty()) {
      const auto& page=storage_->pages[first/PageSize];
      const auto local=first%PageSize;
      const auto count=std::min(output.size(),PageSize-local);
      if(!page.bits) std::fill_n(output.data(),count,storage_->palette[page.palette]);
      else {
        const auto* words=storage_->words.data()+page.words;
        const auto mask=(1u<<page.bits)-1u;
        if(page.bits==16) {
          for(std::size_t i=0;i<count;++i) {
            const auto at=local+i;
            output[i]=static_cast<std::uint32_t>((words[at>>page.word_shift]>>((at*page.bits)&63u))&mask);
          }
        } else {
          const auto* palette=storage_->palette.data()+page.palette;
          for(std::size_t i=0;i<count;++i) {
            const auto at=local+i;
            output[i]=palette[(words[at>>page.word_shift]>>((at*page.bits)&63u))&mask];
          }
        }
      }
      first+=count;output=output.subspan(count);
    }
  }
  ConstIterator begin() const { return {this, 0}; }
  // Skip whole packed pages whose palettes cannot match (for sparse emitters).
  template<class Predicate,class Visitor> void visit_matching(Predicate match,Visitor visit) const {
    if(!storage_)return;
    for(std::size_t first=0;first<size();first+=PageSize) {
      const auto count=std::min(PageSize,size()-first);
      if(storage_->packed) {
        const auto pageIndex=first/PageSize;
        const auto& page=storage_->pages[pageIndex];
        if(page.bits!=16) {
          const auto end=pageIndex+1<storage_->pages.size()?storage_->pages[pageIndex+1].palette:storage_->palette.size();
          bool possible=false;
          for(auto index=page.palette;index<end;++index)if(match(storage_->palette[index])) {possible=true;break;}
          if(!possible)continue;
        }
      }
      for(std::size_t i=0;i<count;++i) {
        const auto value=read(first+i);
        if(match(value))visit(first+i,value);
      }
    }
  }
  ConstIterator end() const { return {this, size()}; }
  void resize(std::size_t size) { writable().resize(size); storage_->size = size; }
  void pop_back() { writable().pop_back(); --storage_->size; }
  void fill(std::uint16_t value) { auto& values = writable(); std::fill(values.begin(), values.end(), value); }
  void clear() { storage_.reset(); }
  const void* storage_identity() const { return storage_.get(); }
  bool is_compact() const { return storage_ && storage_->packed; }
  std::size_t storage_bytes() const {
    return storage_ ? sizeof(Storage) +
        (storage_->dense.capacity() + storage_->palette.capacity()) * sizeof(std::uint16_t) +
        storage_->pages.capacity() * sizeof(Page) + storage_->words.capacity() * sizeof(std::uint64_t) : 0;
  }
  std::vector<std::uint16_t> expand() const {
    if (!storage_) return {};
    if (!storage_->packed) return storage_->dense;
    std::vector<std::uint16_t> result(size());
    for (std::size_t i = 0; i < size(); ++i) result[i] = read(i);
    return result;
  }
  void compact() {
    if (empty() || storage_->packed) return;
    auto next = std::make_shared<Storage>();
    next->size = size(); next->packed = true;
    next->pages.reserve((size() + PageSize - 1) / PageSize);
    std::vector<int> codes(65536, -1);
    std::vector<std::uint16_t> unique;
    unique.reserve(PageSize);
    const auto& values = storage_->dense;
    for (std::size_t first = 0; first < size(); first += PageSize) {
      const auto count = std::min(PageSize, size() - first);
      unique.clear();
      for (std::size_t i = 0; i < count; ++i) {
        const auto value = values[first + i];
        if (codes[value] < 0) {
          codes[value] = static_cast<int>(unique.size()); unique.push_back(value);
        }
      }
      const unsigned bits = unique.size() == 1 ? 0 : unique.size() <= 2 ? 1 :
          unique.size() <= 4 ? 2 : unique.size() <= 16 ? 4 : unique.size() <= 256 ? 8 : 16;
      const unsigned word_shift = bits <= 1 ? 6 : bits == 2 ? 5 : bits == 4 ? 4 : bits == 8 ? 3 : 2;
      const Page page{next->words.size(), next->palette.size(), bits, word_shift};
      if (bits != 16) next->palette.insert(next->palette.end(), unique.begin(), unique.end());
      if (bits) {
        const auto per_word = 64u / bits;
        next->words.resize(next->words.size() + (count + per_word - 1) / per_word, 0);
        for (std::size_t i = 0; i < count; ++i) {
          const auto value = values[first + i];
          const auto code = bits == 16 ? value : static_cast<std::uint16_t>(codes[value]);
          next->words[page.words + i / per_word] |= static_cast<std::uint64_t>(code) << ((i % per_word) * bits);
        }
      }
      next->pages.push_back(page);
      for (const auto value : unique) codes[value] = -1;
    }
    // Trim spare capacity before this payload becomes long-lived shared storage.
    next->palette.shrink_to_fit(); next->words.shrink_to_fit();
    storage_ = std::move(next);
  }
  friend bool operator==(const ColumnBlocks& a, const ColumnBlocks& b) {
    return a.storage_ == b.storage_ || (a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()));
  }
};
}
