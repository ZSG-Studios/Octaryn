#include "WorldLibraryController.h"
#include "GameUi.h"
#include "octaryn_native_schedule_runtime.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <exception>
#include <mutex>
#include <cstdio>
#include <stdexcept>
#include <algorithm>

namespace octaryn::client::app {
namespace {
enum class Work { Refresh,Files,Folder,Prepare,Open,NewSave };
bool selected_work(Work work) {return work==Work::Prepare || work==Work::Open || work==Work::NewSave;}
struct DialogResult {
  std::mutex mutex;
  bool ready{},pending{},alive{true};
  Work work{};
  std::string world_id,error;
  std::vector<std::filesystem::path> paths;
};
void SDLCALL chosen(void* user,const char* const* files,int) {
  std::unique_ptr<std::shared_ptr<DialogResult>> owner(static_cast<std::shared_ptr<DialogResult>*>(user));
  auto& inbox=**owner;
  std::lock_guard lock(inbox.mutex);
  if(!inbox.alive)return;
  if(!files)inbox.error=SDL_GetError();
  else for(auto current=files;*current;++current)inbox.paths.emplace_back(reinterpret_cast<const char8_t*>(*current));
  inbox.ready=true;inbox.pending=false;
}
constexpr SDL_DialogFileFilter filters[]={{"glTF worlds","glb;gltf"}};
}
struct WorldLibraryController::State {
  WorldLibrary library;
  std::vector<WorldLibraryEntry> entries;
  std::vector<WorldLibraryEntry> prepared_entries;
  std::shared_ptr<DialogResult> dialog=std::make_shared<DialogResult>();
  void* scheduler{};
  void* task{};
  std::atomic_bool cancel{};
  Work work{Work::Refresh};
  std::vector<std::filesystem::path> paths;
  std::string locate_id,save_id,status{"Finding your worlds..."},error;
  std::string selected_name,selected_detail;
  std::filesystem::path prepared_save,opened_save;
  std::mutex progress_mutex;
  std::string progress_text;
  bool changed{true},last_failed{},queued{};
  State(const std::filesystem::path& root,const std::filesystem::path& bundle):library(root,bundle) {
    scheduler=octaryn_native_schedule_runtime_create(SDL_GetNumLogicalCPUCores(),2);
    if(!scheduler)throw std::runtime_error("World library scheduler unavailable");
    start(Work::Refresh,{});
  }
  ~State() {
    {std::lock_guard lock(dialog->mutex);dialog->alive=false;}
    cancel=true;
    if(task)octaryn_native_schedule_runtime_task_destroy(task);
    if(scheduler)octaryn_native_schedule_runtime_destroy(scheduler);
  }
  static int execute(void* context) noexcept {
    auto& s=*static_cast<State*>(context);bool ok=false;
    const auto progress=[&](const std::string& text) {
      std::lock_guard lock(s.progress_mutex);s.progress_text=text;
    };
    try {
      if(s.cancel)s.error="World loading canceled.";
      else if(s.work==Work::Refresh)ok=s.library.refresh(s.error);
      else if(s.work==Work::Prepare)ok=s.library.prepare(s.locate_id,s.error,&s.cancel,progress);
      else if(s.work==Work::NewSave)ok=s.library.new_save(s.locate_id,s.prepared_save,s.error,&s.cancel,progress);
      else if(s.work==Work::Open)ok=s.save_id.empty()
          ?s.library.open_world(s.locate_id,s.prepared_save,s.error,&s.cancel,progress)
          :s.library.select_save(s.locate_id,s.save_id,s.prepared_save,s.error,&s.cancel,progress);
      else if(s.work==Work::Folder)ok=s.library.scan_folder(s.paths.front(),s.error,&s.cancel);
      else {
        ok=true;
        for(const auto& path:s.paths) {
          if(s.cancel) {ok=false;break;}
          if(!(s.locate_id.empty()?s.library.add_source(path,s.error,&s.cancel):s.library.locate(s.locate_id,path,s.error,&s.cancel))) {ok=false;break;}
        }
      }
    } catch(const std::exception& failure) {ok=false;s.error=failure.what();}
    s.prepared_entries=s.library.entries();
    return ok?0:-1;
  }
  void start(Work operation,std::vector<std::filesystem::path> files,std::string id={},std::string save={}) {
    if(task || queued)return;
    work=operation;paths=std::move(files);locate_id=std::move(id);save_id=std::move(save);
    error.clear();cancel=false;last_failed=false;prepared_save.clear();opened_save.clear();
    selected_name.clear();selected_detail.clear();
    if(selected_work(operation))for(const auto& entry:entries)if(entry.id==locate_id) {
      selected_name=entry.name;selected_detail=operation==Work::Prepare?"Preparing the starting area"
          :operation==Work::NewSave?"New save":entry.save_label;
      const auto selected=save_id.empty()?entry.active_save:save_id;
      if(operation==Work::Open)for(const auto& item:entry.saves)if(item.id==selected)selected_detail=item.name;
      break;
    }
    status=operation==Work::Refresh?"Finding your worlds...":operation==Work::Folder?"Finding worlds in this folder...":"Adding world files...";
    if(operation==Work::Prepare)status="Preparing world...";
    else if(operation==Work::Open || operation==Work::NewSave)status="Checking the selected world...";
    {std::lock_guard lock(progress_mutex);progress_text.clear();}
    queued=true;changed=true;
  }
  void dispatch() {
    if(!queued || task)return;
    queued=false;
    octaryn_native_schedule_runtime_job job{};
    job.job_id="world_library_work";job.execute=execute;job.context=this;
    task=octaryn_native_schedule_runtime_submit_worker(scheduler,&job,1);
    if(!task){last_failed=true;status="Could not start the world operation.";}
    changed=true;
  }

};
WorldLibraryController::WorldLibraryController(const std::filesystem::path& root,const std::filesystem::path& bundle)
    :state_(std::make_unique<State>(root,bundle)) {}
WorldLibraryController::~WorldLibraryController()=default;
bool WorldLibraryController::busy() const {
  std::lock_guard lock(state_->dialog->mutex);
  return state_->task || state_->queued || state_->dialog->pending;
}
bool WorldLibraryController::loading() const {
  return (state_->queued || state_->task) && selected_work(state_->work);
}
bool WorldLibraryController::failed() const {return state_->last_failed;}
const std::string& WorldLibraryController::status() const {return state_->status;}
const std::string& WorldLibraryController::selected_name() const {return state_->selected_name;}
const std::string& WorldLibraryController::selected_detail() const {return state_->selected_detail;}
bool WorldLibraryController::take_opened(std::filesystem::path& path,std::string& world_name,std::string& save_name) {
  if(state_->opened_save.empty())return false;
  path=std::move(state_->opened_save);state_->opened_save.clear();
  world_name=state_->selected_name;save_name=state_->selected_detail;return true;
}
void WorldLibraryController::message(const std::string& text,bool failed) {
  state_->status=text;state_->last_failed=failed && !text.empty();state_->changed=true;
}
void WorldLibraryController::update(GameUi& ui) {
  auto& s=*state_;
  s.dispatch();
  if(s.task && !s.cancel) {
    std::lock_guard lock(s.progress_mutex);
    if(!s.progress_text.empty() && s.status!=s.progress_text) {s.status=s.progress_text;s.changed=true;}
  }
  if(s.task && octaryn_native_schedule_runtime_task_ready(s.task)) {
    octaryn_native_schedule_runtime_report report{};
    const int result=octaryn_native_schedule_runtime_task_result(s.task,&report);
    octaryn_native_schedule_runtime_task_destroy(s.task);s.task=nullptr;
    s.entries=std::move(s.prepared_entries);
    s.last_failed=result!=0 && !s.cancel;
    s.status=s.cancel?"World operation canceled.":result?s.error:s.work==Work::Prepare?"Starting area prepared.":"";
    if(result==0 && !s.cancel && !s.prepared_save.empty()) {
      s.opened_save=std::move(s.prepared_save);
      for(const auto& entry:s.entries)if(entry.id==s.locate_id) {
        s.selected_name=entry.name;
        for(const auto& save:entry.saves)if(save.id==s.opened_save.filename().string())s.selected_detail=save.name;
      }
    }
    s.prepared_save.clear();
    s.changed=true;
    std::printf("world_library_ready entries=%zu status=%s\n",s.entries.size(),s.cancel?"cancelled":result?"failed":"ready");
  }
  std::vector<std::filesystem::path> paths;std::string id;Work work{};
  {std::lock_guard lock(s.dialog->mutex);
    if(s.dialog->ready) {
      s.dialog->ready=false;paths=std::move(s.dialog->paths);id=s.dialog->world_id;work=s.dialog->work;
      s.status=s.dialog->error;s.last_failed=!s.status.empty();s.dialog->error.clear();s.changed=true;
    }
  }
  if(!paths.empty())s.start(work,std::move(paths),std::move(id));
  if(s.changed) {ui.set_world_library(s.entries,s.status,busy(),(s.queued || s.task) && selected_work(s.work) && !s.cancel,s.last_failed);s.changed=false;}
}
void WorldLibraryController::add_files(const std::vector<std::filesystem::path>& paths) {
  if(!busy() && !paths.empty())state_->start(Work::Files,paths);
}
void WorldLibraryController::find_folder(const std::filesystem::path& folder) {
  if(!busy())state_->start(Work::Folder,{folder});
}
bool WorldLibraryController::action(const WorldLibraryAction& action,SDL_Window* window,std::filesystem::path& requested) {
  if(action.kind==WorldLibraryActionKind::Cancel) {
    if(state_->queued || state_->task) {
      state_->cancel=true;state_->last_failed=false;state_->changed=true;
      state_->status=state_->queued?"World operation canceled.":"Stopping world operation...";
      state_->queued=false;
    }
    return false;
  }
  if(busy())return false;
  auto& s=*state_;
  if(action.kind==WorldLibraryActionKind::Prepare || action.kind==WorldLibraryActionKind::Open || action.kind==WorldLibraryActionKind::NewSave) {
    requested.clear();
    const auto found=std::find_if(s.entries.begin(),s.entries.end(),[&](const auto& entry){return entry.id==action.world_id;});
    if(found==s.entries.end()) {message("That world is unavailable.");return false;}
    const Work work=action.kind==WorldLibraryActionKind::Prepare?Work::Prepare:
        action.kind==WorldLibraryActionKind::NewSave?Work::NewSave:Work::Open;
    s.start(work,{},action.world_id,action.save_id);return true;
  }
  if(action.kind==WorldLibraryActionKind::BrowseFiles || action.kind==WorldLibraryActionKind::BrowseFolder ||
      action.kind==WorldLibraryActionKind::Locate) {
    {std::lock_guard lock(s.dialog->mutex);
      s.dialog->pending=true;s.dialog->work=action.kind==WorldLibraryActionKind::BrowseFolder?Work::Folder:Work::Files;
      s.dialog->world_id=action.kind==WorldLibraryActionKind::Locate?action.world_id:std::string{};
    }
    s.changed=true;s.last_failed=false;s.status="Choose a world file or folder.";
    auto* inbox=new std::shared_ptr<DialogResult>(s.dialog);
    if(action.kind==WorldLibraryActionKind::BrowseFolder)SDL_ShowOpenFolderDialog(chosen,inbox,window,nullptr,false);
    else SDL_ShowOpenFileDialog(chosen,inbox,window,filters,1,nullptr,action.kind!=WorldLibraryActionKind::Locate);
  }
  return false;
}
bool WorldLibraryController::open_index(unsigned index,std::filesystem::path& requested) {
  if(busy() || index>=state_->entries.size()){message("That world is unavailable.");return false;}
  return action({WorldLibraryActionKind::Open,state_->entries[index].id},nullptr,requested);
}
}
