#include "Inventory.h"
#include <glaze/glaze.hpp>
#include <algorithm>
#include <fstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace octaryn::client::app {
namespace {
bool flush_file(const std::filesystem::path& path) {
#ifdef _WIN32
  HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)return false;
  const bool flushed=FlushFileBuffers(file)!=0;const bool closed=CloseHandle(file)!=0;return flushed && closed;
#else
  const int file=::open(path.c_str(),O_WRONLY);if(file<0)return false;
  const bool flushed=::fsync(file)==0;const bool closed=::close(file)==0;return flushed && closed;
#endif
}
bool flush_parent(const std::filesystem::path& path) {
#ifdef _WIN32
  (void)path;return true; // MoveFileExW uses WRITE_THROUGH below.
#else
  const auto parent=path.parent_path().empty()?std::filesystem::path("."):path.parent_path();
  const int directory=::open(parent.c_str(),O_RDONLY|O_DIRECTORY);if(directory<0)return false;
  const bool flushed=::fsync(directory)==0;const bool closed=::close(directory)==0;return flushed && closed;
#endif
}
}
namespace inventory_persistence {
struct Palette {
  std::string schema{"octaryn.client.build-palette.v2"};
  unsigned selected{};
  std::vector<std::string> slots;
  std::vector<std::uint32_t> counts;
  std::string cursor;
  std::uint32_t cursor_count{};
  std::uint64_t grant_watermark{};
  std::uint32_t reserved_drop{};
  std::uint64_t drop_watermark{};
};
}
bool Inventory::load(const std::filesystem::path& path) {
  std::ifstream file(path,std::ios::binary|std::ios::ate);
  const auto size=file.tellg();
  if (!file || size<=0 || size>32768) return false;
  std::string text(static_cast<std::size_t>(size),'\0');file.seekg(0);
  if (!file.read(text.data(),size)) return false;
  inventory_persistence::Palette palette;
  constexpr glz::opts options{.error_on_missing_keys=false};
  if (glz::read<options>(palette,text) ||
      (palette.schema!="octaryn.client.build-palette.v1" && palette.schema!="octaryn.client.build-palette.v2") ||
      palette.selected>=HotbarCount || palette.slots.size()!=SlotCount ||
      (palette.schema=="octaryn.client.build-palette.v2" && palette.counts.size()!=SlotCount) ||
      (!palette.counts.empty() && palette.counts.size()!=SlotCount)) return false;
  std::array<std::uint16_t,SlotCount> slots{};
  std::array<std::uint32_t,SlotCount> counts{};
  for (unsigned i=0;i<SlotCount;++i) {
    if (palette.slots[i].empty()) {if(!palette.counts.empty() && palette.counts[i])return false;continue;}
    const auto found=std::find_if(blocks_.begin(),blocks_.end(),
      [&](const InventoryBlock& block) { return block.key==palette.slots[i]; });
    if (found==blocks_.end()) return false;
    slots[i]=found->id;
    counts[i]=palette.counts.empty()?StackLimit:palette.counts[i];
    if(!counts[i] || counts[i]>StackLimit)return false;
  }
  InventoryStack cursor;
  if(!palette.cursor.empty()) {
    const auto found=std::find_if(blocks_.begin(),blocks_.end(),[&](const InventoryBlock& b){return b.key==palette.cursor;});
    if(found==blocks_.end() || !palette.cursor_count || palette.cursor_count>StackLimit)return false;
    cursor={found->id,palette.cursor_count};
  } else if(palette.cursor_count)return false;
  if(palette.reserved_drop>StackLimit || palette.reserved_drop>cursor.count)return false;
  slots_=slots;counts_=counts;cursor_=cursor;grant_watermark_=palette.grant_watermark;reserved_drop_=palette.reserved_drop;
  selected_=palette.selected;moving_=-1;dirty_=false;++revision_;
  drop_watermark_=palette.drop_watermark;
  return true;
}
bool Inventory::save_if_changed(const std::filesystem::path& path) {
  if (!dirty_) return true;
  if (path.empty() || blocks_.empty()) return false;
  inventory_persistence::Palette palette;palette.selected=selected_;
  palette.counts.assign(counts_.begin(),counts_.end());
  palette.grant_watermark=grant_watermark_;
  palette.reserved_drop=reserved_drop_;
  palette.drop_watermark=drop_watermark_;
  if(const auto* held=find(cursor_.block)){palette.cursor=held->key;palette.cursor_count=cursor_.count;}
  for (const auto id:slots_) {
    const auto* block=find(id);
    palette.slots.push_back(block?block->key:"");
  }
  std::string text;
  if (glz::write_json(palette,text)) return false;
  { // Returning a held stack to its original slot does not rewrite unchanged saves.
    std::ifstream previous(path,std::ios::binary|std::ios::ate);
    if(previous && previous.tellg()==static_cast<std::streamoff>(text.size())) {
      std::string existing(text.size(),'\0');previous.seekg(0);
      if(previous.read(existing.data(),static_cast<std::streamsize>(existing.size())) && existing==text) {
        previous.close();
        if(!flush_file(path) || !flush_parent(path))return false;
        dirty_=false;return true;
      }
    }
  }
  std::error_code error;
  if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(),error);
  if (error) return false;
  auto temporary=path;temporary+=".tmp";
  {
    std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
    if (!file || !file.write(text.data(),static_cast<std::streamsize>(text.size()))) return false;
    file.close();
    if (!file) return false;
  }
  if(!flush_file(temporary))return false;
#ifdef _WIN32
  if (!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) return false;
#else
  std::filesystem::rename(temporary,path,error);
  if (error) return false;
#endif
  if(!flush_parent(path))return false;
  dirty_=false;return true;
}
}
