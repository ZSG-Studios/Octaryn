#include "Inventory.h"
#include <glaze/glaze.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_set>

namespace octaryn::client::app {
namespace inventory_catalog {
struct Atlas { unsigned north{}, south{}, east{}, west{}, up{}, down{}; };
struct CatalogBlock {
  std::string id, displayName;
  bool placeable{}, sprite{}, requiresGrass{}, requiresSolidBase{};
  std::string fluidKind;
  Atlas atlas;
};
struct Catalog { std::string schema; std::vector<CatalogBlock> blocks; };
struct Hand { std::string id, kind, displayName; unsigned stackSize{}; };
}
using namespace inventory_catalog;
namespace {
bool read_file(const std::filesystem::path& path, std::string& text) {
  std::ifstream file(path,std::ios::binary|std::ios::ate);
  const auto size=file.tellg();
  if (!file || size<=0 || size>1024*1024) return false;
  text.resize(static_cast<std::size_t>(size));file.seekg(0);
  return static_cast<bool>(file.read(text.data(),size));
}
bool valid_key(std::string_view value) {
  return !value.empty() && value.size()<=160 &&
    std::all_of(value.begin(),value.end(),[](unsigned char c) {
      return (c>='a' && c<='z') || (c>='0' && c<='9') || c=='.' || c=='_' || c=='-';
    });
}
std::string folded(std::string_view text) {
  std::string result(text);
  std::transform(result.begin(),result.end(),result.begin(),[](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return result;
}
InventoryCategory category(const CatalogBlock& block) {
  if (block.fluidKind!="none") return InventoryCategory::Fluids;
  if (block.sprite && block.requiresSolidBase) return InventoryCategory::Lighting;
  if (block.sprite || block.requiresGrass || block.id.ends_with(".log") || block.id.ends_with(".leaves"))
    return InventoryCategory::Nature;
  return InventoryCategory::Terrain;
}
}
bool Inventory::load_catalog(const std::filesystem::path& block_path,const std::filesystem::path& hand_path) {
  std::string text;
  Catalog catalog;
  constexpr glz::opts options{.error_on_unknown_keys=false,.error_on_missing_keys=true};
  if (!read_file(block_path,text) || glz::read<options>(catalog,text) ||
      catalog.schema!="octaryn.basegame.blocks.v1" || catalog.blocks.empty() || catalog.blocks.size()>65536 ||
      catalog.blocks.front().id!="octaryn.basegame.block.air" || catalog.blocks.front().placeable) return false;
  std::vector<InventoryBlock> blocks;
  std::unordered_set<std::string> keys;
  for (std::size_t i=0;i<catalog.blocks.size();++i) {
    const auto& block=catalog.blocks[i];
    if (!valid_key(block.id) || block.displayName.empty() || block.displayName.size()>128 ||
        !keys.insert(block.id).second || block.atlas.north>65535 ||
        (block.fluidKind!="none" && block.fluidKind!="water" && block.fluidKind!="lava")) return false;
    if (block.placeable) blocks.push_back({static_cast<std::uint16_t>(i),block.id,block.displayName,
      block.atlas.north,category(block)});
  }
  if (blocks.empty()) return false;
  blocks_=std::move(blocks);
  hand_={};
  Hand hand;
  if (read_file(hand_path,text) && !glz::read<options>(hand,text) && hand.kind=="item" &&
      hand.id=="octaryn.basegame.item.hand" && !hand.displayName.empty() &&
      hand.displayName.size()<=128 && hand.stackSize==1) hand_={hand.id,hand.displayName,"hand",true};
  defaults();
  return true;
}
const InventoryBlock* Inventory::find(std::uint16_t id) const {
  const auto found=std::lower_bound(blocks_.begin(),blocks_.end(),id,
    [](const InventoryBlock& block,std::uint16_t value) { return block.id<value; });
  return found!=blocks_.end() && found->id==id?&*found:nullptr;
}
std::vector<std::uint16_t> Inventory::search(std::string_view query,InventoryCategory filter) const {
  const auto needle=folded(query);
  std::vector<std::uint16_t> result;
  for (const auto& block:blocks_) {
    if (filter!=InventoryCategory::All && filter!=block.category) continue;
    if (needle.empty() || folded(block.name).find(needle)!=std::string::npos ||
        folded(block.key).find(needle)!=std::string::npos) result.push_back(block.id);
  }
  return result;
}
void Inventory::changed() { dirty_=true;++revision_; }
void Inventory::defaults() {
  slots_.fill(0);counts_.fill(0);cursor_={};grant_watermark_=0;drop_watermark_=0;reserved_drop_=0;selected_=0;moving_=-1;
  for (std::size_t i=0;i<std::min(blocks_.size(),static_cast<std::size_t>(HotbarCount));++i)
    {slots_[i]=blocks_[i].id;counts_[i]=StackLimit;}
  changed();
}
bool Inventory::select_hotbar(unsigned index) {
  if (index>=HotbarCount || index==selected_) return false;
  selected_=index;changed();return true;
}
bool Inventory::cycle_hotbar(int delta) {
  const auto index=(static_cast<std::int64_t>(selected_)+delta)%HotbarCount;
  return select_hotbar(static_cast<unsigned>((index+HotbarCount)%HotbarCount));
}
bool Inventory::assign(std::uint16_t block) {
  if(reserved_drop_)return false;
  if (!find(block)) return false;
  cancel_move();
  if (slots_[selected_]==block) return false;
  slots_[selected_]=block;counts_[selected_]=StackLimit;changed();return true;
}
bool Inventory::pick(std::uint16_t block) {
  if (!find(block)) return false;
  for (unsigned i=0;i<HotbarCount;++i) if (slots_[i]==block) return select_hotbar(i);
  return assign(block);
}
bool Inventory::click_slot(unsigned index) {
  if(reserved_drop_)return false;
  if (index>=SlotCount) return false;
  if (!cursor_.count) {
    if (slots_[index]==0) return false;
    moving_=static_cast<int>(index);cursor_={slots_[index],counts_[index]};
    slots_[index]=0;counts_[index]=0;changed();return true;
  }
  if(slots_[index]==cursor_.block) {
    const auto count=std::min(StackLimit-counts_[index],cursor_.count);
    if(!count)return false;
    counts_[index]+=count;cursor_.count-=count;
  } else {std::swap(slots_[index],cursor_.block);std::swap(counts_[index],cursor_.count);}
  if(!cursor_.count){cursor_={};moving_=-1;}
  changed();
  return true;
}
void Inventory::cancel_move() {
  if(reserved_drop_)return;
  if(!cursor_.count)return;
  if(moving_>=0 && slots_[static_cast<unsigned>(moving_)]==0) {
    const auto at=static_cast<unsigned>(moving_);slots_[at]=cursor_.block;counts_[at]=cursor_.count;cursor_={};
  } else cursor_.count=receive(cursor_.block,cursor_.count);
  if(!cursor_.count){cursor_={};moving_=-1;}
  changed();
}
bool Inventory::clear_slot(unsigned index) {
  if(reserved_drop_)return false;
  if (index>=SlotCount) return false;
  cancel_move();if(!slots_[index])return false;
  slots_[index]=0;counts_[index]=0;changed();return true;
}
bool Inventory::sort_backpack() {
  if(reserved_drop_)return false;
  cancel_move();
  const auto previous=slots_;
  std::array<unsigned,SlotCount-HotbarCount> order{};
  for(unsigned i=0;i<order.size();++i)order[i]=i+HotbarCount;
  std::stable_sort(order.begin(),order.end(),[this](unsigned ai,unsigned bi) {
    const auto a=slots_[ai],b=slots_[bi];
    if (!a || !b) return a!=0 && b==0;
    const auto* left=find(a);const auto* right=find(b);
    if (left->category!=right->category) return left->category<right->category;
    return left->name<right->name;
  });
  const auto old_counts=counts_;
  for(unsigned i=0;i<order.size();++i){slots_[i+HotbarCount]=previous[order[i]];counts_[i+HotbarCount]=old_counts[order[i]];}
  if (previous==slots_) return false;
  changed();return true;
}
bool Inventory::take_creative(std::uint16_t block,bool single) {
  if(reserved_drop_)return false;
  if(!find(block) || cursor_.count)return false;
  cursor_={block,single?1u:StackLimit};moving_=-1;changed();return true;
}
bool Inventory::right_click_slot(unsigned index) {
  if(reserved_drop_)return false;
  if(index>=SlotCount)return false;
  if(!cursor_.count) {
    if(!slots_[index])return false;
    cursor_={slots_[index],1};moving_=static_cast<int>(index);
    if(!--counts_[index])slots_[index]=0;
  } else if((!slots_[index] || slots_[index]==cursor_.block) && counts_[index]<StackLimit) {
    slots_[index]=cursor_.block;++counts_[index];
    if(!--cursor_.count){cursor_={};moving_=-1;}
  } else return false;
  changed();return true;
}
InventoryStack Inventory::drop_cursor(bool single) {
  if(reserved_drop_)return {};
  if(!cursor_.count)return {};
  const InventoryStack result{cursor_.block,single?1u:cursor_.count};
  cursor_.count-=result.count;if(!cursor_.count){cursor_={};moving_=-1;}
  changed();return result;
}
std::uint32_t Inventory::receive(std::uint16_t block,std::uint32_t count) {
  if(!find(block))return count;
  const auto before=count;
  for(unsigned pass=0;pass<2;++pass)for(unsigned i=0;i<SlotCount && count;++i) {
    if(pass==0?slots_[i]!=block:slots_[i]!=0)continue;
    const auto moved=std::min(count,StackLimit-counts_[i]);
    if(moved){slots_[i]=block;counts_[i]+=moved;count-=moved;}
  }
  if(before!=count)changed();return count;
}
bool Inventory::credit_grant(std::uint64_t grant,std::uint16_t block,std::uint32_t count) {
  if(!grant || !count || count>64 || !find(block))return false;
  if(grant<=grant_watermark_)return true;
  const auto before=*this;
  if(receive(block,count)){*this=before;return false;}
  grant_watermark_=grant;changed();return true;
}
bool Inventory::reserve_drop(std::uint32_t count) {
  if(reserved_drop_ || !count || count>StackLimit || count>cursor_.count)return false;
  reserved_drop_=count;changed();return true;
}
void Inventory::resolve_drop(bool accepted,std::uint64_t receipt) {
  if(receipt && receipt<=drop_watermark_)return;
  if(!reserved_drop_)return;
  if(accepted){cursor_.count-=reserved_drop_;if(!cursor_.count){cursor_={};moving_=-1;}}
  reserved_drop_=0;if(receipt)drop_watermark_=receipt;changed();
}
}
