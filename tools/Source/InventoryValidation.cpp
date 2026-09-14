#include "Inventory.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

using namespace octaryn::client::app;
namespace {
unsigned checks{}, failures{};
void check(bool condition,const char* name) {
  ++checks;
  if (!condition) { ++failures;std::fprintf(stderr,"inventory_check=failed name=%s\n",name); }
}
std::string read(const std::filesystem::path& path) {
  std::ifstream file(path,std::ios::binary);
  return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
void write(const std::filesystem::path& path,const std::string& text) {
  std::ofstream file(path,std::ios::binary);file<<text;
}
void replace(std::string& text,const std::string& from,const std::string& to) {
  text.replace(text.find(from),from.size(),to);
}
}
int main(int argc,char** argv) {
  if (argc!=3) return 2;
  const std::filesystem::path root=argv[1],output=argv[2];
  std::filesystem::create_directories(output);
  const auto catalog=root/"octaryn-basegame/Data/Blocks/octaryn.basegame.blocks.json";
  const auto hand=root/"octaryn-basegame/Data/Items/octaryn.basegame.item.hand.json";
  const auto save=output/"palette.json",invalid=output/"invalid.json";
  Inventory inventory;
  check(inventory.load_catalog(catalog,hand),"catalog loads");
  check(inventory.hand().available && inventory.hand().key=="octaryn.basegame.item.hand","catalog equipment");
  check(!inventory.blocks().empty() && inventory.slots().size()==50,"50 slots");
  check(!inventory.find(0) && !inventory.find(8) && !inventory.find(15),"exclude air cloud flowing water");
  check(inventory.search("tOrCh").size()==7,"case insensitive search");
  check(inventory.search("",InventoryCategory::Fluids).size()==2,"fluid source category");
  check(inventory.search("missing-block").empty(),"search no matches");
  check(inventory.search("Torch",InventoryCategory::Terrain).empty(),"combined filter");
  check(!inventory.select_hotbar(10) && !inventory.select_hotbar(UINT_MAX),"hotbar bounds");
  check(!inventory.assign(0) && !inventory.assign(65535) && !inventory.assign(15),"invalid assign");
  check(!inventory.pick(0) && !inventory.click_slot(50) && !inventory.clear_slot(UINT_MAX),"invalid mutations");
  const auto initial=inventory.slots();
  check(inventory.click_slot(0) && inventory.moving_slot()==0 && inventory.slots()[0]==0 &&
    inventory.cursor().block==initial[0] && inventory.cursor().count==Inventory::StackLimit,"pickup moves exact stack to cursor");
  inventory.cancel_move();
  check(inventory.moving_slot()==-1 && inventory.slots()==initial,"close cancels without loss");
  check(inventory.click_slot(0) && inventory.click_slot(11) && inventory.slots()[11]==initial[0] &&
    inventory.slots()[0]==0,"move to empty");
  check(inventory.click_slot(1) && inventory.click_slot(11) && inventory.slots()[11]==initial[1] &&
    inventory.cursor().block==initial[0],"swap occupied leaves displaced stack on cursor");
  inventory.cancel_move();check(inventory.slots()[1]==initial[0],"cancel returns displaced stack to original empty slot");
  check(inventory.click_slot(11) && inventory.clear_slot(11) && inventory.moving_slot()==-1,"clear cancels move");
  check(inventory.cycle_hotbar(-1) && inventory.selected_slot()==9,"hotbar backward wrap");
  check(inventory.cycle_hotbar(1) && inventory.selected_slot()==0,"hotbar forward wrap");
  inventory.cycle_hotbar(std::numeric_limits<int>::min());
  check(inventory.selected_slot()<10,"large negative cycle bounded");
  inventory.select_hotbar(0);inventory.assign(26);
  check(inventory.selected_block()==26,"creative assign");
  inventory.select_hotbar(9);
  check(inventory.pick(26) && inventory.selected_slot()==0,"pick existing hotbar slot");
  check(inventory.pick(27) && inventory.selected_block()==27,"pick replaces selected slot");
  inventory.click_slot(0);inventory.click_slot(20);inventory.assign(5);
  inventory.click_slot(0);inventory.click_slot(19);
  const auto before_sort=inventory.slots();
  check(inventory.sort_backpack(),"backpack sort changes arrangement");
  check(std::equal(before_sort.begin(),before_sort.begin()+10,inventory.slots().begin()),"sort preserves hotbar");
  auto expected=before_sort,actual=inventory.slots();
  std::sort(expected.begin(),expected.end());std::sort(actual.begin(),actual.end());
  check(expected==actual,"sort preserves contents");
  check(inventory.save_if_changed(save) && !inventory.dirty(),"save clears dirty");
  const auto saved=read(save);
  const auto stamp=std::filesystem::last_write_time(save);
  inventory.click_slot(10);inventory.cancel_move();
  check(inventory.save_if_changed(save) && std::filesystem::last_write_time(save)==stamp,"unchanged does not rewrite");
  Inventory restored;restored.load_catalog(catalog,hand);
  check(restored.load(save) && restored.slots()==inventory.slots() &&
    restored.selected_slot()==inventory.selected_slot() && !restored.dirty(),"persistence roundtrip");
  inventory.assign(1);
  check(inventory.save_if_changed(save) && read(save)!=saved,"atomic replace existing save");
  auto broken=saved;replace(broken,"octaryn.basegame.block.","unknown.block.");write(invalid,broken);
  const auto prior_slots=restored.slots();
  check(!restored.load(invalid) && restored.slots()==prior_slots,"unknown saved ID fails without replacing prior inventory");
  write(invalid,"{broken");check(!restored.load(invalid),"corrupt JSON rejected");
  write(invalid,"{}");check(!restored.load(invalid),"missing fields rejected");
  write(invalid,"{\"schema\":\"octaryn.client.build-palette.v1\",\"selected\":50,\"slots\":[]}");
  check(!restored.load(invalid),"invalid saved bounds");
  write(invalid,std::string(32769,' '));check(!restored.load(invalid),"oversized save rejected");
  broken=read(catalog);replace(broken,"octaryn.basegame.block.grass","octaryn.basegame.block.dirt");write(invalid,broken);
  const auto count=restored.blocks().size();
  check(!restored.load_catalog(invalid,hand) && restored.blocks().size()==count,"duplicate catalog ID rejected transactionally");
  broken=read(catalog);replace(broken,"octaryn.basegame.block.grass","bad<id>");write(invalid,broken);
  check(!restored.load_catalog(invalid,hand),"malformed catalog ID");
  check(restored.load_catalog(catalog,output/"missing-hand.json") && !restored.hand().available,"no invented equipment");
  check(!restored.save_if_changed(output),"failed save reports failure");
  check(restored.dirty(),"failed save stays dirty");
  Inventory stacks;check(stacks.load_catalog(catalog,hand),"stack fixture catalog");
  const auto id=stacks.slots()[0];
  check(stacks.right_click_slot(0) && stacks.cursor().count==1 && stacks.counts()[0]==998,"right click takes exactly one");
  check(stacks.right_click_slot(10) && stacks.counts()[10]==1 && !stacks.cursor().count,"right click places exactly one");
  stacks.click_slot(0);stacks.click_slot(10);
  check(stacks.counts()[10]==999 && !stacks.cursor().count,"same item stacks merge without loss");
  stacks.click_slot(10);stacks.right_click_slot(11);
  check(stacks.counts()[11]==1 && stacks.cursor().count==998,"held stack single deposit");
  check(stacks.drop_cursor(true).count==1 && stacks.cursor().count==997,"single drop bounded");
  check(stacks.save_if_changed(save),"cursor stack persisted");
  Inventory loaded;loaded.load_catalog(catalog,hand);
  check(loaded.load(save) && loaded.cursor().count==997 && loaded.cursor().block==id && loaded.counts()[11]==1,"cursor counts roundtrip");
  loaded.cancel_move();
  check(loaded.credit_grant(1,id,12),"ordered pickup grant credited");
  const auto credited=loaded.counts();
  check(loaded.credit_grant(1,id,12) && loaded.counts()==credited,"duplicate pickup grant not credited twice");
  check(loaded.save_if_changed(save) && stacks.load(save) && stacks.credit_grant(1,id,12) && stacks.counts()==credited,
    "grant dedupe survives save reload");
  for(unsigned i=0;i<50;++i) {stacks.clear_slot(i);}
  check(stacks.receive(id,50*Inventory::StackLimit)==0,"fill inventory exactly");
  const auto full=stacks.counts();
  check(!stacks.credit_grant(2,id,1) && stacks.counts()==full,"full pickup defers without partial credit or watermark advance");
  stacks.clear_slot(49);check(stacks.credit_grant(2,id,1),"same deferred pickup can retry after capacity opens");
  stacks.click_slot(0);
  check(stacks.reserve_drop(64) && !stacks.click_slot(4) && !stacks.right_click_slot(4),"drop reservation locks cursor mutation");
  const auto reserved=stacks.cursor();
  check(stacks.save_if_changed(save) && loaded.load(save) && loaded.reserved_drop()==64 &&
    loaded.cursor().count==reserved.count,"drop reservation survives restart");
  loaded.resolve_drop(false);check(loaded.cursor().count==reserved.count,"rejected drop preserves exact stack");
  check(loaded.reserve_drop(64),"rejected reservation can retry");
  loaded.resolve_drop(true,17);check(loaded.cursor().count==reserved.count-64 && !loaded.reserved_drop(),"accepted drop consumes exact reserved amount");
  check(loaded.save_if_changed(save) && stacks.load(save) && stacks.drop_watermark()==17,"drop receipt watermark survives restart");
  check(stacks.reserve_drop(1),"new drop after committed receipt");
  const auto new_count=stacks.cursor().count;stacks.resolve_drop(true,17);
  check(stacks.cursor().count==new_count && stacks.reserved_drop()==1,"old receipt cannot consume new reservation");
  write(invalid,"{broken");
  check(!stacks.load(invalid) && stacks.drop_watermark()==17 && stacks.reserved_drop()==1 &&
      stacks.cursor().count==new_count,"invalid save preserves durable receipt watermark and reservation");
  std::string legacy="{\"schema\":\"octaryn.client.build-palette.v1\",\"selected\":0,\"slots\":[";
  for(unsigned i=0;i<Inventory::SlotCount;++i) {
    if(i)legacy+=',';
    legacy+='\"';if(i==0)legacy+=stacks.find(id)->key;legacy+='\"';
  }
  legacy+="]}";write(invalid,legacy);
  check(loaded.load(invalid) && loaded.slots()[0]==id && loaded.counts()[0]==999 &&
      !loaded.drop_watermark() && !loaded.reserved_drop(),"legacy creative palette retains IDs with zero receipt state");
  loaded.click_slot(0);
  check(!loaded.reserve_drop(1000)&&loaded.reserve_drop(999),"whole stack reservation has exact999 maximum");
  check(loaded.save_if_changed(save)&&stacks.load(save)&&stacks.reserved_drop()==999&&stacks.cursor().count==999,
      "whole stack reservation survives restart with every unit held");
  stacks.resolve_drop(false,18);
  check(stacks.cursor().count==999&&!stacks.reserved_drop(),"rejected whole stack returns all999 units");
  check(stacks.reserve_drop(999),"whole stack retries after rejection");
  stacks.resolve_drop(true,19);
  check(!stacks.cursor().count&&!stacks.reserved_drop()&&stacks.drop_watermark()==19,
      "one accepted receipt consumes the complete999 stack");
  check(stacks.save_if_changed(save)&&loaded.load(save)&&!loaded.cursor().count&&loaded.drop_watermark()==19,
      "whole stack debit and receipt watermark persist atomically");
  loaded.resolve_drop(true,19);check(!loaded.cursor().count,"duplicate whole-stack receipt cannot debit twice");
  std::printf("inventory_contract=%s checks=%u failures=%u\n",failures?"failed":"passed",checks,failures);
  return failures?1:0;
}
