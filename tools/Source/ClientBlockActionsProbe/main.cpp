#include "LocalSession.h"
#include "SessionFiles.h"
#include "WorldStream.h"
#include "BlockInteraction.h"
#include "PredictedColumn.h"
#include <glaze/glaze.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <thread>

using namespace octaryn::client::app;
using namespace octaryn::client::world_presentation;
using Clock = std::chrono::steady_clock;
struct IntentMetadata { uint64_t frameIndex{},movementFrameID{}; };
namespace {
void require(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
struct Cell { BlockPosition position; uint16_t original{}; };
struct Pending { BlockEditIntent edit; bool accepted{}; uint64_t ack_before{}; };
class Probe {
public:
  LocalSession session;
  std::unique_ptr<WorldStream> stream;
  BlockInteraction catalog;
  LocalPlayerInput input{};
  LocalPlayerPose pose{};
  std::map<std::pair<int,int>,StreamColumn> bases;
  std::map<uint64_t,Pending> pending;
  std::map<uint64_t,BlockReceipt> results;
  std::map<uint64_t,uint64_t> dependencies;
  std::string receipt_session;
  uint64_t sequence{},gated{};
  unsigned feedback_failures{};
  Clock::time_point previous=Clock::now();

  template<class F> void until(F condition,const char* message,int timeout=30) {
    const auto end=Clock::now()+std::chrono::seconds(timeout);
    while(!condition()) {require(Clock::now()<end,message);pump();}
  }
  void inspect_intent() {
    std::string text;
    if(!local_session::read_text(session.chunk_stream_path().parent_path()/"block_interaction.json",text))return;
    IntentMetadata metadata;
    constexpr glz::opts options{.error_on_unknown_keys=false};
    if(!glz::read<options>(metadata,text) && metadata.frameIndex && metadata.movementFrameID)
      dependencies.insert_or_assign(metadata.frameIndex,metadata.movementFrameID);
  }
  void pump() {
    inspect_intent();
    const auto now=Clock::now();
    session.update(input,std::chrono::duration<double>(now-previous).count());previous=now;
    require(session.running(),"authority/session exited during block qualification");
    if(session.player_pose(pose)) {
      stream->request(PredictedBlocks::column(static_cast<int>(std::floor(pose.x))),
          PredictedBlocks::column(static_cast<int>(std::floor(pose.z))),1);
    }
    StreamColumn column;
    while(stream->poll(column))bases.insert_or_assign({column.x,column.z},std::move(column));
    const auto batch=session.block_receipts();
    if(!batch.session.empty()) {
      require(receipt_session.empty() || receipt_session==batch.session,"unexpected receipt session reset");
      receipt_session=batch.session;
      for(const auto& receipt:batch.receipts)if(receipt.sequence>sequence) {
        const auto expected=pending.find(receipt.commandID);
        require(expected!=pending.end(),"receipt does not correlate with a submitted command");
        require(receipt.accepted==expected->second.accepted,"unexpected authority acceptance/rejection");
        const auto cell=expected->second.edit.edit;
        const auto affected=std::find_if(receipt.blocks.begin(),receipt.blocks.end(),[&](const auto& value) {
          return value.x==cell.x && value.y==cell.y && value.z==cell.z;
        });
        require(affected!=receipt.blocks.end(),"receipt omitted authoritative requested-cell value");
        uint16_t authoritative=expected->second.edit.block;
        if(!receipt.accepted)require(base(cell,authoritative),"rejected receipt has no authoritative baseline");
        require(affected->block==authoritative,"receipt authoritative cell disagrees with expected command result");
        stream->resolve_block(receipt.commandID,receipt.accepted,receipt.revision);
        const auto dependency=dependencies.find(receipt.commandID);
        if(dependency!=dependencies.end()) {
          require(session.movement_stats().ack>=dependency->second,"block receipt preceded dependent movement consumption");
          if(dependency->second>expected->second.ack_before)++gated;
        }
        std::printf("block_receipt command=%llu sequence=%llu accepted=%d revision=%llu movement_dependency=%llu consumed=%llu cells=%zu\n",
            (unsigned long long)receipt.commandID,(unsigned long long)receipt.sequence,int(receipt.accepted),
            (unsigned long long)receipt.revision,(unsigned long long)(dependency==dependencies.end()?0:dependency->second),
            (unsigned long long)session.movement_stats().ack,receipt.blocks.size());
        results.emplace(receipt.commandID,receipt);sequence=receipt.sequence;pending.erase(expected);
      }
      if(sequence && !batch.receipts.empty())
        require(session.acknowledge_block_receipts(receipt_session,sequence),"production receipt acknowledgement failed");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  bool base(BlockPosition p,uint16_t& value,uint64_t* revision=nullptr,uint64_t* authority=nullptr) const {
    const auto found=bases.find({PredictedBlocks::column(p.x),PredictedBlocks::column(p.z)});
    if(found==bases.end())return false;
    std::size_t index{};
    if(!predicted_block_index(found->second,p.x,p.y,p.z,index))return false;
    value=found->second.blocks[index];
    if(revision)*revision=found->second.revision;
    if(authority)*authority=found->second.authoritative_revision;
    return true;
  }
  uint16_t visible(BlockPosition p) const {
    uint16_t block{};require(stream->try_block(p.x,p.y,p.z,block),"target column not published");return block;
  }
  bool candidate(int dx,int dz,Cell& cell) {
    const int x=static_cast<int>(std::floor(pose.x))+dx,z=static_cast<int>(std::floor(pose.z))+dz;
    const float fx=static_cast<float>(x),fz=static_cast<float>(z);
    if(pose.x+.4f>fx && pose.x-.4f<fx+1 && pose.z+.4f>fz && pose.z-.4f<fz+1)return false;
    for(int y=static_cast<int>(std::floor(pose.y))-1;y>=static_cast<int>(std::floor(pose.y))-6;--y) {
      uint16_t block{},above{},below{};
      const float ax=fx+.5f-pose.x,ay=static_cast<float>(y)+.5f-pose.y,az=fz+.5f-pose.z;
      if(ax*ax+ay*ay+az*az>25 || !base({x,y,z},block) || !block || !catalog.blocks_camera(block) ||
          !base({x,y+1,z},above) || above || !base({x,y-1,z},below) || !below ||
          !catalog.blocks_camera(below) || !catalog.select(block))continue;
      cell={{x,y,z},block};return true;
    }
    return false;
  }
  Cell far_cell() {
    const int cx=PredictedBlocks::column(static_cast<int>(std::floor(pose.x)));
    const int x=static_cast<int>(std::floor(pose.x))+((pose.x-static_cast<float>(cx*32)<16)?12:-12);
    const int z=static_cast<int>(std::floor(pose.z));
    for(int y=static_cast<int>(std::floor(pose.y))+16;y>=static_cast<int>(std::floor(pose.y))-32;--y) {
      uint16_t block{};
      if(base({x,y,z},block) && block && catalog.blocks_camera(block))return {{x,y,z},block};
    }
    throw std::runtime_error("no delivered solid out-of-reach test cell");
  }
  uint64_t submit(const Cell& cell,bool place,bool accepted=true) {
    BlockEditIntent edit;
    edit.edit=cell.position;edit.hit=cell.position;
    if(place)--edit.hit.y;
    edit.block=place?cell.original:0;
    edit.camera_x=pose.x;edit.camera_y=pose.y;edit.camera_z=pose.z;
    uint64_t command{};
    until([&] {
      require(stream->can_predict(),"prediction admission unexpectedly full");
      return session.submit_block_edit(edit,&command);
    },"block intent submission timeout");
    pending.emplace(command,Pending{edit,accepted,session.movement_stats().ack});
    require(stream->predict_block(command,edit.edit.x,edit.edit.y,edit.edit.z,edit.block),"production stream prediction admission failed");
    require(visible(cell.position)==edit.block,"missing immediate targeting prediction");
    std::printf("block_submit command=%llu cell=(%d,%d,%d) block=%u expected=%d ack_before=%llu\n",
        (unsigned long long)command,cell.position.x,cell.position.y,cell.position.z,unsigned(edit.block),int(accepted),
        (unsigned long long)pending.at(command).ack_before);
    return command;
  }
  BlockReceipt complete(uint64_t command,const Cell& cell,uint16_t expected) {
    until([&]{return results.contains(command);},"command-correlated receipt timeout");
    const auto receipt=results.at(command);
    if(visible(cell.position)!=expected && receipt.accepted) {
      uint16_t block{};uint64_t content_hash{};base(cell.position,block,&content_hash);
      ++feedback_failures;
      std::printf("block_feedback FAIL command=%llu visible=%u expected=%u authoritative=%u content_hash=%llu receipt_revision=%llu\n",
          (unsigned long long)command,unsigned(visible(cell.position)),unsigned(expected),unsigned(block),
          (unsigned long long)content_hash,(unsigned long long)receipt.revision);
    }
    if(!receipt.accepted)require(visible(cell.position)==expected,"rejection did not restore exact authoritative WorldStream value");
    if(receipt.accepted)until([&] {
      uint16_t block{};uint64_t authority{};
      return base(cell.position,block,nullptr,&authority) && block==expected && authority>=receipt.revision;
    },"accepted edit never reached covering authoritative baseline",60);
    require(visible(cell.position)==expected,"covering baseline changed predicted result");
    return receipt;
  }
};
}

int main(int argc,char** argv) {
  try {
    require(argc==4 || argc==5,"arguments: canonical bundle fresh-world logs [host:port]");
    require(!std::filesystem::exists(argv[2]),"probe requires a fresh isolated world/cache directory");
    Probe probe;
    require(probe.catalog.load_catalog(std::filesystem::path(argv[1])/"Data/Blocks/octaryn.basegame.blocks.json"),"load production block catalog");
    require(argc==5?probe.session.start_remote(argv[1],argv[2],1,argv[4],argv[3]):
        probe.session.start(argv[1],argv[2],1,argv[3]),"start production LocalSession");
    probe.stream=std::make_unique<WorldStream>(probe.session.chunk_stream_path());
    Cell first,second;
    probe.until([&]{
      return probe.session.player_pose(probe.pose) && probe.pose.on_ground &&
          probe.candidate(1,0,first) && probe.candidate(2,0,second);
    },"spawn/terrain/nearby editable cells timeout",90);
    std::printf("block_probe_ready transport=%s pose=(%.3f,%.3f,%.3f) first=(%d,%d,%d) original=%u\n",
        argc==5?"remote":"local",probe.pose.x,probe.pose.y,probe.pose.z,first.position.x,first.position.y,first.position.z,unsigned(first.original));

    // Submit immediately after a domain movement sample, without waiting for its ack.
    probe.input.forward=true;
    std::this_thread::sleep_for(std::chrono::milliseconds(18));probe.pump();
    probe.input.forward=false;
    const auto broken=probe.submit(first,false);
    probe.complete(broken,first,0);
    const auto restored=probe.submit(first,true);
    const auto baseline=probe.complete(restored,first,first.original);
    const auto far=probe.far_cell();
    const auto rejected=probe.submit(far,false,false);
    const auto rejection=probe.complete(rejected,far,far.original);
    require(rejection.revision==baseline.revision,"reach rejection unexpectedly changed authoritative revision");
    std::printf("block_rollback command=%llu unchanged_revision=%llu restored=%u\n",
        (unsigned long long)rejected,(unsigned long long)rejection.revision,unsigned(probe.visible(far.position)));

    const auto rapid_first=probe.submit(first,false),rapid_second=probe.submit(second,false);
    probe.complete(rapid_first,first,0);probe.complete(rapid_second,second,0);
    const auto restore_first=probe.submit(first,true),restore_second=probe.submit(second,true);
    probe.complete(restore_first,first,first.original);probe.complete(restore_second,second,second.original);
    probe.until([&]{return !probe.receipt_session.empty() && probe.session.block_receipts().receipts.empty();},
        "acknowledged results not retired by authority");
    require(probe.gated>0,"no captured block command exercised an unacknowledged movement dependency");
    require(probe.pending.empty(),"unresolved client block predictions remain");
    probe.stream->reset_predictions();
    require(probe.visible(first.position)==first.original && probe.visible(second.position)==second.original &&
        probe.visible(far.position)==far.original,"reset exposed a permanent predicted ghost");
    std::printf("client_block_actions_checks receipts=%zu movement_gated=%llu exact_rollback=1 unchanged_revision=1 ack_retired=1 originals_restored=1 feedback_failures=%u\n",
        probe.results.size(),(unsigned long long)probe.gated,probe.feedback_failures);
    probe.session.stop();
    require(probe.feedback_failures==0,"accepted receipt prematurely retired prediction before authoritative baseline (see block_feedback)");
    std::printf("client_block_actions PASS transport=%s receipts=%zu movement_gated=%llu valid_break_place exact_rollback unchanged_revision rapid_edits baseline_cover ack_retirement originals_restored\n",
        argc==5?"remote":"local",probe.results.size(),(unsigned long long)probe.gated);
    return 0;
  } catch(const std::exception& error) {
    std::fprintf(stderr,"client_block_actions FAIL reason=%s\n",error.what());return 1;
  }
}
