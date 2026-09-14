#include "WorldItemsClient.h"
#include "ItemFiles.h"
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <string>
using namespace octaryn::client::world_presentation;
namespace wi=octaryn::world_items;
namespace {
unsigned checks{};
void require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
template<class F> void until(F condition,const char* message) {
  const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
  while(!condition()){if(std::chrono::steady_clock::now()>=end)throw std::runtime_error(message);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));}++checks;
}
void run(const std::filesystem::path& directory) {
  std::filesystem::create_directories(directory/"runtime");
  wi::State server;server.items[0]={1,4,8,0,2,0,0,0,0,0,2};server.item_count=1;server.next_item=2;
  const auto snapshot=directory/"runtime/world_items.snapshot",intent=directory/"runtime/world_items.intent";
  item_files::write(snapshot,&server,sizeof(server));
  {
    WorldItemsClient client(directory);
    until([&]{return client.snapshot()->items.size()==1;},"initial complete server snapshot");
    require(client.submit_drop(4,999),"whole inventory stack queued as one bounded command");
    require(!client.submit_drop(4,8),"second unacknowledged toss rejected");
    wi::Intent request;
    until([&]{return item_files::read(intent,&request,sizeof(request))&&request.command==1;},"worker publishes toss");
    wi::Intent outbox;
    require(item_files::read(directory/"client/world_items.pending",&outbox,sizeof(outbox))&&outbox.command==request.command&&
      outbox.count==999&&request.count==999,"durable outbox and intent preserve all999 units");
    server.last_command=1;server.receipt_block=4;server.receipt_count=999;server.receipt_result=wi::DropResult::Accepted;
    item_files::write(snapshot,&server,sizeof(server));
    DropReceipt receipt;
    until([&]{return client.drop_receipt(receipt);},"actual server receipt delivered");
    require(receipt.accepted()&&receipt.count==999,"receipt preserves whole-stack acceptance and count");
    require(client.drop_receipt(receipt),"uncommitted receipt remains available");
  }
  {
    WorldItemsClient client(directory);DropReceipt receipt;
    until([&]{return client.drop_receipt(receipt);},"restart replays durable pending receipt");
    require(receipt.count==999&&receipt.accepted(),"restart retains whole-stack receipt count");
    client.acknowledge_drop(receipt.command_id);
    require(!client.drop_receipt(receipt),"ack retires committed receipt");
    server.grants[0]={1,4,8};server.grants[1]={2,5,3};server.grant_count=2;server.next_grant=3;
    item_files::write(snapshot,&server,sizeof(server));
    PickupGrant grant;
    until([&]{return client.next_pickup(grant);},"pickup receipt available");
    require(grant.id==1&&grant.count==8,"oldest grant first");
    client.acknowledge_pickup(2);
    require(client.next_pickup(grant)&&grant.id==1,"cannot acknowledge future pickup");
    client.acknowledge_pickup(1);
    require(!client.next_pickup(grant),"sent ack waits for authoritative grant removal");
    wi::Intent request;
    until([&]{return item_files::read(intent,&request,sizeof(request))&&request.acknowledge==1;},"worker publishes pickup ack");
    server.grants[0]=server.grants[1];server.grants[1]={};server.grant_count=1;server.acknowledged_grant=1;
    item_files::write(snapshot,&server,sizeof(server));
    until([&]{return client.next_pickup(grant)&&grant.id==2;},"next grant only after server advances");
    auto invalid=server;invalid.item_count=257;item_files::write(snapshot,&invalid,sizeof(invalid));
    until([&]{return client.status()=="invalid_world_items_snapshot";},"worker rejects malformed bounded snapshot");
    require(client.snapshot()->items.size()==1,"coherent previous snapshot retained on malformed input");
    require(!client.submit_drop(0,1)&&!client.submit_drop(4,1000),"invalid client count/id never queued");
  }
}
}
int main(int argc,char** argv){try{if(argc!=2)throw std::runtime_error("pass isolated fixture directory");
  const auto run_id=std::chrono::steady_clock::now().time_since_epoch().count();
  run(std::filesystem::path(argv[1])/("run-"+std::to_string(run_id)));
  std::printf("world_items_client=passed checks=%u\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"world_items_client=failed reason=%s\n",e.what());return 1;}}
