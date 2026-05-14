#include <SDL3/SDL.h>

#include "render/buffer/buffer.h"
#include "core/check.h"
#include "render/buffer/internal.h"

namespace {

thread_local SDL_GPUCommandBuffer* g_upload_command_buffer = nullptr;
thread_local SDL_GPUCopyPass* g_upload_copy_pass = nullptr;
thread_local gpu_buffer_t* g_pending_list_head = nullptr;

void add_to_pending_list(gpu_buffer_t* gpu)
{
    // Avoid duplicates in the pending list
    gpu_buffer_t* curr = g_pending_list_head;
    while (curr)
    {
        if (curr == gpu) return;
        curr = curr->next_pending;
    }
    gpu->next_pending = g_pending_list_head;
    g_pending_list_head = gpu;
}

void discard_pending_uploads()
{
    gpu_buffer_t* curr = g_pending_list_head;
    while (curr)
    {
        if (curr->has_pending && curr->pending_buffer && curr->pending_buffer != curr->buffer)
        {
            SDL_ReleaseGPUBuffer(curr->device, curr->pending_buffer);
        }
        curr->pending_buffer = nullptr;
        curr->pending_size = 0;
        curr->pending_capacity = 0;
        curr->has_pending = false;
        gpu_buffer_t* next = curr->next_pending;
        curr->next_pending = nullptr;
        curr = next;
    }
    g_pending_list_head = nullptr;
}

} // namespace

auto gpu_upload_command_buffer() -> SDL_GPUCommandBuffer*&
{
    return g_upload_command_buffer;
}

auto gpu_upload_copy_pass() -> SDL_GPUCopyPass*&
{
    return g_upload_copy_pass;
}

void gpu_buffer_init(gpu_buffer_t* gpu, SDL_GPUDevice* device, SDL_GPUBufferUsageFlags usage)
{
    gpu->device = device;
    gpu->usage = usage;
    gpu->buffer = NULL;
    gpu->size = 0;
    gpu->capacity = 0;
    gpu->pending_buffer = NULL;
    gpu->pending_size = 0;
    gpu->pending_capacity = 0;
    gpu->has_pending = false;
    gpu->next_pending = nullptr;
}

void gpu_buffer_free(gpu_buffer_t* gpu)
{
    if (gpu->buffer)
    {
        SDL_ReleaseGPUBuffer(gpu->device, gpu->buffer);
    }
    gpu->usage = 0;
    gpu->buffer = NULL;
    gpu->capacity = 0;
    gpu->size = 0;
    gpu->pending_buffer = NULL;
    gpu->pending_size = 0;
    gpu->pending_capacity = 0;
    gpu->has_pending = false;
    gpu->next_pending = nullptr;
}

bool gpu_buffer_upload(gpu_buffer_t* gpu, cpu_buffer_t* cpu)
{
    SDL_GPUCommandBuffer* const command_buffer = gpu_upload_command_buffer();
    SDL_GPUCopyPass* const copy_pass = gpu_upload_copy_pass();
    CHECK(command_buffer);
    CHECK(copy_pass);
    if (!cpu->size)
    {
        gpu->pending_size = 0;
        gpu->pending_buffer = gpu->buffer;
        gpu->pending_capacity = gpu->capacity;
        gpu->has_pending = true;
        add_to_pending_list(gpu);
        return true;
    }

    const Uint32 size = cpu->size;

    SDL_GPUBuffer* target_buffer = gpu->buffer;
    if (!gpu->buffer || size > gpu->capacity)
    {
        SDL_GPUBufferCreateInfo info = {};
        info.usage = gpu->usage;
        info.size = cpu->capacity * cpu->stride;
        target_buffer = SDL_CreateGPUBuffer(gpu->device, &info);
        if (!target_buffer)
        {
            SDL_Log("Failed to create buffer: %s", SDL_GetError());
            return false;
        }
    }

    CHECK(cpu->data);
    SDL_UnmapGPUTransferBuffer(gpu->device, cpu->buffer);
    cpu->data = NULL;

    SDL_GPUTransferBufferLocation location = {};
    SDL_GPUBufferRegion region = {};
    location.transfer_buffer = cpu->buffer;
    region.buffer = target_buffer;
    region.size = size * cpu->stride;
    const bool cycle = target_buffer == gpu->buffer;
    SDL_UploadToGPUBuffer(copy_pass, &location, &region, cycle);

    cpu->size = 0;
    gpu->pending_buffer = target_buffer;
    gpu->pending_size = size;
    gpu->pending_capacity = target_buffer == gpu->buffer ? gpu->capacity : cpu->capacity;
    gpu->has_pending = true;
    add_to_pending_list(gpu);
    return true;
}

void gpu_buffer_clear(gpu_buffer_t* gpu)
{
    if (g_upload_command_buffer)
    {
        gpu->pending_size = 0;
        gpu->pending_buffer = gpu->buffer;
        gpu->pending_capacity = gpu->capacity;
        gpu->has_pending = true;
        add_to_pending_list(gpu);
    }
    else
    {
        gpu->size = 0;
    }
}

bool gpu_buffer_begin_upload(gpu_buffer_t* gpu)
{
    SDL_GPUCommandBuffer*& command_buffer = gpu_upload_command_buffer();
    SDL_GPUCopyPass*& copy_pass = gpu_upload_copy_pass();
    CHECK(!command_buffer);
    CHECK(!copy_pass);
    CHECK(g_pending_list_head == nullptr);
    command_buffer = SDL_AcquireGPUCommandBuffer(gpu->device);
    if (!command_buffer)
    {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return false;
    }
    copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    if (!copy_pass)
    {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(command_buffer);
        command_buffer = NULL;
        return false;
    }
    return true;
}

void gpu_buffer_abort_upload(gpu_buffer_t* gpu)
{
    (void) gpu;
    SDL_GPUCommandBuffer*& command_buffer = gpu_upload_command_buffer();
    SDL_GPUCopyPass*& copy_pass = gpu_upload_copy_pass();
    CHECK(copy_pass);
    CHECK(command_buffer);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_CancelGPUCommandBuffer(command_buffer);
    discard_pending_uploads();
    copy_pass = NULL;
    command_buffer = NULL;
}

bool gpu_buffer_end_upload(gpu_buffer_t* gpu)
{
    (void) gpu;
    SDL_GPUCommandBuffer*& command_buffer = gpu_upload_command_buffer();
    SDL_GPUCopyPass*& copy_pass = gpu_upload_copy_pass();
    CHECK(copy_pass);
    CHECK(command_buffer);
    SDL_EndGPUCopyPass(copy_pass);
    const bool submitted = SDL_SubmitGPUCommandBuffer(command_buffer);
    if (!submitted)
    {
        SDL_Log("Failed to submit upload command buffer: %s", SDL_GetError());
        discard_pending_uploads();
        copy_pass = NULL;
        command_buffer = NULL;
        return false;
    }

    // Commit pending state after submission
    gpu_buffer_t* curr = g_pending_list_head;
    while (curr)
    {
        if (curr->has_pending)
        {
            if (curr->buffer && curr->buffer != curr->pending_buffer)
            {
                SDL_ReleaseGPUBuffer(curr->device, curr->buffer);
            }
            curr->buffer = curr->pending_buffer;
            curr->size = curr->pending_size;
            curr->capacity = curr->pending_capacity;
            curr->has_pending = false;
        }
        gpu_buffer_t* next = curr->next_pending;
        curr->next_pending = nullptr;
        curr = next;
    }
    g_pending_list_head = nullptr;

    copy_pass = NULL;
    command_buffer = NULL;
    return true;
}
