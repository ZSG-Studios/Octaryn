#pragma once

#include <SDL3/SDL.h>

#include "render/pipelines/pipelines.h"
#include "render/resources/resources.h"
#include "app/player/player.h"

#define MAIN_RENDER_HIDDEN_BLOCK_CAPACITY 32

typedef struct main_render_pass_context
{
    const main_render_resources_t* resources;
    const main_pipelines_t* pipelines;
    const player_t* player;
    bool fog_enabled;
    bool clouds_enabled;
    bool sky_gradient_enabled;
    bool stars_enabled;
    bool sun_enabled;
    bool moon_enabled;
    bool pom_enabled;
    bool pbr_enabled;
    float fog_distance;
    float skylight_floor;
    float ambient_strength;
    float sun_strength;
    float sun_fallback_strength;
    float visual_sky_visibility;
    float twilight_strength;
    float celestial_visibility;
    float world_time_seconds;
    float cloud_time_seconds;
    float cloud_max_distance;
    float visual_light_direction[3];
    Uint32 hidden_block_count;
    int hidden_blocks[MAIN_RENDER_HIDDEN_BLOCK_CAPACITY][4];
}
main_render_pass_context_t;

typedef struct main_frame_profile_sample main_frame_profile_sample_t;

void main_render_pass_gbuffer(SDL_GPUCommandBuffer* cbuf,
                              const main_render_pass_context_t* context,
                              main_frame_profile_sample_t* profile_sample);
void main_render_pass_composite(SDL_GPUCommandBuffer* cbuf, const main_render_pass_context_t* context);
void main_render_pass_depth(SDL_GPUCommandBuffer* cbuf, const main_render_pass_context_t* context);
void main_render_pass_forward(SDL_GPUCommandBuffer* cbuf, const main_render_pass_context_t* context);
