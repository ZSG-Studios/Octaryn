#pragma once
#include <stdint.h>
#if defined(_WIN32)
#if defined(OCTARYN_SCENE_LOADING_BUILD)
#define OCTARYN_SCENE_LOADING_API __declspec(dllexport)
#else
#define OCTARYN_SCENE_LOADING_API __declspec(dllimport)
#endif
#else
#define OCTARYN_SCENE_LOADING_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct octaryn_scene_loading_ticket {uint64_t id,generation;} octaryn_scene_loading_ticket;
// Worker-only verification using the same bounded descriptor/resource admission
// as asynchronous preparation. No renderer or publication is performed.
OCTARYN_SCENE_LOADING_API int octaryn_scene_loading_verify(const char* root,const char* descriptor,char* error,uint32_t capacity);
typedef struct octaryn_scene_loading_progress {
    uint32_t preparation,publication;
    uint64_t completed,total,retained_bytes;
} octaryn_scene_loading_progress;
enum {OCTARYN_SCENE_LOADING_QUEUED=0,OCTARYN_SCENE_LOADING_RUNNING=1,OCTARYN_SCENE_LOADING_CPU_PREPARED=2,
      OCTARYN_SCENE_LOADING_FAILED=3,OCTARYN_SCENE_LOADING_CANCELED=4};
OCTARYN_SCENE_LOADING_API void* octaryn_scene_loading_create(const char* module_id,const char* module_root,void* schedule_runtime);
OCTARYN_SCENE_LOADING_API int octaryn_scene_loading_begin(void*,const char* source,octaryn_scene_loading_ticket*);
OCTARYN_SCENE_LOADING_API int octaryn_scene_loading_query(void*,const octaryn_scene_loading_ticket*,octaryn_scene_loading_progress*);
OCTARYN_SCENE_LOADING_API int octaryn_scene_loading_cancel(void*,const octaryn_scene_loading_ticket*);
OCTARYN_SCENE_LOADING_API int octaryn_scene_loading_release(void*,const octaryn_scene_loading_ticket*);
OCTARYN_SCENE_LOADING_API void octaryn_scene_loading_destroy(void*);
OCTARYN_SCENE_LOADING_API int octaryn_scene_loading_error(void*,const octaryn_scene_loading_ticket*,char*,uint32_t capacity);
#ifdef __cplusplus
}
#include "SceneSnapshot.h"
#include <memory>
namespace octaryn::scene_loading {
OCTARYN_SCENE_LOADING_API std::shared_ptr<const Snapshot> snapshot(void*,const octaryn_scene_loading_ticket&);
}
static_assert(sizeof(octaryn_scene_loading_ticket)==16 && sizeof(octaryn_scene_loading_progress)==32);
#endif
