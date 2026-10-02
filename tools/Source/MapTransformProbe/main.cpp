#include "MapTransformDiagnostics.h"
#include <limits>
using namespace octaryn::client::rendering;
using namespace octaryn::client::rendering::virtual_geometry;
int main(){
 unsigned checks{};auto check=[&](bool ok){++checks;if(!ok){std::printf("FAIL %u\n",checks);std::exit(1);}};
 MapPrimitive draw;draw.source.local_bounds={0,0,0,1,1,1};draw.source.evaluated[12]=10;draw.source.evaluated[13]=20;draw.source.evaluated[14]=30;
 for(unsigned a=0;a<3;++a){draw.bounds_min[a]=float(10*(a+1));draw.bounds_max[a]=draw.bounds_min[a]+1;}
 GeometryTransform unit,posed;std::string error;check(geometry_transform(draw.source.evaluated,posed,error));
 auto valid=[](const auto& d){return d.finite&&d.orientation_valid&&d.normal_valid&&d.source_bounds_valid&&d.source_transform_valid;};
 check(valid(map_transform_source_diagnostic(draw.source,draw,unit,false,0)));
 auto bad=unit;bad.world[3]=1;check(!map_transform_source_diagnostic(draw.source,draw,bad,false,0).source_transform_valid);
 bad=unit;bad.normal[0]=2;check(!map_transform_source_diagnostic(draw.source,draw,bad,false,0).normal_valid);
 bad=unit;bad.orientation=-1;check(!map_transform_source_diagnostic(draw.source,draw,bad,false,0).orientation_valid);
 bad=unit;bad.world[0]=std::numeric_limits<float>::infinity();check(!map_transform_source_diagnostic(draw.source,draw,bad,false,0).finite);
 auto escaped=draw;escaped.bounds_max[0]+=2;check(!map_transform_source_diagnostic(draw.source,escaped,unit,false,0).source_bounds_valid);
 auto object=draw;for(unsigned a=0;a<3;++a){object.bounds_min[a]=0;object.bounds_max[a]=1;}
 check(valid(map_transform_source_diagnostic(draw.source,object,posed,true,0)));
 bad=posed;bad.world[3]+=1;check(!map_transform_source_diagnostic(draw.source,object,bad,true,0).source_transform_valid);
 auto moved=draw.source;moved.evaluated[12]+=7;check(geometry_transform(moved.evaluated,posed,error));check(valid(map_transform_source_diagnostic(moved,object,posed,true,0)));
 moved.evaluated[0]=-2;moved.evaluated[4]=.2f;moved.evaluated[5]=3;check(geometry_transform(moved.evaluated,posed,error));check(valid(map_transform_source_diagnostic(moved,object,posed,true,0)));
 auto reversed=draw;reversed.source.local_bounds[3]=-1;check(!map_transform_source_diagnostic(reversed.source,reversed,unit,false,0).source_bounds_valid);
 auto singular=unit;singular.world[0]=0;check(!map_transform_source_diagnostic(draw.source,draw,singular,false,0).orientation_valid);
 _putenv_s("OCTARYN_CLIENT_TRANSFORM_DIAGNOSTICS","1");MapRenderer map;map.model.primitives.push_back(draw);check(map_transform_diagnostics_snapshot(map));
 struct Camera{float x{},y{},z{},yaw{},pitch{};} camera;check(map_transform_diagnostics_frame(map,0,camera,true,1,1,1,"primary_fixture"));
 map.geometry_instances.push_back(unit);map.instance_sources.push_back(draw.source);check(!map_transform_diagnostics_frame(map,120,camera,true,1,1,1,"primary_fixture"));
 std::printf("PASS %u adversarial assertions\n",checks);return 0;
}
