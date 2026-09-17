#pragma once
#include "WorldStream.h"
#include "BlockInteraction.h"
#include <filesystem>
#include <cmath>

namespace octaryn::client::app::local_session {
// CPU-only authoritative stream. It never receives speculative block overlays.
class PredictionWorld {
 world_presentation::WorldStream stream_;
 world_presentation::BlockInteraction catalog_;
 bool catalog_ready_{};
public:
 PredictionWorld(const std::filesystem::path& snapshot,const std::filesystem::path& bundle):stream_(snapshot) {
 catalog_ready_=catalog_.load_catalog(bundle / "Data" / "Blocks" / "octaryn.basegame.blocks.json");
 }
 void update(float x,float z) {
 stream_.request(int32_t(std::floor(x/32)),int32_t(std::floor(z/32)),1);
 world_presentation::StreamColumn column;
 for (unsigned i=0;i<4 && stream_.poll(column);++i) {}
 }
 static bool query(void* context,int32_t x,int32_t y,int32_t z,uint32_t& packed) {
 const auto& self=*static_cast<const PredictionWorld*>(context);
 uint16_t block{};
 if (!self.catalog_ready_ || !self.stream_.try_block(x,y,z,block)) return false;
 packed=uint32_t(block)|(self.catalog_.blocks_camera(block)?(1u<<16):0u);
 return true;
 }
};
}
