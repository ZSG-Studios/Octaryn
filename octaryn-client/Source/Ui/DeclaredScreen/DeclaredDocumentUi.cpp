#include "DeclaredDocumentUi.h"
#include "DeclaredMeshElement.h"
#include <glaze/glaze.hpp>
#include <algorithm>
#include <cstdio>
#include <vector>

namespace octaryn::client::ui {
struct DocumentEnvelope {
  unsigned version{};
  std::string screen_id,model,markup;
  bool modal{};
  unsigned image_count{};
  unsigned logical_width{},logical_height{};
  std::string fit_mode="contain";
  std::vector<std::string> fonts;
  std::vector<DocumentBinding> fields,actions;
  std::vector<BitmapFont> bitmap_fonts;
  std::vector<DeclaredMesh> meshes;
};
namespace {
bool identifier(const std::string& value) {
  return bitmap_identifier(value);
}
bool action_enabled(Rml::Element* element) {
  for(auto* node=element;node;node=node->GetParentNode())
    if(node->HasAttribute("disabled"))return false;
  return true;
}
struct PendingDocument {
  Rml::ElementDocument* value{};
  ~PendingDocument() {if(value)value->Close();}
};
std::string escaped(const std::string& input) {
  std::string result;
  for(char c:input) {
    if(c=='&')result+="&amp;";
    else if(c=='<')result+="&lt;";
    else if(c=='>')result+="&gt;";
    else if(c=='\"')result+="&quot;";
    else if(c=='\'')result+="&#39;";
    else result+=c;
  }
  return result;
}
}
DeclaredDocumentUi::~DeclaredDocumentUi() {clear();}
void DeclaredDocumentUi::clear() {
  for(auto& [id,screen]:screens_) if(screen.document) {
    screen.document->RemoveEventListener("click",this);screen.document->Close();
  }
  screens_.clear();pending_.clear();pointer_input_=false;
}
bool DeclaredDocumentUi::present(Rml::Context& context,const std::string& declaration,const std::string& values) {
  if(declaration.empty() || declaration.size()>1024*1024 || values.size()>16384)return false;
  DocumentEnvelope envelope;std::map<std::string,std::string> data;
  constexpr glz::opts options{.error_on_missing_keys=false};
  if(glz::read<options>(envelope,declaration) || glz::read_json(data,values) || (envelope.version!=1 && envelope.version!=2) ||
      envelope.model!="declared_document" || !identifier(envelope.screen_id) || envelope.markup.empty() ||
      envelope.markup.size()>512*1024 || envelope.fonts.size()>4 || envelope.bitmap_fonts.size()>4 || envelope.image_count>256 || envelope.fields.size()>32 || envelope.actions.size()>64)return false;
  if(envelope.meshes.size()>64 || (envelope.version==1 && !envelope.meshes.empty()))return false;
  std::vector<std::string> mesh_targets;std::size_t mesh_vertices{},mesh_indices{};
  for(const auto& mesh:envelope.meshes) {
    if(!identifier(mesh.element) || !declared_mesh_valid(mesh) ||
        std::find(mesh_targets.begin(),mesh_targets.end(),mesh.element)!=mesh_targets.end() ||
        std::any_of(envelope.fields.begin(),envelope.fields.end(),[&](const auto& field){return field.element==mesh.element;}) ||
        std::any_of(envelope.actions.begin(),envelope.actions.end(),[&](const auto& action){return action.element==mesh.element;}) ||
        (mesh_vertices+=mesh.vertices.size())>4096 || (mesh_indices+=mesh.indices.size())>12288)return false;
    mesh_targets.push_back(mesh.element);
  }
  if(!document_fit_mode_valid(envelope.fit_mode) || (envelope.fit_mode=="height" && envelope.logical_height==0) ||
      !document_canvas_valid(envelope.logical_width,envelope.logical_height) ||
      (envelope.version==1 && envelope.logical_width!=0))return false;
  auto found=screens_.find(envelope.screen_id);
  Screen initial;PendingDocument admitted;
  const bool creating=found==screens_.end();
  if(envelope.version==1 && (!envelope.bitmap_fonts.empty() || std::any_of(envelope.fields.begin(),envelope.fields.end(),
      [](const auto& field){return field.kind!="text";})))return false;
  if(found==screens_.end()) {
    if(screens_.size()>=8)return false;
    std::size_t total_bytes=declaration.size();for(const auto& [id,screen]:screens_)total_bytes+=screen.declaration.size();
    if(total_bytes>2*1024*1024)return false;
    auto& screen=initial;screen.declaration=declaration;screen.modal=envelope.modal;
    screen.logical_width=envelope.logical_width;screen.logical_height=envelope.logical_height;
    screen.fit_mode=envelope.fit_mode;
    for(const auto& font:envelope.bitmap_fonts) {
      if(!bitmap_font_valid(font))return false;
      if(!screen.bitmap_fonts.emplace(font.id,font).second)return false;
    }
    std::vector<std::string> targets;
    for(const auto& field:envelope.fields)
      if(!identifier(field.id) || !identifier(field.element) || (field.kind!="text" && field.kind!="bitmap_text") ||
          (field.kind=="bitmap_text" && !screen.bitmap_fonts.contains(field.font)) || (field.align!="left" && field.align!="center" && field.align!="right") ||
          !std::isfinite(field.scale) || field.scale<.1 || field.scale>8 ||
          !std::isfinite(field.wrap_width) || field.wrap_width<0 || field.wrap_width>16384 ||
          (field.kind!="bitmap_text" && (field.wrap_width!=0 || field.fit_text_height || field.wrap_to_element)) ||
          std::find(targets.begin(),targets.end(),field.element)!=targets.end() || !screen.fields.emplace(field.id,field).second)return false;
      else targets.push_back(field.element);
    std::vector<std::string> action_ids;
    for(const auto& action:envelope.actions)
      if(!identifier(action.id) || !action.id.starts_with(envelope.screen_id+".") || !identifier(action.element) ||
          (action.wrap_width!=0 || action.fit_text_height || action.wrap_to_element) ||
          std::find(action_ids.begin(),action_ids.end(),action.id)!=action_ids.end() ||
          !screen.actions.emplace(action.element,action.id).second)return false;
      else action_ids.push_back(action.id);
    for(const auto& [id,value]:data)if(!screen.fields.contains(id) || value.size()>4096 || value.find('\0')!=std::string::npos)return false;
    for(const auto& font:envelope.fonts)if(font.empty() || font.size()>4096 || !Rml::LoadFontFace(font))return false;
    screen.document=context.LoadDocumentFromMemory(envelope.markup);
    if(!screen.document)return false;
    admitted.value=screen.document;
    if(!append_declared_meshes(*screen.document,envelope.meshes))return false;
    const auto valid=[&] {
      for(const auto& [id,field]:screen.fields) {
        auto* element=screen.document->GetElementById(field.element);
        if(!element || screen.actions.contains(field.element))return false;
        for(int index=0;index<element->GetNumChildren();++index)
          if(element->GetChild(index)->GetTagName()!="#text")return false;
      }
      for(const auto& [element,id]:screen.actions) {
        auto* target=screen.document->GetElementById(element);if(!target)return false;
        target->SetClass("active_declared_action",false);
        const bool enabled=action_enabled(target);
        target->SetProperty("tab-index",enabled?"auto":"none");
        if(!enabled)target->SetProperty("focus","none");
        else for(const auto* direction:{"nav-up","nav-down","nav-left","nav-right"})
          if(!target->GetLocalProperty(direction))target->SetProperty(direction,"auto");
      }
      return true;
    };
    if(!valid())return false;
  } else if(found->second.declaration!=declaration)return false;
  auto& screen=creating?initial:found->second;
  for(const auto& [id,value]:data)if(!screen.fields.contains(id) || value.size()>4096 || value.find('\0')!=std::string::npos)return false;
  std::map<std::string,std::string> pending;
  std::map<std::string,double> widths;
  std::map<std::string,BitmapTint> tints;
  std::map<std::string,double> heights;
  for(const auto& [id,field]:screen.fields) {
    const auto value=data.find(id);
    const std::string text=value==data.end()?"":value->second;
    auto* element=screen.document->GetElementById(field.element);
    if(!element)return false;
    const double width=element->GetClientWidth();
    const auto color=element->GetComputedValues().image_color();
    const BitmapTint tint{color.red,color.green,color.blue,color.alpha};
    if(screen.values.contains(id) && screen.values.at(id)==text && (field.kind!="bitmap_text" ||
        (screen.widths.contains(id) && screen.tints.contains(id) &&
          bitmap_cached(screen.values.at(id),text,screen.widths.at(id),width,screen.tints.at(id),tint))))continue;
    if(field.kind=="bitmap_text") {
      std::string markup;
      double measured{};
      if(!bitmap_markup(screen.bitmap_fonts.at(field.font),field,text,width,markup,tint,&measured))return false;
      if(field.fit_text_height)heights.emplace(id,measured);
      pending.emplace(id,std::move(markup));
      widths.emplace(id,width);
      tints.emplace(id,tint);
    } else pending.emplace(id,escaped(text));
  }
  if(creating) {
    found=screens_.emplace(envelope.screen_id,std::move(initial)).first;admitted.value=nullptr;
    found->second.document->AddEventListener("click",this);
    std::printf("ui_host declared_document=1 screen=%s images=%u modal=%u\n",envelope.screen_id.c_str(),envelope.image_count,unsigned(envelope.modal));
  }
  auto& published=found->second;
  for(const auto& [id,markup]:pending) {
    published.document->GetElementById(published.fields.at(id).element)->SetInnerRML(markup);
    if(heights.contains(id))published.document->GetElementById(published.fields.at(id).element)->SetProperty("height",bitmap_number(heights.at(id))+"px");
    const auto value=data.find(id);published.values[id]=value==data.end()?"":value->second;
    if(widths.contains(id))published.widths[id]=widths.at(id);
    if(tints.contains(id))published.tints[id]=tints.at(id);
  }
  if(!published.document->IsVisible())published.document->Show(published.modal?Rml::ModalFlag::Modal:Rml::ModalFlag::None,
      published.modal?Rml::FocusFlag::Auto:Rml::FocusFlag::None);
  fit(context);
  return true;
}
void DeclaredDocumentUi::fit(Rml::Context& context) {
  const auto dimensions=context.GetDimensions();
  for(auto& [id,screen]:screens_) {
    if(!screen.logical_width || (screen.viewport_width==dimensions.x && screen.viewport_height==dimensions.y))continue;
    DocumentFit fitted;
    if(!document_fit(screen.logical_width,screen.logical_height,dimensions.x,dimensions.y,fitted,screen.fit_mode))continue;
    auto* document=screen.document;
    document->SetProperty("position","absolute");
    document->SetProperty("width",bitmap_number(fitted.width)+"px");
    document->SetProperty("height",bitmap_number(fitted.height)+"px");
    document->SetProperty("transform-origin","0px 0px");
    document->SetProperty("transform","scale("+bitmap_number(fitted.scale)+")");
    document->SetProperty("left",bitmap_number(fitted.left)+"px");document->SetProperty("top",bitmap_number(fitted.top)+"px");
    screen.viewport_width=dimensions.x;screen.viewport_height=dimensions.y;
  }
}
bool DeclaredDocumentUi::reflow() {
  struct Update {Screen* screen;Rml::Element* element;std::string id,markup;double width,height;BitmapTint tint;bool fit_height;};
  std::vector<Update> pending;
  for(auto& [screen_id,screen]:screens_) {
    if(!screen.document->IsVisible())continue;
    for(const auto& [id,field]:screen.fields) {
      if(field.kind!="bitmap_text" || !screen.values.contains(id))continue;
      auto* element=screen.document->GetElementById(field.element);if(!element)return false;
      const double width=element->GetClientWidth();const auto color=element->GetComputedValues().image_color();
      const BitmapTint tint{color.red,color.green,color.blue,color.alpha};const auto& text=screen.values.at(id);
      if(screen.widths.contains(id) && screen.tints.contains(id) &&
          bitmap_cached(text,text,screen.widths.at(id),width,screen.tints.at(id),tint))continue;
      std::string markup;
      double measured{};
      if(!bitmap_markup(screen.bitmap_fonts.at(field.font),field,text,width,markup,tint,&measured))return false;
      pending.push_back({&screen,element,id,std::move(markup),width,measured,tint,field.fit_text_height});
    }
  }
  for(auto& update:pending) {
    update.element->SetInnerRML(update.markup);
    if(update.fit_height)update.element->SetProperty("height",bitmap_number(update.height)+"px");
    update.screen->widths[update.id]=update.width;update.screen->tints[update.id]=update.tint;
  }
  return !pending.empty();
}
bool DeclaredDocumentUi::hide(const std::string& id) {
  const auto found=screens_.find(id);if(found==screens_.end())return false;
  found->second.document->RemoveEventListener("click",this);
  found->second.document->Close();screens_.erase(found);
  std::erase_if(pending_,[&](const auto& action){return action.first==id;});
  return true;
}
bool DeclaredDocumentUi::modal() const {
  for(const auto& [id,screen]:screens_)if(screen.modal && screen.document->IsVisible())return true;
  return false;
}
bool DeclaredDocumentUi::poll(std::string& action) {
  if(pending_.empty())return false;action=std::move(pending_.front().second);pending_.pop_front();return true;
}
bool DeclaredDocumentUi::dispatch_action(const std::string& action) {
  for(auto& [id,screen]:screens_) {
    if(!screen.document || !screen.document->IsVisible())continue;
    for(const auto& [element,declared]:screen.actions) {
      if(declared!=action)continue;
      auto* target=screen.document->GetElementById(element);
      if(!target || !target->IsVisible(true) || !action_enabled(target))return false;
      Rml::Dictionary parameters;target->DispatchEvent("click",parameters);return true;
    }
  }
  return false;
}
void DeclaredDocumentUi::pointer_move(Rml::Context& context) {
  pointer_input_=true;
  auto* hovered=context.GetHoverElement();
  auto* focused=context.GetFocusElement();
  for(auto& [id,screen]:screens_) {
    if(!screen.document->IsVisible())continue;
    if(hovered && hovered->GetOwnerDocument()==screen.document) {
      for(auto* target=hovered;target;target=target->GetParentNode())
        if(screen.actions.contains(target->GetId()) && action_enabled(target)) {
          if(focused!=target)target->Focus(false);
          return;
        }
    }
    if(focused && focused->GetOwnerDocument()==screen.document)
      for(auto* target=focused;target;target=target->GetParentNode())
        if(screen.actions.contains(target->GetId())) {
          screen.document->Focus(false);break;
        }
  }
}
void DeclaredDocumentUi::pointer_position(Rml::Context& context,int x,int y,int modifiers) {
  context.ProcessMouseMove(x,y,modifiers);pointer_move(context);
}
void DeclaredDocumentUi::keyboard_input(Rml::Context& context) {
  pointer_input_=false;context.ProcessMouseLeave();
}
bool DeclaredDocumentUi::sync_selection(Rml::Context& context) {
  if(pointer_input_)pointer_move(context);
  auto* focused=context.GetFocusElement();bool changed=false;
  for(auto& [id,screen]:screens_) {
    Rml::Element* selected=nullptr;
    if(screen.document->IsVisible() && focused && focused->GetOwnerDocument()==screen.document)
      for(auto* target=focused;target;target=target->GetParentNode())
        if(screen.actions.contains(target->GetId()) && action_enabled(target)) {selected=target;break;}
    const std::string next=selected?selected->GetId():"";
    if(screen.active_action==next)continue;
    if(!screen.active_action.empty())
      if(auto* previous=screen.document->GetElementById(screen.active_action))previous->SetClass("active_declared_action",false);
    if(selected)selected->SetClass("active_declared_action",true);
    screen.active_action=next;changed=true;
  }
  return changed;
}
void DeclaredDocumentUi::ProcessEvent(Rml::Event& event) {
  auto* target=event.GetTargetElement();
  for(const auto& [id,screen]:screens_)if(target && target->GetOwnerDocument()==screen.document) {
    if(!screen.document->IsVisible())return;
    while(target) {
      const auto found=screen.actions.find(target->GetId());
      if(found!=screen.actions.end()) {
        if(action_enabled(target)) {
          if(auto* context=target->GetContext();context && context->GetFocusElement()!=target)target->Focus(false);
          if(pending_.size()<64)pending_.emplace_back(id,found->second);
        }
        event.StopPropagation();return;
      }
      target=target->GetParentNode();
    }
  }
}
}
