#include "CharacterBodyPressure.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace octaryn::character_motion {
namespace {
// Generic character pressure policy; this is not an authored game mass override.
constexpr float character_mass=80;
constexpr float maximum_pressure_force=character_mass*9.81f;
struct Contact {b3BodyId body{};b3Vec3 normal{};b3Pos point{};float closing{};};
struct Query {b3Pos origin{};b3Vec3 velocity{};std::array<Contact,32> contacts{};unsigned count{};};
bool collect(b3ShapeId shape,const b3PlaneResult* planes,int count,void* context) {
  auto& query=*static_cast<Query*>(context);const auto body=b3Shape_GetBody(shape);
  if(b3Body_GetType(body)!=b3_dynamicBody || !b3Body_IsEnabled(body))return true;
  for(int i=0;i<count;++i) {
    const auto normal=planes[i].plane.normal;
    if(std::abs(normal.y)>.6f)continue;
    const auto point=query.origin+planes[i].point;
    const auto velocity=b3Body_GetWorldPointVelocity(body,point);
    const float closing=-b3Dot(b3Sub(query.velocity,velocity),normal);
    if(closing<=0)continue;
    unsigned slot=0;for(;slot<query.count;++slot)if(B3_ID_EQUALS(query.contacts[slot].body,body))break;
    if(slot==query.count) {if(query.count==query.contacts.size())continue;++query.count;}
    if(closing>query.contacts[slot].closing)query.contacts[slot]={body,normal,point,closing};
  }
  return true;
}
}
void apply_character_body_pressure(b3WorldId world,b3Pos origin,const b3Capsule& capsule,b3Vec3 velocity,float seconds) {
  if(seconds<=0 || b3LengthSquared(velocity)<.0001f)return;
  Query query;query.origin=origin;query.velocity=velocity;
  auto contact_capsule=capsule;contact_capsule.radius+=.02f;
  b3World_CollideMover(world,origin,&contact_capsule,b3DefaultQueryFilter(),collect,&query);
  float available_impulse=maximum_pressure_force*seconds;
  for(unsigned i=0;i<query.count && available_impulse>0;++i) {
    const auto& contact=query.contacts[i];const float mass=b3Body_GetMass(contact.body);
    if(!(mass>0) || !std::isfinite(mass))continue;
    const float reduced_mass=character_mass*mass/(character_mass+mass);
    const float impulse=std::min(reduced_mass*contact.closing,available_impulse);
    b3Body_ApplyLinearImpulse(contact.body,b3MulSV(-impulse,contact.normal),contact.point,true);
    available_impulse-=impulse;
  }
}
}
