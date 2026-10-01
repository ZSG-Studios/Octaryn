#pragma once
#include <string>
namespace octaryn::client::rendering {
struct MapRenderer;
bool upload_scene_forward(MapRenderer&,std::string& error);
}
