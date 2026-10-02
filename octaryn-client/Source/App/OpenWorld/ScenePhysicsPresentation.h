#pragma once
#include <memory>
namespace octaryn::client::rendering {struct WorldRenderer;}
namespace octaryn::client::app {
class LocalSession;
class ScenePhysicsPresentation {
public:
  ScenePhysicsPresentation();
  ~ScenePhysicsPresentation();
  bool update(rendering::WorldRenderer*,const LocalSession&);
private:
  struct State;
  std::unique_ptr<State> state_;
};
}
