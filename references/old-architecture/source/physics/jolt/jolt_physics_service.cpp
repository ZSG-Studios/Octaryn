#include "physics/jolt/jolt_physics_service.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <memory>

#include "core/log.h"
#include "core/profile.h"
#include "physics/jolt/jolt_layers.h"
#include "physics/jolt/jolt_trace.h"
#include "physics/jolt/jolt_worker_jobs.h"
#include "runtime/jobs/runtime_worker_pool.h"

namespace octaryn::physics {
namespace {

constexpr JPH::uint kMaxBodies = 65536;
constexpr JPH::uint kNumBodyMutexes = 0;
constexpr JPH::uint kMaxBodyPairs = 65536;
constexpr JPH::uint kMaxContactConstraints = 32768;
constexpr float kFixedStepSeconds = 1.0f / 60.0f;
constexpr int kMaxCatchUpSteps = 2;
constexpr size_t kTempAllocatorBytes = 16 * 1024 * 1024;

bool g_jolt_types_registered = false;

void register_jolt_runtime()
{
    JPH::RegisterDefaultAllocator();
    JPH::Trace = jolt_trace_callback;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = jolt_assert_callback;)

    if (JPH::Factory::sInstance == nullptr)
    {
        JPH::Factory::sInstance = new JPH::Factory();
    }
    if (!g_jolt_types_registered)
    {
        JPH::RegisterTypes();
        g_jolt_types_registered = true;
    }
}

void unregister_jolt_runtime()
{
    if (g_jolt_types_registered)
    {
        JPH::UnregisterTypes();
        g_jolt_types_registered = false;
    }
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

} // namespace

struct JoltPhysicsService {
    jolt_layers::BroadPhaseLayerInterface broadphase_layers;
    jolt_layers::ObjectVsBroadPhaseLayerFilter object_vs_broadphase_filter;
    jolt_layers::ObjectLayerPairFilter object_pair_filter;
    std::unique_ptr<JPH::PhysicsSystem> system;
    std::unique_ptr<JPH::TempAllocatorImplWithMallocFallback> temp_allocator;
    std::unique_ptr<JoltRuntimeWorkerJobSystem> job_system;
    double accumulator_seconds = 0.0;
    bool initialized = false;
};

JoltPhysicsService& jolt_physics_service()
{
    static JoltPhysicsService service;
    return service;
}

bool jolt_physics_service_startup()
{
    JoltPhysicsService& service = jolt_physics_service();
    if (service.initialized)
    {
        return true;
    }

    register_jolt_runtime();
    service.temp_allocator = std::make_unique<JPH::TempAllocatorImplWithMallocFallback>(kTempAllocatorBytes);
    service.job_system = jolt_create_runtime_worker_job_system(runtime_worker_pool_shared());
    service.system = std::make_unique<JPH::PhysicsSystem>();
    service.system->Init(kMaxBodies,
                         kNumBodyMutexes,
                         kMaxBodyPairs,
                         kMaxContactConstraints,
                         service.broadphase_layers,
                         service.object_vs_broadphase_filter,
                         service.object_pair_filter);
    service.accumulator_seconds = 0.0;
    service.initialized = true;
    oct_log_infof("Jolt initialized: max_bodies=%u body_mutexes=%u max_body_pairs=%u max_contact_constraints=%u max_concurrency=%d",
                  kMaxBodies,
                  kNumBodyMutexes,
                  kMaxBodyPairs,
                  kMaxContactConstraints,
                  service.job_system->GetMaxConcurrency());
    return true;
}

void jolt_physics_service_tick(double frame_seconds)
{
    JoltPhysicsService& service = jolt_physics_service();
    if (!service.initialized || service.system == nullptr)
    {
        return;
    }
    if (service.system->GetNumBodies() == 0u)
    {
        service.accumulator_seconds = 0.0;
        return;
    }

    OCT_PROFILE_ZONE("Jolt physics tick");
    service.accumulator_seconds += frame_seconds;
    const double max_accumulator = static_cast<double>(kFixedStepSeconds) * static_cast<double>(kMaxCatchUpSteps);
    if (service.accumulator_seconds > max_accumulator)
    {
        service.accumulator_seconds = max_accumulator;
    }

    int steps = 0;
    while (service.accumulator_seconds >= kFixedStepSeconds && steps < kMaxCatchUpSteps)
    {
        service.system->Update(kFixedStepSeconds, 1, service.temp_allocator.get(), service.job_system.get());
        service.job_system->wait_for_submitted_jobs();
        service.accumulator_seconds -= kFixedStepSeconds;
        ++steps;
    }
}

void jolt_physics_service_shutdown()
{
    JoltPhysicsService& service = jolt_physics_service();
    if (!service.initialized)
    {
        return;
    }
    service.system.reset();
    service.job_system.reset();
    service.temp_allocator.reset();
    service.accumulator_seconds = 0.0;
    service.initialized = false;
    unregister_jolt_runtime();
    oct_log_infof("Jolt shutdown complete");
}

bool jolt_physics_service_initialized()
{
    return jolt_physics_service().initialized;
}

Uint32 jolt_physics_service_body_count()
{
    JoltPhysicsService& service = jolt_physics_service();
    if (!service.initialized || service.system == nullptr)
    {
        return 0u;
    }
    return service.system->GetNumBodies();
}

int jolt_physics_service_active_job_count()
{
    JoltPhysicsService& service = jolt_physics_service();
    if (!service.initialized || service.job_system == nullptr)
    {
        return 0;
    }
    return service.job_system->active_job_count();
}

} // namespace octaryn::physics
