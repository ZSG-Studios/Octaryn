#include "world/jobs/taskflow/internal.h"

#include "render/world/voxel.h"

namespace world_jobs_taskflow_detail {

namespace {

constexpr Uint32 kInitialOpaqueFaceCapacity = 65536;
constexpr Uint32 kInitialTransparentFaceCapacity = 8192;
constexpr Uint32 kInitialWaterVertexCapacity = 16384;
constexpr Uint32 kInitialSpriteVertexCapacity = 8192;

} // namespace

auto slots() -> std::array<taskflow_slot, kTaskflowSlotCount>&
{
    static std::array<taskflow_slot, kTaskflowSlotCount> g_slots{};
    return g_slots;
}

auto busy_slot_count() -> std::atomic_int&
{
    static std::atomic_int g_busy_slots{0};
    return g_busy_slots;
}

int cpu_slot_count()
{
    int count = 0;
    for (const taskflow_slot& slot : slots())
    {
        if (slot.busy.load(std::memory_order_acquire) &&
            !slot.ready_to_upload.load(std::memory_order_acquire))
        {
            ++count;
        }
    }
    return count;
}

int ready_upload_count()
{
    int count = 0;
    for (const taskflow_slot& slot : slots())
    {
        if (slot.busy.load(std::memory_order_acquire) &&
            slot.ready_to_upload.load(std::memory_order_acquire))
        {
            ++count;
        }
    }
    return count;
}

void init_slots(SDL_GPUDevice* device)
{
    for (taskflow_slot& slot : slots())
    {
        slot.busy.store(false);
        slot.queued_for_cpu.store(false);
        slot.ready_to_upload.store(false);
        cpu_buffer_init(&slot.voxels[MESH_TYPE_OPAQUE], device, sizeof(packed_face_t));
        cpu_buffer_init(&slot.voxels[MESH_TYPE_TRANSPARENT], device, sizeof(packed_face_t));
        cpu_buffer_init(&slot.voxels[MESH_TYPE_WATER], device, sizeof(water_vertex_t));
        cpu_buffer_init(&slot.voxels[MESH_TYPE_LAVA], device, sizeof(water_vertex_t));
        cpu_buffer_init(&slot.voxels[MESH_TYPE_SPRITE], device, sizeof(sprite_voxel_t));
        cpu_buffer_init(&slot.skylight, device, sizeof(Uint8));
        cpu_buffer_reserve(&slot.voxels[MESH_TYPE_OPAQUE], kInitialOpaqueFaceCapacity);
        cpu_buffer_reserve(&slot.voxels[MESH_TYPE_TRANSPARENT], kInitialTransparentFaceCapacity);
        cpu_buffer_reserve(&slot.voxels[MESH_TYPE_WATER], kInitialWaterVertexCapacity);
        cpu_buffer_reserve(&slot.voxels[MESH_TYPE_LAVA], kInitialWaterVertexCapacity);
        cpu_buffer_reserve(&slot.voxels[MESH_TYPE_SPRITE], kInitialSpriteVertexCapacity);
    }
}

void free_slots()
{
    for (taskflow_slot& slot : slots())
    {
        for (int i = 0; i < MESH_TYPE_COUNT; ++i)
        {
            cpu_buffer_free(&slot.voxels[i]);
        }
        cpu_buffer_free(&slot.skylight);
        slot.busy.store(false);
        slot.queued_for_cpu.store(false);
        slot.ready_to_upload.store(false);
    }
    busy_slot_count().store(0);
}

auto try_claim_slot(int slot_limit) -> taskflow_slot*
{
    const int clamped_limit = SDL_clamp(slot_limit, 0, OCTARYN_WORLD_JOBS_MAX_WORKERS);
    if (cpu_slot_count() >= clamped_limit)
    {
        return nullptr;
    }

    for (taskflow_slot& slot : slots())
    {
        bool expected = false;
        if (slot.busy.compare_exchange_strong(expected, true))
        {
            busy_slot_count().fetch_add(1);
            return &slot;
        }
    }
    return nullptr;
}

void release_slot(taskflow_slot& slot)
{
    slot.ready_to_upload.store(false, std::memory_order_release);
    slot.queued_for_cpu.store(false, std::memory_order_release);
    slot.chunk = nullptr;
    slot.busy.store(false);
    busy_slot_count().fetch_sub(1);
}

} // namespace world_jobs_taskflow_detail
