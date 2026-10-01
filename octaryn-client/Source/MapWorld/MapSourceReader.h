#pragma once
#include "MapSource.h"
#include <atomic>
#include <memory>
#include <span>

namespace octaryn::client::rendering {
class MapSourceReader {
public:
  MapSourceReader();
  ~MapSourceReader();
  bool open(const std::filesystem::path& source,const std::filesystem::path& scratch,std::string& error,
      const std::atomic_bool* cancel=nullptr);
  bool load(std::size_t mesh,std::size_t primitive,std::uint64_t first_triangle,
      std::size_t triangle_count,MapModel&,std::string& error);
  bool load(std::size_t mesh,std::size_t primitive,std::span<const std::uint64_t> triangles,MapModel&,std::string& error);
  const MapSourceInfo& info() const;
private:
  struct State;
  std::unique_ptr<State> state_;
  bool load_triangles(std::size_t mesh,std::size_t primitive,std::uint64_t first,std::size_t count,
      std::span<const std::uint64_t>,MapModel&,std::string&);
};
}
