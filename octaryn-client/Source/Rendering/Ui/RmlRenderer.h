#pragma once
#include <slang-rhi.h>

namespace Rml { class Context; class RenderInterface; }
namespace octaryn::client::rendering {
struct RmlRenderer;
RmlRenderer* create_rml_renderer(rhi::IDevice*, rhi::Format);
Rml::RenderInterface* rml_render_interface(RmlRenderer*);
bool render_rml(RmlRenderer*, rhi::ICommandEncoder*, rhi::ITextureView*,
                Rml::Context*, int width, int height);
void destroy_rml_renderer(RmlRenderer*);
}
