#pragma once
#include "GeometryFormat.h"
#include <span>

namespace octaryn::client::rendering::virtual_geometry {
bool decode_geometry_payload(const GeometryPage&,std::span<const std::uint8_t>,
    std::vector<std::uint8_t>&,std::string& error);
}
