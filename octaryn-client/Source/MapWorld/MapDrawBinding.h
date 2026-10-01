#pragma once
#include "MapRenderer.h"
namespace octaryn::client::rendering {
namespace virtual_geometry {struct GeometryTransform;}
bool bind_map_geometry(MapRenderer&,rhi::IShaderObject*);
bool bind_map_draw_uniforms(MapRenderer&,rhi::IShaderObject*,const MapPrimitive&,const WorldRenderer&,
    const virtual_geometry::GeometryTransform* instance=nullptr);
}
