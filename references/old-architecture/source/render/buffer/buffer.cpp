#include <SDL3/SDL.h>

#include "render/buffer/buffer.h"
#include "core/check.h"

void cpu_buffer_init(cpu_buffer_t* cpu, SDL_GPUDevice* device, Uint32 stride)
{
    CHECK(stride);
    cpu->device = device;
    cpu->buffer = NULL;
    cpu->data = NULL;
    cpu->capacity = 0;
    cpu->size = 0;
    cpu->stride = stride;
    cpu->payload = CPU_BUFFER_PAYLOAD_GENERIC;
}

void cpu_buffer_free(cpu_buffer_t* cpu)
{
    if (cpu->data && cpu->buffer)
    {
        SDL_UnmapGPUTransferBuffer(cpu->device, cpu->buffer);
    }
    SDL_ReleaseGPUTransferBuffer(cpu->device, cpu->buffer);
    cpu->device = NULL;
    cpu->buffer = NULL;
    cpu->data = NULL;
    cpu->capacity = 0;
    cpu->size = 0;
    cpu->stride = 0;
    cpu->payload = CPU_BUFFER_PAYLOAD_GENERIC;
}

bool cpu_buffer_reserve(cpu_buffer_t* cpu, Uint32 count)
{
    if (!count)
    {
        return true;
    }
    if (!cpu->data && cpu->buffer)
    {
        CHECK(!cpu->size);
        cpu->data = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(cpu->device, cpu->buffer, true));
        if (!cpu->data)
        {
            SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
            return false;
        }
    }
    CHECK(cpu->size <= cpu->capacity);
    while (count > cpu->capacity)
    {
        const Uint32 capacity = SDL_max(64u, cpu->capacity ? cpu->capacity * 2u : count);
        SDL_GPUTransferBufferCreateInfo info = {};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = capacity * cpu->stride;
        SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer(cpu->device, &info);
        if (!buffer)
        {
            SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
            return false;
        }
        Uint8* data = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(cpu->device, buffer, false));
        if (!data)
        {
            SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
            SDL_ReleaseGPUTransferBuffer(cpu->device, buffer);
            return false;
        }
        if (cpu->data)
        {
            SDL_memcpy(data, cpu->data, cpu->size * cpu->stride);
            SDL_UnmapGPUTransferBuffer(cpu->device, cpu->buffer);
        }
        SDL_ReleaseGPUTransferBuffer(cpu->device, cpu->buffer);
        cpu->capacity = capacity;
        cpu->buffer = buffer;
        cpu->data = data;
    }
    return true;
}

void cpu_buffer_append(cpu_buffer_t* cpu, void* item)
{
    if (!cpu_buffer_reserve(cpu, cpu->size + 1u))
    {
        return;
    }
    CHECK(cpu->data);
    SDL_memcpy(cpu->data + cpu->size * cpu->stride, item, cpu->stride);
    cpu->size++;
}

void cpu_buffer_append_many(cpu_buffer_t* cpu, const void* items, Uint32 count)
{
    if (!count)
    {
        return;
    }
    const Uint32 required = cpu->size + count;
    if (!cpu_buffer_reserve(cpu, required))
    {
        return;
    }
    CHECK(cpu->data);
    SDL_memcpy(cpu->data + cpu->size * cpu->stride, items, static_cast<size_t>(count) * cpu->stride);
    cpu->size += count;
}

void cpu_buffer_clear(cpu_buffer_t* cpu)
{
    cpu->size = 0;
}
