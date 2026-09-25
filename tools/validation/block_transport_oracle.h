#pragma once
#include <array>
#include <cstddef>
#include <vector>

// Independent double-precision reference: enumerate complete light paths.
namespace block_transport_oracle {
using Color=std::array<double,3>;
struct Surface {
  Color reflectance{},direct{};
  // Negative slots terminate without redistributing their probability.
  std::vector<int> targets;
  bool valid{true};
};
using Scene=std::vector<Surface>;
struct Result {
  std::array<Color,3> outgoing{};
  Color indirect{};
};
inline void paths(const Scene& scene,std::size_t receiver,unsigned remaining,
    Color throughput,Color& result) {
  const auto& surface=scene.at(receiver);
  if(!surface.valid)return;
  if(!remaining) {
    for(unsigned c=0;c<3;++c)result[c]+=throughput[c]*surface.direct[c];
    return;
  }
  if(surface.targets.empty())return;
  const double probability=1./double(surface.targets.size());
  for(const int index:surface.targets) {
    if(index<0 || std::size_t(index)>=scene.size())continue;
    const auto& target=scene[std::size_t(index)];
    if(!target.valid)continue;
    Color next{};
    for(unsigned c=0;c<3;++c)next[c]=throughput[c]*probability*target.reflectance[c];
    paths(scene,std::size_t(index),remaining-1,next,result);
  }
}
inline std::vector<Result> evaluate(const Scene& scene) {
  std::vector<Result> results(scene.size());
  for(std::size_t i=0;i<scene.size();++i) {
    if(!scene[i].valid)continue;
    for(unsigned order=0;order<3;++order) {
      paths(scene,i,order,scene[i].reflectance,results[i].outgoing[order]);
      paths(scene,i,order+1,Color{1,1,1},results[i].indirect);
    }
  }
  return results;
}
inline Scene chain(unsigned slots=16) {
  Scene scene(4);
  scene[0].reflectance={.2,.4,.8};
  scene[1].reflectance={.75,.25,.5};
  scene[2].reflectance={.3,.6,.9};
  scene[3].reflectance={.8,.7,.2};
  scene[3].direct={8,4,2};
  for(unsigned i=0;i<scene.size();++i) {
    scene[i].targets.assign(slots,-1);
    if(i+1<scene.size())for(unsigned sample=0;sample<slots;++sample)
      scene[i].targets[sample]=int(i+1);
  }
  return scene;
}
inline Scene mixed(unsigned slots=16) {
  Scene scene(5);
  for(unsigned i=0;i<scene.size();++i) {
    scene[i].reflectance={double(i+1)/8,double(i+2)/9,double(i+3)/10};
    scene[i].direct={double(i%2)*3,double(i%3)*2,double(i%4)};
    scene[i].targets.resize(slots);
    for(unsigned sample=0;sample<slots;++sample)
      scene[i].targets[sample]=sample%5==0?-1:int((i+sample+1)%scene.size());
  }
  return scene;
}
}
