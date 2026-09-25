#include "PlayerModel.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

void check_player_presentation();
void check_pose_history_metadata();
void check_stream_residency();
void check_column_blocks();
void check_action_feedback();

namespace {
using namespace octaryn::client::rendering;
using namespace fastgltf::math;
using Palette=std::array<fmat4x4,PlayerMaxJoints>;
void require(bool value,const char* error) {if(!value) throw std::runtime_error(error);}
float difference(const Palette& a,const Palette& b,size_t count) {
  float result=0;
  for(size_t j=0;j<count;++j) for(size_t c=0;c<4;++c) for(size_t r=0;r<4;++r)
    result=std::max(result,std::abs(a[j][c][r]-b[j][c][r]));
  return result;
}
void check_transitions(const PlayerModel& model) {
  PlayerLocalPose from,to,middle;from.count=to.count=1;
  to.nodes[0].translation=fvec3(2.f,4.f,6.f);to.nodes[0].scale=fvec3(3.f);
  to.nodes[0].rotation=fquat(0.f,1.f,0.f,0.f);
  require(blend_player_local_pose(from,to,0,middle) && middle.nodes[0].rotation[3]==1.f,
          "blend start must preserve outgoing rotation");
  require(blend_player_local_pose(from,to,1,middle) && middle.nodes[0].translation[2]==6.f &&
          middle.nodes[0].rotation[1]==1.f,"blend end must preserve target TRS");
  require(blend_player_local_pose(from,to,.5f,middle),"midpoint blend");
  const auto& node=middle.nodes[0];float norm=0;
  for(size_t i=0;i<4;++i) norm+=node.rotation[i]*node.rotation[i];
  require(std::abs(norm-1.f)<1e-6f && std::abs(std::abs(node.rotation[1])-.70710678f)<1e-6f &&
          std::abs(std::abs(node.rotation[3])-.70710678f)<1e-6f,"midpoint rotation must remain unit quaternion");
  require(node.translation==fvec3(1.f,2.f,3.f) && node.scale==fvec3(2.f),"local translation/scale midpoint");
  // A halfway matrix lerp between identity and a 180-degree turn collapses X/Z.
  // Building the hierarchy after quaternion blending must preserve their lengths.
  PlayerModel synthetic;synthetic.nodes.resize(1);synthetic.joints={0};synthetic.inverse_bind.emplace_back();
  Palette actual,expected;
  require(build_player_skin(synthetic,middle,actual),"blended hierarchy");
  for(size_t c=0;c<3;++c) {
    float length=0;for(size_t r=0;r<3;++r) length+=actual[0][c][r]*actual[0][c][r];
    require(std::abs(length-4.f)<1e-5f,"local quaternion blend must not collapse basis");
  }
  PlayerAnimator animator;
  require(animator.sample(model,"walk_loop",0,10,actual) && animator.sample(model,"walk_loop",0,10.4,actual),"initial movement");
  expected=actual;
  require(animator.sample(model,"idle_loop",0,10.4,actual) && difference(actual,expected,model.joints.size())<1e-6f,
          "movement switch must preserve exact outgoing pose");
  require(animator.sample(model,"idle_loop",0,10.46,expected),"partial transition");
  require(animator.sample(model,"run_loop",0,10.46,actual) && difference(actual,expected,model.joints.size())<1e-6f,
          "interrupted transition must preserve blended outgoing pose");
  require(animator.sample(model,"run_loop",0,10.46,actual) && difference(actual,expected,model.joints.size())<1e-6f,
          "held source clock must hold transition");
  require(animator.sample(model,"run_loop",0,10.580001,actual) && sample_player_skin(model,"run_loop",.120001,expected) &&
          difference(actual,expected,model.joints.size())<1e-5f,"completed transition must match authored target clock");
  require(animator.sample(model,"attack_slash_once",1,11,actual) && animator.sample(model,"attack_slash_once",1,11.5,expected),"one-shot start");
  require(animator.sample(model,"attack_slash_once",2,11.5,actual) && difference(actual,expected,model.joints.size())<1e-6f,
          "one-shot restart must transition continuously");
  require(animator.sample(model,"attack_slash_once",2,11.620001,actual) && sample_player_skin(model,"attack_slash_once",.120001,expected) &&
          difference(actual,expected,model.joints.size())<1e-5f,"one-shot sequence must restart authored clock");
  require(animator.sample(model,"idle_loop",0,1,actual) && sample_player_skin(model,"idle_loop",0,expected) &&
          difference(actual,expected,model.joints.size())<1e-6f,"source clock reset must reset animation");
}
}
int main(int argc,char** argv) {
  try {
    require(argc==3,"provide retained glTF and isolated scratch directory");
    PlayerModel model;std::string error;
    require(load_player_model(argv[1],model,error),error.c_str());
    require(model.vertices.size()==144 && model.indices.size()==216 && model.primitives.size()==3,
            "retained geometry counts changed");
    require(model.nodes.size()==9 && model.joints.size()==8 && model.animations.size()==8,"retained rig/clip counts changed");
    size_t hidden_indices=0;
    for(const auto& primitive:model.primitives) {
      hidden_indices+=primitive.count-primitive.first_person_count;
      require(primitive.color==std::array<float,4>{1,1,1,1} && primitive.metallic==0 && primitive.roughness==1 &&
          std::abs(primitive.alpha_cutoff-.05f)<1e-6,"authored material changed");
    }
    require(hidden_indices==144,"first person must hide head, torso, and both legs");
    require(model.first_person_indices.size()==72 && model.primitives[0].first_person_count==0 &&
            model.primitives[1].first_person_count==72 && model.primitives[2].first_person_count==0,
            "first person must retain only both authored arms");
    // Independent asset oracle: the complete left/right arm triangles occupy
    // full-index offsets72..143 and vertices48..95 in octaryn_player_v1.gltf.
    require(std::equal(model.first_person_indices.begin(),model.first_person_indices.end(),
        model.indices.begin()+72),"first person must preserve exact authored arm triangles and order");
    std::array<size_t,2> limb_indices{};
    constexpr const char* limbs[]={"left_arm","right_arm"};
    for(auto index:model.first_person_indices) {
      const auto& vertex=model.vertices[index];
      for(size_t k=0;k<4;++k) if(vertex.weights[k]>0) {
        const auto& name=model.nodes[model.joints[vertex.joints[k]]].name;
        require(index>=48 && index<96,"non-arm authored triangle leaked into first person");
        require(name=="left_arm" || name=="right_arm","non-arm skin influence leaked into first person");
        for(size_t limb=0;limb<2;++limb) if(name==limbs[limb]) ++limb_indices[limb];
      }
    }
    for(auto count:limb_indices) require(count==36,"each authored arm must retain all 12 triangles");
    Palette bind;require(sample_player_skin(model,"",0,bind),"bind pose sample");
    float worst_bind=0;
    for(const auto& vertex:model.vertices) {
      fvec4 transformed(0.f),source(vertex.position[0],vertex.position[1],vertex.position[2],1.f);
      for(size_t k=0;k<4;++k) transformed+=bind[vertex.joints[k]]*source*vertex.weights[k];
      for(size_t k=0;k<4;++k) worst_bind=std::max(worst_bind,std::abs(transformed[k]-source[k]));
    }
    require(worst_bind<1e-5,"inverse-bind transforms do not preserve source geometry");
    for(const auto& clip:model.animations) {
      Palette a,b,c;
      require(sample_player_skin(model,clip.name,clip.duration*.37,a),"clip sample");
      require(sample_player_skin(model,clip.name,clip.duration*.63,b),"second clip sample");
      require(difference(a,b,model.joints.size())>1e-5,"authored animation not moving rig");
      for(size_t joint=0;joint<model.joints.size();++joint) for(size_t column=0;column<4;++column)
        for(size_t row=0;row<4;++row) require(std::isfinite(a[joint][column][row]),"nonfinite skin matrix");
      if(clip.looping) {
        require(sample_player_skin(model,clip.name,clip.duration*1.37,c),"wrapped clip sample");
        require(difference(a,c,model.joints.size())<1e-4,"loop does not preserve authored period");
      } else {
        require(sample_player_skin(model,clip.name,clip.duration,b) && sample_player_skin(model,clip.name,clip.duration+10,c),"one-shot end");
        require(difference(b,c,model.joints.size())<1e-6,"one-shot must clamp at final authored key");
      }
    }
    // Independent decoded asset oracle: body node translation at walk key 1/24 is 0.7536574006.
    Palette walk;require(sample_player_skin(model,"walk_loop",1.0/24.0,walk),"walk key sample");
    require(std::abs(walk[1][3][1]-.0036574006f)<1e-6,"authored translation key not sampled correctly");
    check_transitions(model);
    check_player_presentation();
    check_pose_history_metadata();
    check_stream_residency();
    check_column_blocks();
    check_action_feedback();
    Palette unchanged;require(!sample_player_skin(model,"missing_clip",0,unchanged),"unknown animation must fail");
    const std::filesystem::path scratch=argv[2];std::filesystem::create_directories(scratch);
    {std::ofstream broken(scratch/"invalid.gltf");broken<<"{\"asset\":{\"version\":\"2.0\"},\"meshes\":[]}";}
    require(!load_player_model(scratch/"invalid.gltf",model,error) && model.vertices.size()==144,
            "failed parse must retain previous coherent model");
    std::cout<<"player_model_probe=passed vertices=144 triangles=72 joints=8 clips=8 first_person_indices=72 first_person=arms_only"
             <<" bind_max_error="<<worst_bind<<" animation_periods=passed authored_translation=passed transitions=passed presentation=passed pose_metadata=passed stream_residency=passed action_feedback=passed malformed_rejected=passed\n";
    return 0;
  } catch(const std::exception& error) {std::cerr<<"player_model_probe=failed "<<error.what()<<'\n';return 1;}
}
