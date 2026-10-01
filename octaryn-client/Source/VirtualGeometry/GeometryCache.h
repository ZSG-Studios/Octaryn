#pragma once
#include "GeometryFormat.h"
#include <filesystem>

namespace octaryn::client::rendering::virtual_geometry {
bool write_geometry_cache(const std::filesystem::path&,const GeometryAsset&,std::string& error);
bool read_geometry_cache(const std::filesystem::path&,const std::string& source_hash,
    GeometryAsset&,std::string& error,bool load_payloads=true);
bool read_geometry_page(const std::filesystem::path&,const GeometryPage&,
    std::vector<std::uint8_t>& decoded,std::string& error);
}
