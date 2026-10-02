#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace octaryn::client::rendering {
inline bool tile_desired_set(std::uint32_t total,std::span<const std::uint32_t> wanted,
 std::span<const std::uint32_t> retained,std::vector<bool>& wanted_out,std::vector<bool>& retained_out) {
 if(!total || total>65536 || wanted.size()>total || retained.size()>total)return false;
 std::vector<bool> next_wanted(total),next_retained(total);
 for(auto index:wanted) {if(index>=total || next_wanted[index])return false;next_wanted[index]=true;}
 for(auto index:retained) {if(index>=total || next_retained[index])return false;next_retained[index]=true;}
 for(auto index:wanted)if(!next_retained[index])return false;
 wanted_out=std::move(next_wanted);retained_out=std::move(next_retained);return true;
}
}
