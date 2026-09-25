#include "BlockTransportBoundary.h"
namespace mesh_probe {
void block_transport_boundary_cases(Fixture& fixture) {
  using namespace boundary;auto& r=fixture.renderer;
  unsigned stone=0;for(unsigned i=1;i<fixture.catalog.size();++i)if(fixture.catalog[i].id=="octaryn.basegame.block.stone")stone=i;
  require(stone!=0,"BT boundary room stone catalog material");open_world_renderer_set_center(&r,0,0,0);
  auto room=column();const auto previous=r.sources.find({0,0});room.revision=previous==r.sources.end()?1:previous->second.revision+1;
  for(int z=8;z<=13;++z)for(int y=0;y<=5;++y)for(int x=8;x<=13;++x)
    if(x==8 || x==13 || y==0 || y==5 || z==8 || z==13)put(room,x,y,z,std::uint16_t(stone));
  room.blocks.compact();require(open_world_renderer_update(&r,room),"BT boundary room publication");plant_probe::settle(r);
  std::vector<Query> queries;
  const int coordinates[]={-16777217,-16777216,-8388608,-4194304,-65536,-32768,-32767,-16,-1,0,1,15,16,32767,32768,65536,4194304,8388607,8388608,16777215,16777216,INT32_MIN,INT32_MAX};
  for(int cell:coordinates)for(unsigned face=0;face<6;++face)
    for(const auto uv:{std::array<float,2>{0,0},{1,1},{0,1},{1,0},{.5f,.5f}}) {
      Query query{};query.key={cell,cell,cell,face};query.info[0]=1;query.origin={uv[0],uv[1],0,0};queries.push_back(query);
    }
  const auto actual=dispatch(fixture,queries);unsigned large=0,rejected=0;
  for(unsigned i=0;i<actual.size();++i) {
    check_position(queries[i],actual[i]);
    if(actual[i].origin[3]==0)++rejected;
    if((queries[i].key.x==-32768 || queries[i].key.x==32768) && actual[i].origin[3]!=0)++large;
  }
  require(large==60 && rejected>0,"BT hostile coordinate fixture missed supported large cells or precision rejection");
  Query rounded{};rounded.key={-3,7,-11,1};rounded.info={0,173,0,2768};
  const auto regression=dispatch(fixture,{rounded});
  require(regression[0].position[2]<-10 && regression[0].position[2]>-11,
      "BT ordinal2768 direct sample still rounds onto adjacent face");
  std::vector<Query> rays;
  Query near{};near.info[0]=2;near.origin={10.3758049f,1.00100005f,9.00015068f,32};
  near.direction={-.0318049379f,.762626231f,-.64605695f,0};rays.push_back(near);
  auto legacy=near;legacy.direction[3]=-1;rays.push_back(legacy);
  auto short_range=near;short_range.origin[3]=.00024f;rays.push_back(short_range);
  short_range.direction[3]=-1;rays.push_back(short_range);
  auto sub_bias=near;sub_bias.origin={10.5f,1.5f,std::nextafter(9.f,INFINITY),.00001f};
  sub_bias.direction={0,0,-1,0};rays.push_back(sub_bias);
  auto empty=near;empty.origin[3]=0;rays.push_back(empty);
  auto sampled=near;sampled.key={10,0,9,3};sampled.info={3,29,15,29};sampled.direction[3]=0;rays.push_back(sampled);
  const auto hits=dispatch(fixture,rays);
  const double norm=std::sqrt(double(near.direction[0])*near.direction[0]+double(near.direction[1])*near.direction[1]+double(near.direction[2])*near.direction[2]);
  const double distance=(double(near.origin[2])-9)*norm/-near.direction[2];
  for(unsigned i:{0u,2u})require(hits[i].hit[0]==1 && hits[i].hit[2]==0 && hits[i].hit[3]==1 &&
      std::abs(hits[i].hit[1]-distance)<2e-7,"BT positive blocker below legacy TMin was skipped");
  require(hits[1].hit[0]==1 && hits[1].hit[1]>.1f && hits[1].hit[2]==1,
      "BT explicit GI interval changed legacy caller TMin semantics");
  require(hits[3].hit[0]==0 && hits[5].hit[0]==0,"BT empty ray interval did not return before traversal");
  require(hits[4].hit[0]==1 && hits[4].hit[3]==1 &&
      std::abs(hits[4].hit[1]-(sub_bias.origin[2]-9))<1e-9f,"BT sub-bias narrow gap lost its actual blocker");
  require(hits[6].origin[3]==1 && hits[6].hit[0]==1 && hits[6].hit[2]==0 && hits[6].hit[3]==1 &&
      hits[6].hit[1]>0 && hits[6].hit[1]<.00025f,"BT actual batch29 slot15 still loses its perpendicular wall");
  require(r.debug.errors.load()==0,"BT boundary graphics validation errors");
  std::printf("block_transport_boundary=passed hardware=1 production_sampling=1 production_query=1 ieee_domain=1 signed_coordinates=1 large_32768=1 precision_fail_closed=1 outward_ulp=1 rounded_owner=1 near_wall=1 narrow_gap=1 empty_interval=1 legacy_interval=1 hostile_samples=%zu ray_cases=%zu validation_errors=0\n",queries.size()+1,rays.size());
}
}
