#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace octaryn::client::world_presentation {

struct PredictedBlock {
 std::uint64_t command{}, accepted_revision{};
 std::int32_t x{}, y{}, z{};
 std::uint16_t block{};
 bool accepted{};
};

// Presentation-thread owned. Authoritative columns are never modified here.
class PredictedBlocks {
public:
 static constexpr std::size_t Capacity = 256;
 static int column(int value) { return value / 32 - (value % 32 < 0 ? 1 : 0); }
 bool can_submit() const { return edits_.size() < Capacity; }
 bool add(std::uint64_t command,int x,int y,int z,std::uint16_t block) {
 if(!command || !can_submit() || find(command)!=edits_.end()) return false;
 edits_.push_back({command,0,x,y,z,block,false});
 return true;
 }
 bool resolve(std::uint64_t command,bool accepted,std::uint64_t revision) {
 const auto edit=find(command);
 if(edit==edits_.end()) return false;
 if(!accepted) edits_.erase(edit);
 else {edit->accepted=true;edit->accepted_revision=revision;}
 return true;
 }
 void cover(int x,int z,std::uint64_t revision) {
 std::erase_if(edits_,[&](const auto& edit) {
 return column(edit.x)==x && column(edit.z)==z && edit.accepted &&
 revision>=edit.accepted_revision;
 });
 }
 bool query(int x,int y,int z,std::uint16_t& block) const {
 for(auto edit=edits_.rbegin();edit!=edits_.rend();++edit)
 if(edit->x==x && edit->y==y && edit->z==z) {block=edit->block;return true;}
 return false;
 }
 bool contains_column(int x,int z) const {
 return std::any_of(edits_.begin(),edits_.end(),[&](const auto& edit) {
 return column(edit.x)==x && column(edit.z)==z;
 });
 }
 const std::vector<PredictedBlock>& edits() const { return edits_; }
 void clear() { edits_.clear(); }
private:
 std::vector<PredictedBlock> edits_;
 std::vector<PredictedBlock>::iterator find(std::uint64_t command) {
 return std::find_if(edits_.begin(),edits_.end(),[&](const auto& edit) {return edit.command==command;});
 }
};
}
