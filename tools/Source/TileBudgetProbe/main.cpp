#include "TileBudget.h"
#include "MapUploadBudget.h"
#include <cstdio>
#include <stdexcept>
using namespace octaryn::client::rendering;
void require(bool value) {if(!value)throw std::runtime_error("streaming budget boundary failed");}
int main() {
  try {
    for(std::uint64_t budget=0;budget<201;++budget)for(std::uint64_t usage=0;usage<203;++usage)
      for(std::uint64_t pending=0;pending<203;++pending) {
        const auto ceiling=budget*7/10;
        const auto expected=usage+pending>=ceiling?0:ceiling-usage-pending;
        require(tile_gpu_headroom(budget,usage,pending)==expected);
        require(tile_gpu_admits(expected,budget,usage,pending));
        require(!tile_gpu_admits(expected+1,budget,usage,pending));
      }
    require(tile_gpu_headroom(UINT64_MAX,UINT64_MAX,UINT64_MAX)==0);
    require(!tile_gpu_admits(UINT64_MAX,UINT64_MAX,0,0));
    require(tile_gpu_headroom(1000,600,50)==50);
    for(std::uint64_t budget=0;budget<=2*1024*1024;budget+=127) {
      const auto copied=map_buffer_upload_bytes(1024*1024,budget);
      require(copied<=budget && copied<=256*1024 && copied%4==0);
      for(const std::uint64_t pitch:{64ull,256ull,1024ull,16384ull,UINT64_MAX}) {
        const auto rows=map_texture_upload_rows(4096,pitch,budget);
        if(rows) {
          const auto padded=(rows*pitch+511)&~std::uint64_t(511);
          require(padded<=budget && padded<=256*1024 && rows<=4096);
        }
      }
    }
    require(map_texture_upload_rows(1,256,511)==0);
    require(map_texture_upload_rows(1,256,512)==1);
    require(map_texture_upload_rows(1,0,512)==0);
    std::puts("tile_budget_tests passed=1 os_admission=8283009 copy_boundaries=1 overflow=1");return 0;
  } catch(const std::exception& error) {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
