#include "SceneLoading.h"
#include "ScenePreparation.h"
#include "octaryn_native_schedule_runtime.h"
#include "FilePath.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>
namespace octaryn::scene_loading {
namespace {
std::atomic<std::uint64_t> reserved{},sequence{1};
bool reserve(std::uint64_t bytes) {
    auto current=reserved.load();
    while(current<=memory_limit && bytes<=memory_limit-current) {
        if(reserved.compare_exchange_weak(current,current+bytes))return true;
    }
    return false;
}
struct Owner;
struct Reservation {
    std::uint64_t bytes{working_bytes};
    ~Reservation() {reserved.fetch_sub(bytes);}
};
struct Job {
    Owner* owner{};octaryn_scene_loading_ticket ticket{};std::filesystem::path input;
    std::atomic_bool canceled{};octaryn_scene_loading_progress progress{};std::string error;
    std::shared_ptr<const Snapshot> result;bool released{};void* task{};
    std::shared_ptr<Reservation> reservation;
};
struct Owner {
    std::filesystem::path root;std::string module;void* scheduler{};
    std::mutex mutex;std::array<std::shared_ptr<Job>,8> tickets;std::shared_ptr<Job> active;bool closed{};
};
std::shared_ptr<Job> find(Owner& owner,const octaryn_scene_loading_ticket& ticket) {
    if(!ticket.id || ticket.id>owner.tickets.size())return {};
    const auto job=owner.tickets[ticket.id-1];
    return job && job->ticket.generation==ticket.generation?job:nullptr;
}
int execute(void* context) {
    auto& job=*static_cast<Job*>(context);auto& owner=*job.owner;std::unique_ptr<Snapshot> result;std::string error;
    try {
        Preparation work{owner.root,job.input,&job.canceled,[&](std::uint64_t completed,std::uint64_t total) {
            std::lock_guard lock(owner.mutex);job.progress.completed=completed;job.progress.total=total;
        }};
        result=prepare(work);
    }catch(const std::exception& failure) {error=failure.what();}
    std::lock_guard lock(owner.mutex);
    if(owner.closed || job.released || job.canceled.load()) {
        result.reset();job.progress.preparation=OCTARYN_SCENE_LOADING_CANCELED;job.reservation.reset();
    }else if(!result) {
        job.progress.preparation=OCTARYN_SCENE_LOADING_FAILED;job.error=error.substr(0,1024);job.reservation.reset();
    }else {
        const auto bytes=result->retained_bytes;
        try {
            const auto lease=job.reservation;
            job.result=std::shared_ptr<const Snapshot>(result.release(),[lease](const Snapshot* value){delete value;});
            reserved.fetch_sub(working_bytes-bytes);lease->bytes=bytes;job.reservation.reset();
            job.progress.retained_bytes=bytes;job.progress.preparation=OCTARYN_SCENE_LOADING_CPU_PREPARED;
        }catch(...) {
            job.progress.preparation=OCTARYN_SCENE_LOADING_FAILED;job.error="scene snapshot allocation failed";job.reservation.reset();
        }
    }
    return 0;
}
void pump(Owner& owner) {
    if(owner.active && octaryn_native_schedule_runtime_task_ready(owner.active->task)==1) {
        octaryn_native_schedule_runtime_task_destroy(owner.active->task);owner.active->task=nullptr;owner.active.reset();
    }
    if(owner.closed || owner.active)return;
    for(const auto& job:owner.tickets) {
        if(!job || job->progress.preparation!=OCTARYN_SCENE_LOADING_QUEUED)continue;
        if(!reserve(working_bytes))return;
        try {job->reservation=std::make_shared<Reservation>();}
        catch(...) {reserved.fetch_sub(working_bytes);job->progress.preparation=OCTARYN_SCENE_LOADING_FAILED;job->error="scene working lease allocation failed";return;}
        octaryn_native_schedule_runtime_job work{"scene.metadata.prepare",nullptr,0,nullptr,0,0,execute,job.get()};
        job->progress.preparation=OCTARYN_SCENE_LOADING_RUNNING;owner.active=job;
        job->task=octaryn_native_schedule_runtime_submit_worker(owner.scheduler,&work,1);
        if(!job->task) {
            job->progress.preparation=OCTARYN_SCENE_LOADING_FAILED;job->error="host worker submission failed";
            job->reservation.reset();owner.active.reset();
        }
        return;
    }
}
}
bool reserve_verification_work() {return reserve(working_bytes);}
void release_verification_work() {reserved.fetch_sub(working_bytes);}
std::shared_ptr<const Snapshot> snapshot(void* value,const octaryn_scene_loading_ticket& ticket) {
    if(!value)return {};auto& owner=*static_cast<Owner*>(value);std::lock_guard lock(owner.mutex);pump(owner);
    const auto job=find(owner,ticket);return job && job->progress.preparation==OCTARYN_SCENE_LOADING_CPU_PREPARED?job->result:nullptr;
}
}
using namespace octaryn::scene_loading;
void* octaryn_scene_loading_create(const char* module,const char* root,void* scheduler) {
    try {
        if(!module || !root || !scheduler || !*module || std::strlen(module)>128)return nullptr;
        auto owner=std::make_unique<Owner>();owner->module=module;owner->scheduler=scheduler;
        owner->root=std::filesystem::canonical(octaryn::content::file_io_path(std::filesystem::path(reinterpret_cast<const char8_t*>(root))));
        if(!std::filesystem::is_directory(owner->root))return nullptr;return owner.release();
    }catch(...) {return nullptr;}
}
int octaryn_scene_loading_begin(void* value,const char* source,octaryn_scene_loading_ticket* ticket) {
    if(ticket)*ticket={};if(!value || !source || !ticket)return -1;
    try {
        auto& owner=*static_cast<Owner*>(value);std::lock_guard lock(owner.mutex);if(owner.closed)return -1;pump(owner);
        Preparation work{owner.root,{},nullptr,{}};
        const auto path=work.confined(std::filesystem::path(reinterpret_cast<const char8_t*>(source)));
        if(path.extension()!=".gltf" && path.extension()!=".json")return -1;
        const auto used=std::count_if(owner.tickets.begin(),owner.tickets.end(),[](const auto& job){return bool(job);});
        if(used+((owner.active && owner.active->released)?1:0)>=8)return -2;
        for(std::size_t slot=0;slot<owner.tickets.size();++slot)if(!owner.tickets[slot]) {
            auto job=std::make_shared<Job>();job->owner=&owner;job->input=path;job->ticket={slot+1,sequence.fetch_add(1)};
            *ticket=job->ticket;owner.tickets[slot]=std::move(job);pump(owner);return 0;
        }
        return -2;
    }catch(...) {return -1;}
}
int octaryn_scene_loading_query(void* value,const octaryn_scene_loading_ticket* ticket,octaryn_scene_loading_progress* progress) {
    if(progress)*progress={};if(!value || !ticket || !progress)return -1;
    auto& owner=*static_cast<Owner*>(value);std::lock_guard lock(owner.mutex);pump(owner);const auto job=find(owner,*ticket);
    if(!job)return -2;*progress=job->progress;return 0;
}
int octaryn_scene_loading_cancel(void* value,const octaryn_scene_loading_ticket* ticket) {
    if(!value || !ticket)return -1;auto& owner=*static_cast<Owner*>(value);std::lock_guard lock(owner.mutex);
    const auto job=find(owner,*ticket);if(!job)return -2;
    job->canceled=true;job->progress.preparation=OCTARYN_SCENE_LOADING_CANCELED;job->progress.retained_bytes=0;job->result.reset();pump(owner);return 0;
}
int octaryn_scene_loading_release(void* value,const octaryn_scene_loading_ticket* ticket) {
    if(!value || !ticket)return -1;auto& owner=*static_cast<Owner*>(value);std::lock_guard lock(owner.mutex);
    const auto job=find(owner,*ticket);if(!job)return -2;job->released=true;job->canceled=true;job->result.reset();
    owner.tickets[ticket->id-1].reset();pump(owner);return 0;
}
int octaryn_scene_loading_error(void* value,const octaryn_scene_loading_ticket* ticket,char* output,uint32_t capacity) {
    if(!value || !ticket || !output || !capacity)return -1;output[0]=0;
    auto& owner=*static_cast<Owner*>(value);std::lock_guard lock(owner.mutex);const auto job=find(owner,*ticket);if(!job)return -2;
    const auto count=std::min<std::size_t>(job->error.size(),capacity-1);std::memcpy(output,job->error.data(),count);output[count]=0;return 0;
}
void octaryn_scene_loading_destroy(void* value) {
    if(!value)return;auto* owner=static_cast<Owner*>(value);void* task{};
    {
        std::lock_guard lock(owner->mutex);owner->closed=true;
        for(const auto& job:owner->tickets)if(job) {job->canceled=true;job->result.reset();}
        if(owner->active) {owner->active->canceled=true;task=owner->active->task;}
    }
    if(task)octaryn_native_schedule_runtime_task_destroy(task);delete owner;
}
