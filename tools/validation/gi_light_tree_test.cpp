#include "GILights.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numeric>

using namespace octaryn::client::rendering;
namespace {
unsigned checks=0;
void require(bool condition,const char* message) {
  ++checks;if(!condition) {std::fprintf(stderr,"GI light tree: %s\n",message);std::exit(1);}
}
bool near(double a,double b,double tolerance=1e-6) {return std::abs(a-b)<=tolerance*std::max({1.,std::abs(a),std::abs(b)});}
WorldLocalLight point(float x,float power=1) {
  WorldLocalLight light;light.position_range={x,0,0,100};light.color_intensity={1,1,1,power};return light;
}
unsigned audit(const std::vector<GILightNode>& nodes,unsigned index,std::vector<unsigned>& seen) {
  require(index<nodes.size(),"child in bounds");const auto& node=nodes[index];
  require(node.bounds_min_flux[3]>0 && std::isfinite(node.bounds_min_flux[3]),"positive finite flux");
  for(unsigned axis=0;axis<3;++axis)require(node.bounds_min_flux[axis]<=node.bounds_max[axis],"ordered bounds");
  if(node.links[3]==1) {require(node.links[2]<seen.size(),"original light identity");++seen[node.links[2]];return 0;}
  require(node.links[0]>index && node.links[1]>index,"acyclic forward children");
  const unsigned left=node.links[0],right=node.links[1];
  const unsigned depth=1+std::max(audit(nodes,left,seen),audit(nodes,right,seen));
  require(node.links[3]==nodes[left].links[3]+nodes[right].links[3],"descendant count");
  require(std::abs(int(nodes[left].links[3])-int(nodes[right].links[3]))<=1,"balanced median partition");
  require(near(node.bounds_min_flux[3],double(nodes[left].bounds_min_flux[3])+nodes[right].bounds_min_flux[3]),"merged flux");
  for(unsigned child:{left,right})for(unsigned axis=0;axis<3;++axis)
    require(node.bounds_min_flux[axis]<=nodes[child].bounds_min_flux[axis] &&
        node.bounds_max[axis]>=nodes[child].bounds_max[axis],"parent contains child emitter bounds");
  return depth;
}
// Independent scalar evaluation of the emitted node layout and sampling PDF.
double importance(const GILightNode& node,const std::array<double,3>& position) {
  double distance=0;
  for(unsigned axis=0;axis<3;++axis) {
    const double d=std::max({double(node.bounds_min_flux[axis])-position[axis],position[axis]-node.bounds_max[axis],0.});
    distance+=d*d;
  }
  return node.bounds_min_flux[3]/std::max(distance,.25);
}
void probabilities(const std::vector<GILightNode>& nodes,unsigned index,const std::array<double,3>& position,
    double pdf,std::vector<double>& output) {
  const auto& node=nodes[index];
  if(node.links[3]==1) {output[node.links[2]]=pdf;return;}
  const auto left=node.links[0],right=node.links[1];
  const double a=importance(nodes[left],position),b=importance(nodes[right],position);
  const double p=std::clamp(a+b>0?a/(a+b):.5,1./64,63./64);
  probabilities(nodes,left,position,pdf*p,output);probabilities(nodes,right,position,pdf*(1-p),output);
}
std::vector<double> distribution(const std::vector<GILightNode>& nodes,unsigned count,std::array<double,3> position={}) {
  std::vector<double> pdf(count);probabilities(nodes,0,position,1,pdf);
  require(near(std::accumulate(pdf.begin(),pdf.end(),0.),1.,1e-12),"full probability sums to one");return pdf;
}
}
int main() {
  require(build_gi_light_tree({}).empty(),"empty light set");
  std::vector<WorldLocalLight> lights{point(0,0),point(2),point(3,-1)};
  auto nodes=build_gi_light_tree(lights);require(nodes.size()==1 && nodes[0].links[2]==1,"zero energy skipped without renumbering");
  require(distribution(nodes,3)[1]==1,"one-light probability");
  lights={point(-2),point(2)};nodes=build_gi_light_tree(lights);
  auto pdf=distribution(nodes,2);require(near(pdf[0],.5) && near(pdf[1],.5),"equal symmetric lights");
  pdf=distribution(nodes,2,{-1.99,0,0});require(pdf[0]>.98 && pdf[1]>0,"near light importance preserves distant support");
  // Exact expected value/pdf recovers each channel for arbitrary importance.
  const double red[2]={1,4},green[2]={3,2},blue[2]={.25,9};
  for(const double* channel:{red,green,blue}) {
    double estimate=0;for(unsigned i=0;i<2;++i)estimate+=pdf[i]*(channel[i]/pdf[i]);
    require(near(estimate,channel[0]+channel[1]),"importance correction preserves RGB energy");
  }
  lights={point(0,1e-30f),point(0,1e30f)};nodes=build_gi_light_tree(lights);pdf=distribution(nodes,2);
  require(near(pdf[0],1./64) && pdf[1]>0,"extreme flux still has positive support");
  lights.assign(7,point(0));nodes=build_gi_light_tree(lights);pdf=distribution(nodes,7);
  for(double p:pdf)require(near(p,1./7),"non-power-of-two coincident equal lights");
  WorldLocalLight rectangle=point(10);rectangle.axis_u_inner={2,0,0,.9f};rectangle.axis_v_type={0,0,3,2};
  lights={point(10),rectangle};nodes=build_gi_light_tree(lights);
  const auto rect=std::find_if(nodes.begin(),nodes.end(),[](const GILightNode& node){return node.links[3]==1 && node.links[2]==1;});
  require(rect!=nodes.end() && rect->bounds_min_flux[0]==8 && rect->bounds_max[0]==12 &&
      rect->bounds_min_flux[2]==-3 && rect->bounds_max[2]==3,"rectangle emitter bounds");
  pdf=distribution(nodes,2,{10,0,0});require(near(pdf[1]/pdf[0],24),"rectangle flux includes actual emitting area");
  lights.clear();lights.reserve(65536);
  for(unsigned i=0;i<65536;++i)lights.push_back(point(float(i%256)-128));
  nodes=build_gi_light_tree(lights);require(nodes.size()==131071,"maximum complete tree size");
  std::vector<unsigned> seen(lights.size());require(audit(nodes,0,seen)==16,"maximum depth matches shader traversal");
  for(unsigned count:seen)require(count==1,"each source appears exactly once");
  pdf=distribution(nodes,65536,{-127.5,1,0});
  for(double p:pdf)require(p>0 && std::isfinite(p) && float(p)>0,"full PDF remains representable");
  std::printf("gi_light_tree passed checks=%u max_lights=65536 max_depth=16 gpu_runtime=false\n",checks);return 0;
}
