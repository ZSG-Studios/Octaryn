#pragma once
#include <slang-rhi.h>
#include "SelectionTarget.h"
#include <slang-com-ptr.h>
namespace octaryn::client::rendering {
struct WorldCamera;

bool create_selection_pipeline(rhi::IDevice*,rhi::Format,rhi::Format,Slang::ComPtr<rhi::IRenderPipeline>&);
bool render_selection(rhi::IRenderPassEncoder*,rhi::IRenderPipeline*,const WorldCamera&,
                      int width,int height,const SelectionTarget&);
}
