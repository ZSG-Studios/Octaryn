#include "GameUiState.h"
#include "../DeclaredScreen/DeclaredScreen.h"
#include "Menu.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace octaryn::client::app {
namespace {
std::string folded(const std::string& value) {
  std::string result=value;
  std::transform(result.begin(),result.end(),result.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
  return result;
}
std::string last_opened(const std::string& value) {
  if(value.empty())return "Not played yet";
  std::tm utc{};
  std::istringstream input(value);input>>std::get_time(&utc,"%Y-%m-%dT%H:%M:%SZ");
  if(input.fail())return value;
#if defined(_WIN32)
  const auto instant=_mkgmtime(&utc);std::tm local{};localtime_s(&local,&instant);
#else
  const auto instant=timegm(&utc);std::tm local{};localtime_r(&instant,&local);
#endif
  std::ostringstream result;result<<"Last opened "<<std::put_time(&local,"%b %d, %Y");return result.str();
}
bool same(const WorldLibraryEntry& a,const WorldLibraryEntry& b) {
  return a.id==b.id && a.name==b.name && a.source==b.source && a.format==b.format &&
      a.save_label==b.save_label && a.last_played==b.last_played && a.available==b.available && a.active_save==b.active_save &&
      a.preparation_required==b.preparation_required && a.saves.size()==b.saves.size() && std::equal(a.saves.begin(),a.saves.end(),b.saves.begin(),[](const auto& x,const auto& y){
        return x.id==y.id && x.name==y.name && x.last_played==y.last_played;
      });
}
}
void GameUi::State::initialize_world_library(const std::filesystem::path& assets) {
  auto constructor=context->CreateDataModel("world_library");
  if(!constructor)throw std::runtime_error("Cannot create world library data model");
  auto row=constructor.RegisterStruct<WorldLibraryRow>();
  row.RegisterMember("id",&WorldLibraryRow::id);
  row.RegisterMember("name",&WorldLibraryRow::name);
  row.RegisterMember("format",&WorldLibraryRow::format);
  row.RegisterMember("save_label",&WorldLibraryRow::save_label);
  row.RegisterMember("last_played",&WorldLibraryRow::last_played);
  row.RegisterMember("available",&WorldLibraryRow::available);
  row.RegisterMember("selected",&WorldLibraryRow::selected);
  row.RegisterMember("preparation_required",&WorldLibraryRow::preparation_required);
  constructor.RegisterArray<std::vector<WorldLibraryRow>>();
  auto save=constructor.RegisterStruct<WorldLibrarySave>();
  save.RegisterMember("id",&WorldLibrarySave::id);
  save.RegisterMember("name",&WorldLibrarySave::name);
  constructor.RegisterArray<std::vector<WorldLibrarySave>>();
  constructor.Bind("saves",&library.saves);
  constructor.Bind("multiple_saves",&library.multiple_saves);
  constructor.Bind("has_saves",&library.has_saves);
  constructor.BindFunc("selected_save",[this](Rml::Variant& value){value=library.selected_save;},
      [this](const Rml::Variant& value){if(!library.busy){library.selected_save=value.Get<Rml::String>();rebuild_world_library();}});
  constructor.Bind("worlds",&library.rows);
  constructor.Bind("name",&library.name);
  constructor.Bind("source",&library.source);
  constructor.Bind("format",&library.format);
  constructor.Bind("save_label",&library.save_label);
  constructor.Bind("last_played",&library.last_played);
  constructor.Bind("open_label",&library.open_label);
  constructor.Bind("new_save_label",&library.new_save_label);
  constructor.Bind("prepare_label",&library.prepare_label);
  constructor.Bind("status",&library.status);
  constructor.Bind("busy",&library.busy);
  constructor.Bind("cancelable",&library.cancelable);
  constructor.Bind("failed",&library.failed);
  constructor.Bind("has_status",&library.has_status);
  constructor.Bind("single_action",&library.single_action);
  constructor.Bind("has_selection",&library.has_selection);
  constructor.Bind("can_open",&library.can_open);
  constructor.Bind("missing",&library.missing);
  constructor.Bind("preparation_required",&library.preparation_required);
  constructor.Bind("empty",&library.empty);
  constructor.Bind("no_matches",&library.no_matches);
  constructor.BindFunc("query",[this](Rml::Variant& value){value=library.query;},
      [this](const Rml::Variant& value){if(!library.busy){library.query=value.Get<Rml::String>();rebuild_world_library();}});
  for(const char* action:{"select","search","browse","find","open","new_save","locate","prepare","cancel","settings","multiplayer","exit"}) {
    constructor.BindEventCallback(action,[this,action](Rml::DataModelHandle,Rml::Event&,const Rml::VariantList& arguments){
      library_action(action,arguments);
    });
  }
  library.model=constructor.GetModelHandle();
  const auto package=assets.parent_path().parent_path().parent_path()/"Assets"/"Ui"/"WorldLibrary";
  library.document=ui::load_world_library_screen(*context,package);
  rebuild_world_library();
}
void GameUi::State::rebuild_world_library() {
  const auto query=folded(library.query);
  auto matches=[&](const auto& entry){return query.empty() || folded(entry.name+" "+entry.source).find(query)!=std::string::npos;};
  auto selected=std::find_if(library.entries.begin(),library.entries.end(),[&](const auto& entry){return entry.id==library.selected_id && matches(entry);});
  if(selected==library.entries.end())selected=std::find_if(library.entries.begin(),library.entries.end(),matches);
  library.has_selection=selected!=library.entries.end();
  if(library.has_selection) {
    if(library.selected_id!=selected->id)library.selected_save.clear();
    library.selected_id=selected->id;library.name=selected->name;library.source=selected->source;
    library.format=selected->format;library.save_label=selected->save_label;library.last_played=last_opened(selected->last_played);
    library.preparation_required=selected->preparation_required;
    library.can_open=selected->available && !library.busy;
    library.missing=!selected->available && !selected->preparation_required;
    library.saves=selected->saves;
    if(!library.saves.empty()) {
      auto save=std::find_if(library.saves.begin(),library.saves.end(),[&](const auto& entry){return entry.id==library.selected_save;});
      if(save==library.saves.end())save=std::find_if(library.saves.begin(),library.saves.end(),[&](const auto& entry){return entry.id==selected->active_save;});
      if(save==library.saves.end())save=library.saves.begin();
      library.selected_save=save->id;library.save_label=save->name;library.last_played=last_opened(save->last_played);
    } else library.selected_save.clear();
  } else {
    library.selected_id.clear();library.name.clear();library.source.clear();library.format.clear();
    library.save_label.clear();library.last_played.clear();library.can_open=false;library.missing=false;library.preparation_required=false;
    library.saves.clear();library.selected_save.clear();
  }
  library.multiple_saves=library.saves.size()>1;
  library.has_saves=!library.saves.empty();
  library.open_label=library.saves.empty()?"Open world":"Open save";
  if(library.preparation_required)library.open_label="Preparation needed";
  library.new_save_label="New save";library.prepare_label="Prepare world";
  if(library.busy) {
    if(library.active_action==WorldLibraryActionKind::Open)library.open_label="Opening...";
    if(library.active_action==WorldLibraryActionKind::NewSave)library.new_save_label="Creating save...";
    if(library.active_action==WorldLibraryActionKind::Prepare)library.prepare_label="Preparing...";
  }
  library.has_status=library.busy || !library.status.empty();
  library.single_action=library.preparation_required || library.missing || !library.has_saves;
  library.rows.clear();
  for(const auto& entry:library.entries) {
    if(!matches(entry))continue;
    library.rows.push_back({entry.id,entry.name,entry.source,entry.format,entry.save_label,entry.last_played,
        entry.available,entry.id==library.selected_id,entry.preparation_required});
  }
  library.empty=library.entries.empty();library.no_matches=!library.empty && library.rows.empty();
  library.model.DirtyAllVariables();
}
void GameUi::set_world_library(const std::vector<WorldLibraryEntry>& entries,const std::string& status,bool busy,bool cancelable,bool failed) {
  auto& s=*state_;
  const bool changed=entries.size()!=s.library.entries.size() || !std::equal(entries.begin(),entries.end(),s.library.entries.begin(),same);
  cancelable=busy && cancelable;
  if(!changed && s.library.status==status && s.library.busy==busy && s.library.cancelable==cancelable && s.library.failed==failed)return;
  s.library.entries=entries;s.library.status=status;s.library.busy=busy;s.library.cancelable=cancelable;s.library.failed=failed;
  if(!busy)s.library.active_action=WorldLibraryActionKind::None;
  s.rebuild_world_library();
}
bool GameUi::take_world_library_action(WorldLibraryAction& action) {
  auto& pending=state_->library.actions;
  if(pending.empty())return false;
  action=std::move(pending.front());pending.pop_front();return true;
}
void GameUi::show_world_library() {
  state_->controls.display_menu.active=1;
  state_->controls.display_menu.screen=DISPLAY_MENU_SCREEN_SINGLEPLAYER;
  hide_loading();
}
bool GameUi::world_library_visible() const {
  const auto& s=*state_;
  return !s.loading_visible && s.library.document && s.library.document->IsVisible();
}
void GameUi::State::sync_world_library() {
  if(!library.document)return;
  const auto& menu=controls.display_menu;
  const bool visible=menu.active && !controls.session_active && !loading_visible && !lighting.visible &&
      !inventory_open && !controls_open && !fsr_open &&
      (menu.screen==DISPLAY_MENU_SCREEN_SINGLEPLAYER || menu.screen==DISPLAY_MENU_SCREEN_MAIN);
  if(visible && !library.document->IsVisible())library.document->Show(Rml::ModalFlag::None,Rml::FocusFlag::None);
  else if(!visible && library.document->IsVisible())library.document->Hide();
}
void GameUi::State::library_action(const std::string& action,const Rml::VariantList& arguments) {
  if(!library.document || !library.document->IsVisible())return;
  if(action=="cancel" && library.cancelable && library.actions.empty()) {
    library.actions.push_back({WorldLibraryActionKind::Cancel,{},{}});
    library.cancelable=false;library.status="Stopping preparation...";rebuild_world_library();return;
  }
  if(library.busy)return;
  if(action=="select" && !arguments.empty()) {
    const auto id=arguments.front().Get<Rml::String>();
    if(std::none_of(library.rows.begin(),library.rows.end(),[&](const auto& row){return row.id==id;}))return;
    if(library.selected_id!=id)library.selected_save.clear();
    library.selected_id=id;rebuild_world_library();return;
  }
  if(action=="search"){rebuild_world_library();return;}
  if(action=="settings" || action=="multiplayer") {
    controls.display_menu.screen=action=="settings"?DISPLAY_MENU_SCREEN_SETTINGS:DISPLAY_MENU_SCREEN_MULTIPLAYER;
    controls.display_menu.row=action=="settings"?0:2;sync_menu();sync_capture();return;
  }
  if(action=="exit") {
    controls.display_menu.screen=DISPLAY_MENU_SCREEN_MAIN;
    pending|=runtime_controls_activate_menu_row(&controls,window,DISPLAY_MENU_EXIT_ROW,1);
    sync_menu();return;
  }
  WorldLibraryActionKind kind=WorldLibraryActionKind::None;
  if(action=="browse")kind=WorldLibraryActionKind::BrowseFiles;
  else if(action=="find")kind=WorldLibraryActionKind::BrowseFolder;
  else if(action=="open" && library.can_open)kind=WorldLibraryActionKind::Open;
  else if(action=="new_save" && library.can_open && library.has_saves)kind=WorldLibraryActionKind::NewSave;
  else if(action=="locate" && library.has_selection && library.missing)kind=WorldLibraryActionKind::Locate;
  else if(action=="prepare" && library.has_selection && library.preparation_required)kind=WorldLibraryActionKind::Prepare;
  if(kind==WorldLibraryActionKind::None || !library.actions.empty())return;
  library.actions.push_back({kind,library.selected_id,kind==WorldLibraryActionKind::Open?library.selected_save:std::string{}});
  library.active_action=kind;library.busy=true;library.cancelable=false;library.failed=false;
  if(kind==WorldLibraryActionKind::Open)library.status="Opening "+library.name+"...";
  else if(kind==WorldLibraryActionKind::NewSave)library.status="Creating a save for "+library.name+"...";
  else if(kind==WorldLibraryActionKind::Prepare)library.status="Preparing "+library.name+"...";
  else if(kind==WorldLibraryActionKind::BrowseFolder)library.status="Choose a folder.";
  else if(kind==WorldLibraryActionKind::Locate)library.status="Choose the source file for "+library.name+".";
  else library.status="Choose a world file.";
  rebuild_world_library();
}
bool GameUi::State::world_library_key(const SDL_Event& event) {
  if(!library.document || !library.document->IsVisible() || event.type!=SDL_EVENT_KEY_DOWN || event.key.repeat)return false;
  const auto* focus=context->GetFocusElement();
  const bool typing=focus && focus->GetTagName()=="input";
  if(focus && focus->GetTagName()=="select")return false;
  if((event.key.mod&SDL_KMOD_CTRL) && event.key.key==SDLK_F) {
    if(auto* input=library.document->GetElementById("library-search"))input->Focus();return true;
  }
  if(typing)return false;
  if(event.key.key==SDLK_RETURN && focus && focus->IsClassSet("library-world")) {
    if(library.can_open)audio_feedback.activate(context->GetFocusElement());
    library_action("open",{});return true;
  }
  if(event.key.key!=SDLK_UP && event.key.key!=SDLK_DOWN)return false;
  if(library.busy || library.rows.empty())return true;
  auto found=std::find_if(library.rows.begin(),library.rows.end(),[&](const auto& row){return row.selected;});
  std::size_t index=found==library.rows.end()?0:static_cast<std::size_t>(found-library.rows.begin());
  if(event.key.key==SDLK_UP)index=index==0?library.rows.size()-1:index-1;
  else index=(index+1)%library.rows.size();
  library.selected_save.clear();library.selected_id=library.rows[index].id;rebuild_world_library();context->Update();
  if(auto* list=library.document->GetElementById("library-list")) {
    if(auto* row=list->GetChild(static_cast<int>(index))) {
      if(auto* button=row->GetChild(0))button->Focus(true);
      row->ScrollIntoView();
    }
  }
  return true;
}
}
