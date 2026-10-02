#pragma once
#include <RmlUi/Core.h>
#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/ElementInstancer.h>
#include <RmlUi/Core/Geometry.h>
#include <RmlUi/Core/RenderManager.h>
#include <cmath>
#include <string>
#include <vector>
#include "../../Rendering/Ui/RmlTextureSource.h"

namespace octaryn::client::ui {
struct DeclaredMeshVertex {double x{},y{},u{},v{};};
struct DeclaredMesh {
  std::string element,texture;
  std::vector<DeclaredMeshVertex> vertices;
  std::vector<int> indices;
  bool wrap{};
};
inline bool declared_mesh_valid(const DeclaredMesh& mesh) {
  if(mesh.element.empty() || mesh.element.size()>128 || mesh.texture.empty() || mesh.texture.size()>4096 ||
      mesh.texture.find('\0')!=std::string::npos || mesh.vertices.size()<3 || mesh.vertices.size()>256 ||
      mesh.indices.size()<3 || mesh.indices.size()>768 || mesh.indices.size()%3)return false;
  for(const auto& vertex:mesh.vertices)
    if(!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.u) || !std::isfinite(vertex.v) ||
        std::abs(vertex.x)>16384 || std::abs(vertex.y)>16384 || std::abs(vertex.u)>64 || std::abs(vertex.v)>64)return false;
  for(const auto index:mesh.indices)if(index<0 || std::size_t(index)>=mesh.vertices.size())return false;
  return true;
}
class DeclaredMeshElement final : public Rml::Element {
  DeclaredMesh mesh_;
  Rml::Geometry geometry_;
  Rml::Texture texture_;
  Rml::ColourbPremultiplied tint_{};
  bool initialized_{};
public:
  explicit DeclaredMeshElement(const Rml::String& tag):Rml::Element(tag) {}
  void set_mesh(DeclaredMesh mesh) {mesh_=std::move(mesh);}
protected:
  void OnRender() override {
    auto* manager=GetRenderManager();if(!manager)return;
    const auto& computed=GetComputedValues();
    const auto color=GetParentNode()->GetComputedValues().image_color().ToPremultiplied(computed.opacity());
    if(!initialized_)texture_=manager->LoadTexture(mesh_.wrap?
        rendering::rml_wrapped_texture_source(mesh_.texture):mesh_.texture);
    if(!initialized_ || tint_!=color) {
      Rml::Mesh geometry;
      geometry.vertices.reserve(mesh_.vertices.size());
      for(const auto& vertex:mesh_.vertices)
        geometry.vertices.push_back({{float(vertex.x),float(vertex.y)},color,{float(vertex.u),float(vertex.v)}});
      geometry.indices.assign(mesh_.indices.begin(),mesh_.indices.end());
      geometry_=manager->MakeGeometry(std::move(geometry));tint_=color;initialized_=true;
    }
    geometry_.Render(GetAbsoluteOffset(Rml::BoxArea::Border),texture_);
  }
};
inline bool append_declared_meshes(Rml::ElementDocument& document,const std::vector<DeclaredMesh>& meshes) {
  static Rml::ElementInstancerGeneric<DeclaredMeshElement> instancer;
  for(const auto& mesh:meshes) {
    auto* target=document.GetElementById(mesh.element);
    if(!target || target->GetTagName()!="div" || target->GetNumChildren()!=0)return false;
    Rml::ElementPtr child(new DeclaredMeshElement("div"));
    child->SetInstancer(&instancer);
    static_cast<DeclaredMeshElement*>(child.get())->set_mesh(mesh);
    child->SetProperty("position","absolute");child->SetProperty("left","0px");child->SetProperty("top","0px");
    child->SetProperty("width","100%");child->SetProperty("height","100%");
    target->AppendChild(std::move(child));
  }
  return true;
}
}
