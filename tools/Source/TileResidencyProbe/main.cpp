#include "TileSet.h"
#include "MapManifest.h"
#include "WorldLibrary.h"
#include "TileLoadLimits.h"
#include "TileRegionStatus.h"
#include "TileDesiredSet.h"
#include "TileActorAnchor.h"
#include "MapMeshOptimization.h"
#include "GltfTriangleReader.h"
#include "TileCatalogFiles.h"
#include "../../../octaryn-server/Source/World/MapWorld/MapManifest.h"
#include "../../../octaryn-server/Source/World/MapWorld/MapSceneGeometry.h"
#include <limits>
#include <fstream>
#include <iostream>
using namespace octaryn::client;
namespace octaryn::client::app {bool WorldLibrary::resolve(const std::filesystem::path&,MapManifest&,std::string&,const std::atomic_bool*){std::abort();}}
int main(int argc,char** argv){
 std::string error;unsigned checks{};auto check=[&](bool ok){++checks;if(!ok){std::cerr<<"FAIL "<<checks<<" error="<<error;std::exit(1);}};
 check(argc>=2 && argc<=4);const auto path=std::filesystem::path(argv[1])/"fixture.json";
 auto load=[&](std::string ids,std::string files="[\"a.gltf\",\"b.gltf\"]",std::string policy=""){
 std::ofstream(path)<<"{\"version\":1,\"map\":\"world.gltf\",\"tiles\":[[0,-4,0,10,4,10],[10,-4,0,20,4,10]],\"tile_files\":"<<files<<",\"tile_ids\":"<<ids<<policy<<"}";
 app::TileSet tiles;return std::pair(tiles.load(path),std::move(tiles));};
 auto [ok,tiles]=load("[\"cell-a/0\",\"cell-b/0\"]");check(ok);check(tiles.tile_count()==2);check(tiles.tile(0)->id=="cell-a/0");
 check(tiles.evaluate(5,0,5,2,3).wanted==1);tiles.mutable_tile(0)->resident=true;check(tiles.evaluate(15,0,5,2,3).evict_queue==1);
 check(!load("[\"duplicate\",\"duplicate\"]").first);check(!load("[\"onlyone\"]").first);check(!load("[\"bad id\",\"okay\"]").first);
 check(!load("[\"cell-a/0\",\"cell-b/0\"]","[\"../a.gltf\",\"b.gltf\"]").first);
 check(!load("[\""+std::string(129,'a')+"\",\"okay\"]").first);
 auto legacy=load("[]","[\"My Long Tile Name.gltf\",\"b.gltf\"]");check(legacy.first);check(legacy.second.tile(0)->id=="tile/0");
 std::vector<bool> wanted,retained;const std::uint32_t initial[]{0,1},one[]{1},bad[]{2},duplicate[]{1,1};
 check(rendering::tile_desired_set(2,initial,initial,wanted,retained));check(wanted[0]&&wanted[1]);
 check(!rendering::tile_desired_set(2,bad,initial,wanted,retained));check(wanted[0]&&wanted[1]);
 check(!rendering::tile_desired_set(2,duplicate,initial,wanted,retained));check(!rendering::tile_desired_set(2,initial,one,wanted,retained));
 check(rendering::tile_desired_set(2,one,initial,wanted,retained));check(!wanted[0]&&wanted[1]&&retained[0]);
 check(rendering::tile_desired_set(2,{}, {},wanted,retained));check(!wanted[0]&&!retained[1]);
 const std::string ids="[\"cell-a/0\",\"cell-b/0\"]",files="[\"a.gltf\",\"b.gltf\"]";
 check(tiles.tile(0)->collision && tiles.tile(1)->collision);
 auto render_only=load(ids,files,",\"tile_collision\":[true,false]");
 check(render_only.first);check(render_only.second.tile(0)->collision && !render_only.second.tile(1)->collision);
 check(!load(ids,files,",\"tile_collision\":[false]").first);
 check(!load(ids,files,",\"tile_collision\":[false,false,false]").first);
 check(!load(ids,files,",\"tile_collision\":[true,1]").first);
 const auto pack=path.parent_path()/"fixture.pack";
 {std::ofstream(pack,std::ios::binary)<<std::string(128,'x');}
 const std::string packed_files="[\"fixture.pack\",\"fixture.pack\"]";
 auto packed=load(ids,packed_files,",\"tile_collision\":[false,false],\"tile_ranges\":[[0,64],[64,64]]");
 check(packed.first);check(packed.second.tile(1)->source_offset==64 && packed.second.tile(1)->source_length==64);
 check(load(ids,packed_files,",\"tile_ranges\":[[0,64],[64,64]]").first);
 check(load(ids,packed_files,",\"tile_collision\":[false,true],\"tile_ranges\":[[0,64],[64,64]]").first);
 check(!load(ids,packed_files,",\"tile_collision\":[false,false],\"tile_ranges\":[[0,64]]").first);
 check(!load(ids,packed_files,",\"tile_collision\":[false,false],\"tile_ranges\":[[0,0],[1,0]]").first);
 check(!load(ids,packed_files,",\"tile_collision\":[false,false],\"tile_ranges\":[[0,64],[64,65]]").first);
 check(!load(ids,packed_files,",\"tile_collision\":[false,false],\"tile_ranges\":[[18446744073709551615,64],[0,0]]").first);
 check(tiles.gpu_budget_mib()==0);check(tiles.texture_cache_directory()==std::filesystem::path(path.string()+".textures"));
 check(load(ids,files,",\"tile_gpu_budget_mib\":4096").second.gpu_budget_mib()==4096);
 check(load(ids,files,",\"tile_gpu_budget_mib\":64").first);check(load(ids,files,",\"tile_gpu_budget_mib\":32768").first);
 check(!load(ids,files,",\"tile_gpu_budget_mib\":63").first);check(!load(ids,files,",\"tile_gpu_budget_mib\":32769").first);
 check(!load(ids,files,",\"tile_gpu_budget_mib\":-1").first);check(!load(ids,files,",\"tile_gpu_budget_mib\":64.5").first);
 auto external=load(ids,files,",\"tile_residency\":\"external\",\"tile_initial_wanted\":[0,1]");
 check(external.first);check(external.second.external_residency());check(external.second.initial_wanted().size()==2);
 check(!load(ids,files,",\"tile_residency\":\"external\"").first);
 check(!load(ids,files,",\"tile_residency\":\"external\",\"tile_initial_wanted\":[2]").first);
 check(!load(ids,files,",\"tile_residency\":\"external\",\"tile_initial_wanted\":[0,0]").first);
 check(!load(ids,files,",\"tile_residency\":\"invented\"").first);
 octaryn_host_region_anchor actor;check(sizeof(actor)==12);
 check(!rendering::tile_actor_anchor(true,true,false,1,2,3,actor));check(actor.x==0);
 check(rendering::tile_actor_anchor(true,true,true,1,2,3,actor));check(actor.x==1&&actor.y==2&&actor.z==3);
 check(!rendering::tile_actor_anchor(false,true,true,1,2,3,actor));check(!rendering::tile_actor_anchor(true,false,true,1,2,3,actor));
 check(!rendering::tile_actor_anchor(true,true,true,std::numeric_limits<float>::quiet_NaN(),2,3,actor));
 auto ready=rendering::tile_region_status(*tiles.tile(0),0,5,true,true,true,true,false,7);
 check(ready.size==192 && sizeof(ready)==192);check(ready.generation==7);check(ready.flags==15);check(std::string(ready.id)=="cell-a/0");check(ready.bounds[3]==10);
 auto retired=rendering::tile_region_status(*tiles.tile(0),0,0,false,false,false,false,false,8);check(retired.flags==0);check(retired.generation==8);
 auto failed=rendering::tile_region_status(*tiles.tile(1),1,1,true,true,false,false,true,8);check(failed.flags==19);
 const auto asset=path.parent_path()/"oversized.gltf";const auto buffer=path.parent_path()/"oversized.bin";
 {std::ofstream binary(buffer,std::ios::binary);const float vertices[9]={0,0,0,1,0,0,0,0,1};binary.write(reinterpret_cast<const char*>(vertices),sizeof(vertices));
  for(unsigned index=0;index<393219;++index){const unsigned value=index%3;binary.write(reinterpret_cast<const char*>(&value),4);}}
 {std::ofstream gltf(asset);gltf<<R"({"asset":{"version":"2.0"},"buffers":[{"uri":"oversized.bin","byteLength":1572912}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":1572876}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5125,"count":393219,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";}
 const auto limits=rendering::tile_load_limits();rendering::MapModel model;
 check(!rendering::load_map_model(asset,model,error,limits));check(!error.empty());
 std::string packed_json=R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":48}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":12}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,0,1]},{"bufferView":1,"componentType":5125,"count":3,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
 while(packed_json.size()%4)packed_json+=' ';
 const std::uint32_t packed_length=12+8+unsigned(packed_json.size())+8+48;
 {std::ofstream binary(pack,std::ios::binary);binary<<std::string(16,'x');
  auto u32=[&](std::uint32_t value){binary.write(reinterpret_cast<const char*>(&value),4);};
  u32(0x46546c67);u32(2);u32(packed_length);u32(unsigned(packed_json.size()));u32(0x4e4f534a);binary<<packed_json;
  u32(48);u32(0x004e4942);const float vertices[9]={0,0,0,1,0,0,0,0,1};binary.write(reinterpret_cast<const char*>(vertices),36);
  u32(0);u32(1);u32(2);binary<<std::string(20,'x');}
 auto range_limits=limits;range_limits.source_offset=16;range_limits.source_length=packed_length;
 check(rendering::load_map_model(pack,model,error,range_limits));check(model.indices.size()==3 && model.vertices.size()==3);
 range_limits.source_length=packed_length-1;check(!rendering::load_map_model(pack,model,error,range_limits));
 range_limits.source_length=packed_length;range_limits.source_offset=0;check(!rendering::load_map_model(pack,model,error,range_limits));
 range_limits.source_offset=UINT64_MAX;check(!rendering::load_map_model(pack,model,error,range_limits));
 range_limits.source_offset=16;range_limits.source_length=64ull*1024*1024+1;check(!rendering::load_map_model(pack,model,error,range_limits));
 unsigned collision_fixture{};
 auto write_collision_fixture=[&](std::string declaration) {
   auto text=packed_json;const auto at=text.find("\"meshes\":[{");text.insert(at+11,"\"extras\":{\"octaryn_collision\":"+declaration+"},");
   while(text.size()%4)text+=' ';
   const auto fixture_path=path.parent_path()/("collision-"+std::to_string(collision_fixture++)+".glb");std::ofstream binary(fixture_path,std::ios::binary);
   auto u32=[&](std::uint32_t value){binary.write(reinterpret_cast<const char*>(&value),4);};
   u32(0x46546c67);u32(2);u32(12+8+unsigned(text.size())+8+48);u32(unsigned(text.size()));u32(0x4e4f534a);binary<<text;
   u32(48);u32(0x004e4942);const float vertices[9]={0,0,0,1,0,0,0,0,1};binary.write(reinterpret_cast<const char*>(vertices),36);u32(0);u32(1);u32(2);
   return fixture_path;
 };
 auto collision_path=write_collision_fixture("{\"version\":1,\"enabled\":false}");
 check(rendering::load_map_model(collision_path,model,error,limits));check(model.indices.size()==3 && model.collision_indices.empty());
 check(rendering::optimize_map_mesh(model,error));check(model.indices.size()==3 && model.collision_indices.empty());
 octaryn::server::map_world::MapTriangleSoup physical;check(octaryn::server::map_world::load_map_triangle_soup(collision_path,physical));check(physical.indices.empty());
 octaryn::assets::GltfTriangleReader reader;octaryn::assets::GltfTriangleWindow window;
 check(reader.open(collision_path,path.parent_path()/"collision-scratch",error));check(reader.read(0,0,0,1,window,error));check(window.indices.empty());
 collision_path=write_collision_fixture("{\"version\":1,\"enabled\":true}");
 check(rendering::load_map_model(collision_path,model,error,limits));check(model.indices.size()==3 && model.collision_indices.size()==3);
 check(rendering::optimize_map_mesh(model,error));check(model.indices==model.collision_indices);
 physical={};check(octaryn::server::map_world::load_map_triangle_soup(collision_path,physical));check(physical.indices.size()==3);
 collision_path=write_collision_fixture("{\"version\":1,\"enabled\":1}");check(!rendering::load_map_model(collision_path,model,error,limits));
 collision_path=write_collision_fixture("{\"version\":2,\"enabled\":false}");check(!rendering::load_map_model(collision_path,model,error,limits));
check(limits.source_bytes==64ull*1024*1024);check(limits.encoded_bytes==64ull*1024*1024);check(limits.triangles==131072 && limits.primitives==2048);check(limits.accessor_elements==393216);
 std::ofstream(path.parent_path()/"world.gltf")<<"{}";
 auto strict=[&](std::string field){std::ofstream(path)<<"{\"version\":1,\"map\":\"world.gltf\",\"spawn\":[0,0,0],\"yaw\":0,\"pitch\":0"<<field<<"}";app::MapManifest actual;return app::load_map_manifest_from(path,actual);};
 check(strict(""));check(strict(",\"tile_gpu_budget_mib\":4096"));check(strict(",\"tile_gpu_budget_mib\":64"));check(strict(",\"tile_gpu_budget_mib\":32768"));
 check(!strict(",\"tile_gpu_budget_mib\":0"));check(!strict(",\"tile_gpu_budget_mib\":63"));check(!strict(",\"tile_gpu_budget_mib\":32769"));check(!strict(",\"tile_gpu_budget_mib\":-1"));check(!strict(",\"tile_gpu_budget_mib\":64.5"));check(!strict(",\"unknown_budget\":4096"));
 const auto shard_path=path.parent_path()/"catalog.json";
 const std::string shard="{\"version\":1,\"tiles\":[[0,0,0,1,1,1]],\"tile_files\":[\"world.gltf\"],\"tile_ids\":[\"source-cell/ground\"],\"tile_collision\":[true],\"tile_ranges\":[[0,0]]}";
 auto shard_check=[&](std::string body,std::vector<std::string> names=std::vector<std::string>{"catalog.json"}) {
   std::ofstream(shard_path)<<body;octaryn::content::TileCatalogArrays arrays;
   return octaryn::content::expand_tile_catalogs(path,names,arrays);
 };
 check(shard_check(shard));check(!shard_check(shard,{"catalog.json","catalog.json"}));
 check(!shard_check(shard,{"../catalog.json"}));check(!shard_check(shard,{"missing.json"}));
 auto invalid=shard;invalid.replace(invalid.find("world.gltf"),10,"../world.gltf");check(!shard_check(invalid));
 invalid=shard;invalid.replace(invalid.find("\"version\":1"),11,"\"version\":2");check(!shard_check(invalid));
 invalid=shard;invalid.replace(invalid.find("[[0,0]]"),7,"[[1,0]]");check(!shard_check(invalid));
 invalid=shard;invalid.replace(invalid.find("[true]"),6,"[true,false]");check(!shard_check(invalid));
 if(argc>=3){app::MapManifest actual;check(app::load_map_manifest_from(std::filesystem::u8path(argv[2]),actual));app::TileSet real;check(real.load(std::filesystem::u8path(argv[2])));check(real.external_residency());check(!real.initial_wanted().empty());
  octaryn::server::map_world::MapManifest server;check(octaryn::server::map_world::parse_map_manifest(std::filesystem::u8path(argv[2]),server));check(server.tiles.size()==real.tile_count());
  unsigned range_checks{};
  for(std::uint32_t i=0;i<real.tile_count() && range_checks<3;++i) {
    const auto& tile=*real.tile(i);if(!tile.collision || !tile.source_length)continue;
    auto source=std::filesystem::u8path(argv[2]).parent_path()/std::filesystem::u8path(tile.file);
    auto range_limits=limits;range_limits.source_offset=tile.source_offset;range_limits.source_length=tile.source_length;
    check(rendering::load_map_model(source,model,error,range_limits));check(!model.collision_indices.empty());
    physical={};physical.source_offset=tile.source_offset;physical.source_length=tile.source_length;
    check(octaryn::server::map_world::load_map_triangle_soup(source,physical));check(physical.indices.size()==model.collision_indices.size());
    check(reader.open_range(source,path.parent_path()/"ground-scratch",tile.source_offset,tile.source_length,error));
    check(reader.read(0,0,0,1,window,error));check(window.indices.size()==3);
    check(server.tile_ranges[i][0]==tile.source_offset && server.tile_ranges[i][1]==tile.source_length);
    ++range_checks;
  }
  std::cout<<"physical_packed_probes="<<range_checks<<" client_server_collision_agree=1\n";
  std::cout<<"real_tiles="<<real.tile_count()<<" initial_wanted="<<real.initial_wanted().size()<<" external=1\n";}
 if(argc==4) {
   auto source=std::filesystem::u8path(argv[3]);check(rendering::load_map_model(source,model,error));
   const auto collision_count=model.collision_indices.size();check(collision_count<model.indices.size());
   check(rendering::optimize_map_mesh(model,error));check(model.collision_indices.size()==collision_count);
   physical={};check(octaryn::server::map_world::load_map_triangle_soup(source,physical));check(physical.indices.size()==collision_count);
   std::cout<<"owned_collision render_triangles="<<model.indices.size()/3<<" physical_triangles="<<collision_count/3<<" excluded_triangles="<<(model.indices.size()-collision_count)/3<<"\n";
 }
 std::cout<<"PASS "<<checks<<" tile identity/admission/residency assertions\n";
}
