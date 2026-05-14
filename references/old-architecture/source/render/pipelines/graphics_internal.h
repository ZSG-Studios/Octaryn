#pragma once

#include "render/pipelines/internal.h"

void main_pipelines_graphics_init_mesh_vertex_input(SDL_GPUVertexAttribute (&vertex_attributes)[1],
                                                    SDL_GPUVertexBufferDescription (&vertex_buffers)[1]);
void main_pipelines_graphics_init_alpha_blend_target(SDL_GPUColorTargetDescription& color_target,
                                                     SDL_GPUTextureFormat format);
bool main_pipelines_graphics_create_pipeline(SDL_GPUGraphicsPipeline** pipeline, SDL_GPUDevice* device,
                                             SDL_GPUGraphicsPipelineCreateInfo& info, const char* vertex_path,
                                             const char* fragment_path);
