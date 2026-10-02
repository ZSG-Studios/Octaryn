#include "ModulePanelUi.h"
#include <glaze/glaze.hpp>
#include <algorithm>
#include <cctype>
#include <vector>

namespace octaryn::client::ui {
struct PanelEntry {std::string id,label;};
struct PanelDeclaration {unsigned version{};std::string screen_id,model,title;std::vector<PanelEntry> fields,actions;};
namespace {
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
bool identifier(const std::string& value) {
  return !value.empty() && value.size()<=128 && std::all_of(value.begin(),value.end(),[](unsigned char c){
    return std::isalnum(c) || c=='_' || c=='-' || c=='.';
  });
}
}
ModulePanelUi::~ModulePanelUi() {
  if(document_) {document_->RemoveEventListener("click",this);document_->Close();}
}
bool ModulePanelUi::present(Rml::Context& context,const std::string& declaration,const std::string& values) {
  if(declaration.empty() || declaration.size()>8192 || values.size()>16384)return false;
  PanelDeclaration panel;
  std::map<std::string,std::string> data;
  constexpr glz::opts options{.error_on_missing_keys=true};
  if(glz::read<options>(panel,declaration) || glz::read_json(data,values) || panel.version!=1 ||
      panel.model!="module_panel" || !identifier(panel.screen_id) || panel.title.empty() || panel.title.size()>128 ||
      panel.fields.empty() || panel.fields.size()>32 || panel.actions.size()>16)return false;
  std::set<std::string> fields,actions;
  for(const auto& field:panel.fields)
    if(!identifier(field.id) || field.label.size()>128 || !fields.insert(field.id).second)return false;
  for(const auto& action:panel.actions)
    if(!identifier(action.id) || !action.id.starts_with(panel.screen_id+".") || action.label.size()>128 || !actions.insert(action.id).second)return false;
  for(const auto& [id,value]:data)
    if(!fields.contains(id) || value.size()>4096 || value.find('\0')!=std::string::npos)return false;
  if(document_ && (id_!=panel.screen_id || declaration_!=declaration))return false;
  if(!document_) {
    document_=context.CreateDocument();if(!document_)return false;
    id_=panel.screen_id;declaration_=declaration;fields_=fields;actions_=actions;
    document_->SetProperty("position","absolute");document_->SetProperty("left","6%");document_->SetProperty("top","5%");
    document_->SetProperty("width","88%");document_->SetProperty("height","88%");document_->SetProperty("overflow","auto");
    document_->SetProperty("background-color","#151d23");document_->SetProperty("color","#e6e9dd");
    document_->SetProperty("font-family","LatoLatin");document_->SetProperty("font-size","18px");
    document_->SetProperty("padding","20px");document_->SetProperty("z-index","50");
    // A host-created document has no game stylesheet supplying tag defaults.
    // Declare block flow explicitly so labels/values do not concatenate inline.
    std::string markup="<h1 style='display:block;font-size:28px;font-weight:bold;line-height:34px;margin-bottom:16px'>"+escaped(panel.title)+"</h1>";
    for(const auto& field:panel.fields)markup+="<div style='display:block;margin-bottom:12px'><div style='display:block;font-size:13px;color:#97b9b4;line-height:18px'>"+escaped(field.label)+
        "</div><div id='module-field-"+field.id+"' style='display:block;white-space:pre-wrap;line-height:22px;margin-top:3px'></div></div>";
    markup+="<div style='display:block;margin-top:16px'>";
    for(const auto& action:panel.actions)markup+="<button module-action='"+action.id+
        "' style='display:inline-block;background-color:#34494d;color:#e6e9dd;padding:8px;margin-right:8px;margin-bottom:8px'>"+escaped(action.label)+"</button>";
    markup+="</div>";
    document_->SetInnerRML(markup);document_->AddEventListener("click",this);document_->Show();
  }
  for(const auto& id:fields_) {
    auto* element=document_->GetElementById("module-field-"+id);if(!element)return false;
    const auto found=data.find(id);element->SetInnerRML(found==data.end()?"":escaped(found->second));
  }
  return true;
}
bool ModulePanelUi::hide(const std::string& id) {
  if(!document_ || id!=id_)return false;
  document_->Hide();return true;
}
bool ModulePanelUi::visible() const {return document_ && document_->IsVisible();}
bool ModulePanelUi::toggle() {if(!document_)return false;if(visible())document_->Hide();else {document_->Show();focus();}return true;}
void ModulePanelUi::focus() {if(document_)document_->Focus();}
bool ModulePanelUi::poll(std::string& action) {
  if(pending_.empty())return false;action=std::move(pending_.front());pending_.pop_front();return true;
}
void ModulePanelUi::ProcessEvent(Rml::Event& event) {
  auto* target=event.GetTargetElement();
  while(target && !target->HasAttribute("module-action"))target=target->GetParentNode();
  if(!target)return;
  const auto action=target->GetAttribute<Rml::String>("module-action","");
  if(actions_.contains(action) && pending_.size()<64)pending_.push_back(action);
  event.StopPropagation();
}
}
