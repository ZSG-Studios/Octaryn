#include "MapAssetBuildInternal.h"
#include <cstdio>
#include <stdexcept>

using namespace octaryn::client::rendering;
namespace {
void require(bool okay,const char* reason) {if(!okay)throw std::runtime_error(reason);}
std::shared_ptr<MapTextureResource> texture(MapTexturePool& pool,const std::filesystem::path& cache,
    const char* key,bool ready,bool validated) {
  auto value=std::make_shared<MapTextureResource>();value->ready=ready;value->validated_cache=validated;
  value->cache_directory=cache;value->content_key=key;value->bytes=64;
  value->metadata={true,false,true,4,4,1,64};
  pool.textures[map_texture_pool_key(cache,key,false)]=value;return value;
}
}
int main() try {
  const std::filesystem::path cache="world-a/textures",other="world-b/textures";
  MapTexturePool pool;
  auto ready=texture(pool,cache,"shared",true,true);
  auto pending=texture(pool,cache,"pending",false,true);
  auto uncooked=texture(pool,cache,"uncooked",true,false);
  auto foreign=texture(pool,other,"shared",true,true);
  auto index=snapshot_map_texture_reuse(pool,cache);
  require(index && index->entries.size()==1,"publication exposed unverified, foreign or fence-pending texture");
  require(index==snapshot_map_texture_reuse(pool,cache),"unchanged generation rebuilt snapshot");
  require(!index->lookup(other,"shared"),"world cache namespaces aliased");
  require(map_texture_pool_key(cache,"shared",false)!=map_texture_pool_key(other,"shared",false),"pool namespace collision");
  pending->ready=true;++pool.ready_generation;
  auto newer=snapshot_map_texture_reuse(pool,cache);
  require(newer!=index && newer->entries.size()==2 && !index->lookup(cache,"pending"),"immutable publication changed old snapshot");
  auto lease=index->lookup(cache,"shared");std::weak_ptr<MapTextureResource> weak=ready;ready.reset();
  require(!weak.expired() && map_texture_pool_stats(pool).allocated==256,"prepared ticket lost unique pool accounting");
  PreparedMapAsset asset;asset.texture_cache=cache;asset.images.textures.resize(1);
  asset.images.textures[0].key="shared";asset.images.textures[0].resident=lease;
  require(map_texture_pool_additional_bytes(pool,asset)==0,"ticket double-reserved GPU bytes");
  // Admission transfers ownership before destroying the prepared ticket.
  auto builder_owner=asset.images.textures[0].resident->resource;asset={};lease.reset();
  require(map_texture_pool_stats(pool).allocated==256,"adoption lost accounting");
  builder_owner.reset();
  require(weak.expired() && map_texture_pool_stats(pool).allocated==192 && !index->lookup(cache,"shared"),
      "cancel/eviction retained resource through weak snapshot");
  MapTexturePool next_world;
  require(!snapshot_map_texture_reuse(next_world,cache)->lookup(cache,"pending"),"map switch reused another pool");
  pending.reset();uncooked.reset();foreign.reset();
  require(map_texture_pool_stats(pool).allocated==0,"retired world retained GPU allocation");
  // A reload republishes the same content identity under a new ready generation.
  auto replacement=texture(pool,cache,"shared",true,true);++pool.ready_generation;
  require(!index->lookup(cache,"shared") && snapshot_map_texture_reuse(pool,cache)->lookup(cache,"shared"),
      "expired old generation was revived or new resource hidden");
  std::puts("map_texture_reuse_pool_tests passed=1");return 0;
} catch(const std::exception& error) {std::fprintf(stderr,"%s\n",error.what());return 1;}
