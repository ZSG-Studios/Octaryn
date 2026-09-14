#include "LocalSession.h"
#include "BlockInteraction.h"
#include "WorldStream.h"
#include "PoseHistory.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace octaryn::client::app;
using namespace octaryn::client::world_presentation;
namespace {
void require(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
struct Runner {
  LocalSession session;
  std::unique_ptr<WorldStream> stream;
  BlockInteraction interaction;
  LocalPlayerPose pose{};
  LocalPlayerInput input{};
  std::chrono::steady_clock::time_point previous{std::chrono::steady_clock::now()};
  Runner(const std::filesystem::path& bundle, const std::filesystem::path& world,
         const std::filesystem::path& logs, const std::filesystem::path& catalog) {
    require(interaction.load_catalog(catalog), "load catalog");
    require(session.start(bundle, world, 2, logs), "start isolated session");
    stream = std::make_unique<WorldStream>(session.chunk_stream_path());
    input.flying = true; input.pitch = -1.2f;
  }
  void tick() {
    const auto now = std::chrono::steady_clock::now();
    session.update(input, std::chrono::duration<double>(now-previous).count());
    previous = now;
    require(session.running(), "server stopped unexpectedly");
    StreamColumn column;
    while (stream->poll(column)) {}
    if (session.player_pose(pose))
      interaction.update(*stream, pose.x, pose.y, pose.z, input.yaw, input.pitch);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  template <typename Check> void until(Check check, const char* failure) {
    const auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(20);
    do { tick(); if(check()) return; } while(std::chrono::steady_clock::now()<deadline);
    std::cerr << "session_status=" << session.status() << " stream_status=" << stream->status() << '\n';
    throw std::runtime_error(failure);
  }
  bool air(BlockPosition p) { std::uint16_t block=65535; return stream->try_block(p.x,p.y,p.z,block) && block==0; }
};
}
int main(int argc, char** argv) {
  try {
    require(argc==5, "arguments: isolated bundle, fresh world, logs, catalog");
    {
      local_session::PoseHistory history;
      LocalPlayerPose a{}, b{}, sampled{};
      a.source_tick=1; a.source_seconds=1; a.world_day_fraction=.99f; a.world_total_seconds=99;
      b.source_tick=2; b.source_seconds=1.1; b.world_day_fraction=.01f; b.world_total_seconds=101;
      require(history.push(a) && history.push(b),"world time pose samples");
      history.advance(.05);
      require(history.sample(sampled) && (sampled.world_day_fraction<.001f || sampled.world_day_fraction>.999f) &&
          std::abs(sampled.world_total_seconds-100)<.001,"midnight-safe world time interpolation");
    }
    const std::filesystem::path bundle=argv[1], world=argv[2], logs=argv[3], catalog=argv[4];
    require(!std::filesystem::exists(world), "probe requires a fresh isolated world");
    BlockPosition first{}, second{};
    {
      Runner run(bundle,world,logs/"first",catalog);
      run.until([&]{return run.pose.flying && run.interaction.target().actionable;},"initial authoritative target");
      const auto world_time=run.pose.world_total_seconds;
      run.until([&]{return run.pose.world_total_seconds>world_time+.05;},"stationary live world clock");
      BlockEditIntent edit;
      require(run.interaction.make_edit(false,edit),"prepare first break"); first=edit.edit;
      require(run.session.submit_block_edit(edit),"queue first break");
      run.until([&]{return run.air(first);},"first break did not replicate air");
      std::cout << "first_break=" << first.x << ',' << first.y << ',' << first.z << " replicated=1\n";
      // A forged nearby camera must not allow a remote hit: authority supplies its own eye.
      const BlockEditIntent remote{{10000,-1,0},{10000,-1,0},0,10000.5f,-.5f,.5f};
      require(run.session.submit_block_edit(remote),"queue deliberately out of reach command");
      run.until([&]{
        std::ifstream log(logs/"first"/"local-session.log");
        const std::string text{std::istreambuf_iterator<char>(log),{}};
        return text.find("request=2")!=std::string::npos && text.find("server_live_client_command_rejected")!=std::string::npos;
      },"out of reach command not rejected");
      run.until([&]{return !std::filesystem::exists(world/"runtime"/"block_interaction.json");},"rejected file not acknowledged");
      require(run.interaction.make_edit(false,edit),"prepare second break"); second=edit.edit;
      require(first.x!=second.x || first.y!=second.y || first.z!=second.z,"second target must advance past air");
      require(run.session.submit_block_edit(edit),"queue valid command after rejection");
      run.until([&]{return run.air(second);},"valid command after rejection did not apply");
      std::cout << "second_break=" << second.x << ',' << second.y << ',' << second.z << " rejection_recovery=1\n";
      run.until([&]{
        const auto target=run.interaction.target();
        std::uint16_t block{};
        return target.hit && run.stream->try_block(target.block.x,target.block.y,target.block.z,block) && block!=0;
      },"target refresh after second break");
      run.interaction.pick();
      require(run.interaction.make_edit(true,edit),"prepare placement against next target");
      const auto placed = edit.edit;
      const auto placed_block = edit.block;
      require(run.session.submit_block_edit(edit),"queue valid placement");
      run.until([&]{
        std::uint16_t block{};
        return run.stream->try_block(placed.x,placed.y,placed.z,block) && block==placed_block;
      },"placement did not replicate");
      run.until([&]{const auto p=run.interaction.target().block;
        return p.x==placed.x && p.y==placed.y && p.z==placed.z;
      },"target refresh after placement");
      require(run.interaction.make_edit(false,edit) && edit.edit.x==placed.x &&
          edit.edit.y==placed.y && edit.edit.z==placed.z,"placed block becomes target");
      require(run.session.submit_block_edit(edit),"queue break of placed block");
      run.until([&]{return run.air(placed);},"break of placed block did not replicate");
      std::cout << "placement=" << placed.x << ',' << placed.y << ',' << placed.z << " block=" << placed_block << " replicated_then_removed=1\n";
      run.session.stop();
    }
    {
      Runner run(bundle,world,logs/"restart",catalog);
      run.until([&]{return run.air(first) && run.air(second);},"saved air edits missing after restart");
      run.session.stop();
    }
    std::cout << "local_interaction_session_probe=passed authority=passed streamed_air=passed rejection_recovery=passed restart_persistence=passed\n";
    return 0;
  } catch(const std::exception& error) {
    std::cerr << "local_interaction_session_probe=failed " << error.what() << '\n'; return 1;
  }
}
