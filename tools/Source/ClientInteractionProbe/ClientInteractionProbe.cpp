#include "BlockInteraction.h"
#include "WorldStream.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <limits>

using namespace octaryn::client::world_presentation;
namespace {
void require(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
template <typename T> void write(std::ofstream& out, T value) {
  out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
void snapshot_file(const std::filesystem::path& path) {
  std::ofstream out(path, std::ios::binary);
  out.write("OCSTRM01",8); write(out,3u); write(out,std::uint64_t{1});write(out,std::uint64_t{});
  write(out,-1); write(out,-1); write(out,2u); write(out,std::uint64_t{1337});
  write(out,0u); write(out,3u);
  write(out,std::uint64_t{}); write(out,0u); write(out,0.0); write(out,0.0f);
  for(int i=0;i<8;++i) write(out,0.0f);
  write(out,0u); write(out,1u); write(out,1u); write(out,0u);
  write(out,-1); write(out,-1); write(out,-32); write(out,-32); write(out,0u); write(out,0u);
}
}
int main(int argc, char** argv) {
  const auto path = std::filesystem::temp_directory_path() /
      ("octaryn-interaction-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".bin");
  try {
    require(argc == 2, "provide basegame block catalog path");
    snapshot_file(path);
    WorldStream stream(path);
    stream.request(-1,-1,2);
    StreamColumn column;
    bool ready=false;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while (!(ready=stream.poll(column)) && std::chrono::steady_clock::now()<deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    require(ready,"column ready");
    std::uint16_t block{};
    require(stream.try_block(-32,-256,-32,block) && block!=0,"negative origin query");
    require(!stream.try_block(0,0,0,block),"unknown column remains unknown");
    int surface=255;
    while (stream.try_block(-16,surface,-16,block) && block==0) --surface;
    BlockInteraction interaction;
    require(!interaction.select(0) && !interaction.select(1),"selection requires a loaded catalog");
    require(interaction.load_catalog(argv[1]),"catalog load");
    require(interaction.selected()==25,"basegame default selection");
    for(const std::uint16_t invalid:{std::uint16_t{8},std::uint16_t{15},std::uint16_t{65535}})
      require(!interaction.select(invalid) && interaction.selected()==25,
          "cloud flowing fluid and out-of-range selections rejected without replacing selected block");
    require(interaction.select(14) && interaction.selected()==14,"placeable water source accepted");
    require(interaction.select(25) && interaction.selected()==25,"valid hotbar selection accepted");
    interaction.cycle(1); require(interaction.selected()==26,"original positive selection cycle");
    interaction.cycle(-1); require(interaction.selected()==25,"original negative selection cycle");
    interaction.update(stream,-15.5f,surface+5.25f,-15.5f,0,-1.57079632679f);
    const auto target=interaction.target();
    require(target.hit && target.actionable && target.can_place,"downward reachable target");
    require(target.block.x==-16 && target.block.y==surface && target.block.z==-16,"ray hit coordinates");
    BlockEditIntent edit;
    require(interaction.make_edit(false,edit) && edit.block==0 && edit.edit.y==surface,"break intent only");
    require(interaction.make_edit(true,edit) && edit.block==25 && edit.edit.y==surface+1,"adjacent place intent");
    require(interaction.select(0) && interaction.selected()==0,"empty inventory slot selects no placeable block");
    edit.block=65535;
    require(!interaction.make_edit(true,edit) && edit.block==65535,
        "empty slot suppresses placement without generating an air place intent");
    require(interaction.make_edit(false,edit) && edit.block==0 && edit.edit.y==surface,
        "empty hand retains block breaking");
    require(!interaction.select(15) && interaction.selected()==0 && !interaction.make_edit(true,edit),
        "invalid selection cannot reenable placement from empty slot");
    require(interaction.select(26) && interaction.make_edit(true,edit) && edit.block==26,
        "valid hotbar selection restores placement with requested block");
    interaction.pick(); require(interaction.selected()==target.block_id,"pick target block");
    interaction.update(stream,-15.5f,surface+8.5f,-15.5f,0,-1.57079632679f);
    require(interaction.target().hit && !interaction.target().actionable && !interaction.make_edit(false,edit),"ten block preview does not bypass six block authority");
    const InteractionView view{{-15.5f,surface+5.25f,-15.5f},0,-1.57079632679f};
    for(const float side:{-1.5f,1.5f}) {
      const InteractionEye eye{-15.5f+side,surface+5.25f,-15.5f};
      interaction.update(stream,view,eye);
      require(interaction.target().hit && interaction.target().actionable && interaction.target().block.x==-16 &&
          interaction.target().block.z==-16,"shoulder targeting follows camera ray, not displaced player eye");
      require(interaction.make_edit(false,edit) && edit.camera_x==eye.x && edit.camera_y==eye.y && edit.camera_z==eye.z,
          "shoulder edit intent preserves authoritative player eye");
    }
    interaction.update(stream,{{-15.5f,surface+8.5f,-15.5f},0,-1.57079632679f},
        {-15.5f,surface+3.5f,-15.5f});
    require(interaction.target().hit && interaction.target().actionable && interaction.make_edit(false,edit),
        "distant shoulder camera must not reduce player reach");
    interaction.update(stream,{{-15.5f,surface+3.5f,-15.5f},0,-1.57079632679f},
        {-15.5f,surface+8.5f,-15.5f});
    require(interaction.target().hit && !interaction.target().actionable && !interaction.make_edit(false,edit),
        "near camera must not extend authoritative player reach");
    interaction.update(stream,view,{std::numeric_limits<float>::quiet_NaN(),0,0});
    require(!interaction.target().hit && !interaction.make_edit(false,edit),"invalid player eye rejects shoulder edit");
    interaction.update(stream,{{0,0,std::numeric_limits<float>::infinity()},0,0},{0,0,0});
    require(!interaction.target().hit,"invalid render camera rejects targeting");
    interaction.update(stream,0.5f,surface+3.0f,0.5f,0,-1.57079632679f);
    require(!interaction.target().hit,"unloaded column not targeted");
    require(stream.try_block(target.block.x,target.block.y,target.block.z,block) && block==target.block_id,"commands never mutate client terrain");
    std::filesystem::remove(path);
    std::cout<<"client_interaction_probe=passed catalog=passed raycast=passed intent=passed authority_reach=passed shoulder_ray=passed eye_intent=passed inventory_select=passed empty_slot=passed\n";
    return 0;
  } catch(const std::exception& error) {
    std::error_code ignored; std::filesystem::remove(path,ignored);
    std::cerr<<"client_interaction_probe=failed "<<error.what()<<'\n'; return 1;
  }
}
