#pragma once
#include <slang-rhi.h>
#include <string>

namespace octaryn::client::rendering::virtual_geometry {
class GeometryStream;
bool upload_geometry_roots(GeometryStream&,rhi::IDevice*,rhi::ICommandQueue*,std::string&);
}
