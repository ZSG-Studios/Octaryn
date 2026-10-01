#include "CollisionResidency.h"
#include "MapSceneGeometry.h"
#include "SceneCollisionResidency.h"
#include "octaryn_native_schedule_runtime.h"
#include <box3d/collision.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace octaryn::server::map_world {
namespace {
using Clock = std::chrono::steady_clock;
constexpr uint64_t JobReservation = 72ull * 1024 * 1024;
constexpr size_t MaximumResidents = 256;
constexpr auto KeepTime = std::chrono::seconds(2);

uint64_t configured_budget() {
  const char* value = std::getenv("OCTARYN_SERVER_COLLISION_BUDGET_MIB");
  if (!value || !*value) return 512ull * 1024 * 1024;
  char* end{};
  const auto mib = std::strtoul(value, &end, 10);
  if (*end || mib < 64 || mib > 4096) throw std::runtime_error("Collision budget must be 64..4096 MiB");
  return mib * 1024ull * 1024ull;
}

struct Preparation {
  std::filesystem::path path;
  std::array<float, 6> bounds;
  std::unique_ptr<character_motion::PreparedCollisionTile> tile;
  uint64_t bytes{};
  uint64_t triangles{};
  static int execute(void* context) {
    try { return static_cast<Preparation*>(context)->prepare(); }
    catch (...) { return -5; }
  }
  int prepare() {
    MapTriangleSoup soup;
    soup.max_file_bytes = 64ull * 1024 * 1024;
    soup.max_triangles = 250000;
    if (!load_map_triangle_soup(path, soup)) return -1;
    triangles = soup.triangle_count();
    for (size_t i = 0; i < soup.positions.size(); ++i) {
      const auto axis = i % 3;
      if (!std::isfinite(soup.positions[i]) || soup.positions[i] < bounds[axis] - .01f ||
          soup.positions[i] > bounds[axis + 3] + .01f) return -4;
    }
    bytes = soup.triangle_count() * 256ull + soup.positions.size() * sizeof(float) + 65536;
    if (bytes > JobReservation) return -2;
    tile = character_motion::MeshCollisionScene::prepare_tile(
        {soup.positions.data(), soup.positions.size(), soup.indices.data(), soup.indices.size()});
    return tile ? 0 : -3;
  }
};
}

struct CollisionResidency::State {
  struct Entry {
    Clock::time_point required{}, wanted{};
    uint64_t bytes{};
    uint64_t triangles{};
    bool resident{}, failed{}, queued{};
  };
  struct Job {
    size_t id{};
    std::unique_ptr<Preparation> preparation;
    void* task{};
    Clock::time_point budget_wait{};
  };
  std::filesystem::path directory;
  std::vector<std::string> paths;
  std::vector<std::array<float, 6>> bounds;
  std::vector<Entry> entries;
  std::vector<size_t> active;
  std::vector<size_t> required;
  std::array<Job, 2> jobs;
  b3DynamicTree tree{};
  void* workers{};
  character_motion::MeshCollisionScene scene;
  CollisionResidencyStats counters{2};
  Clock::time_point next_pump{};

  State(const MapManifest& manifest, const std::filesystem::path& root)
      : directory(root), paths(manifest.tile_files), bounds(manifest.tiles), entries(paths.size()) {
    counters.budget_bytes = configured_budget();
    active.reserve(paths.size());
    required.reserve(paths.size());
    tree = b3DynamicTree_Create(static_cast<int>(paths.size()));
    for (size_t id = 0; id < paths.size(); ++id) {
      const auto& b = manifest.tiles[id];
      b3DynamicTree_CreateProxy(&tree, {{b[0], -1e6f, b[2]}, {b[3], 1e6f, b[5]}}, 1, id);
    }
    b3DynamicTree_Rebuild(&tree, true);
    workers = octaryn_native_schedule_runtime_create(2, 2);
    if (!workers) {
      b3DynamicTree_Destroy(&tree);
      throw std::runtime_error("Could not create authority collision workers");
    }
  }
  ~State() {
    for (auto& job : jobs) if (job.task) octaryn_native_schedule_runtime_task_destroy(job.task);
    octaryn_native_schedule_runtime_destroy(workers);
    b3DynamicTree_Destroy(&tree);
  }

  void pump(Clock::time_point now) {
    if (now < next_pump) return;
    next_pump = now + std::chrono::milliseconds(1);
    const bool pressure = counters.resident_bytes + JobReservation > counters.budget_bytes ||
        counters.resident >= MaximumResidents;
    // Never evict a tile required by a recently queried player or awake item.
    for (size_t id : active) {
      auto& entry = entries[id];
      if (!entry.resident || entry.required >= now || (!pressure && entry.wanted >= now)) continue;
      scene.remove_tile(id);
      entry.resident = false;
      counters.reserved_bytes -= entry.bytes;
      counters.resident_bytes -= entry.bytes;
      entry.bytes = 0;
      --counters.resident;
      ++counters.evictions;
    }
    for (auto& job : jobs) {
      if (!job.preparation) continue;
      auto& entry = entries[job.id];
      if (job.task) {
        if (!octaryn_native_schedule_runtime_task_ready(job.task)) continue;
        const int result = octaryn_native_schedule_runtime_task_result(job.task, nullptr);
        octaryn_native_schedule_runtime_task_destroy(job.task);
        job.task = nullptr;
        counters.reserved_bytes -= JobReservation;
        if (result != 0) {
          entry.failed = true;
          ++counters.failed;
          std::fprintf(stderr, "server_collision_tile failed=%zu result=%d\n", job.id, result);
          job.preparation.reset();
          --counters.preparing;
          continue;
        }
        counters.reserved_bytes += job.preparation->bytes;
      }
      const auto bytes = job.preparation->bytes;
      const bool fits = counters.resident_bytes + bytes <= counters.budget_bytes && counters.resident < MaximumResidents;
      if (entry.wanted < now || (!fits && entry.required < now)) {
        ++counters.cancelled;
      } else if (!fits) {
        if (job.budget_wait == Clock::time_point{}) job.budget_wait = now;
        if (now - job.budget_wait <= KeepTime + std::chrono::milliseconds(100)) continue;
        entry.failed = true;
        ++counters.failed;
        uint64_t required_bytes_min = 0;
        unsigned unknown_tiles = 0;
        for (size_t id : active) {
          const auto& required = entries[id];
          if (required.required < now) continue;
          if (required.resident) { required_bytes_min += required.bytes; continue; }
          const auto prepared = std::find_if(jobs.begin(), jobs.end(), [id](const Job& candidate) {
            return candidate.id == id && candidate.preparation && !candidate.task;
          });
          if (prepared != jobs.end()) required_bytes_min += prepared->preparation->bytes;
          else ++unknown_tiles;
        }
        std::fprintf(stderr, "server_collision_tile failed=%zu reason=protected_budget resident_bytes=%llu tile_bytes=%llu budget_bytes=%llu resident_tiles=%u required_bytes_min=%llu required_unprepared_tiles=%u setting=OCTARYN_SERVER_COLLISION_BUDGET_MIB range=64..4096\n",
            job.id, static_cast<unsigned long long>(counters.resident_bytes), static_cast<unsigned long long>(bytes),
            static_cast<unsigned long long>(counters.budget_bytes), counters.resident,
            static_cast<unsigned long long>(required_bytes_min), unknown_tiles);
      } else if (!scene.set_tile(job.id, std::move(job.preparation->tile))) {
        entry.failed = true;
        ++counters.failed;
        std::fprintf(stderr, "server_collision_tile failed=%zu reason=publication\n", job.id);
      } else {
        entry.resident = true;
        entry.bytes = bytes;
        entry.triangles = job.preparation->triangles;
        counters.resident_bytes += bytes;
        ++counters.resident;
        ++counters.loads;
      }
      if (!entry.resident) counters.reserved_bytes -= bytes;
      --counters.preparing;
      job.preparation.reset();
    }
    std::erase_if(active, [&](size_t id) {
      auto& entry = entries[id];
      const bool preparing = std::any_of(jobs.begin(), jobs.end(),
          [id](const Job& job) { return job.preparation && job.id == id; });
      if (entry.resident || preparing || entry.wanted >= now) return false;
      entry.queued = false;
      return true;
    });
    for (auto& job : jobs) {
      if (job.preparation || counters.failed) continue;
      auto eligible = [&](size_t id, bool required_only) {
        const auto& entry = entries[id];
        return !entry.resident && !entry.failed && entry.wanted >= now &&
            (!required_only || entry.required >= now) &&
            std::none_of(jobs.begin(), jobs.end(), [id](const Job& other) { return other.preparation && other.id == id; });
      };
      auto found = std::find_if(active.begin(), active.end(), [&](size_t id) { return eligible(id, true); });
      if (found == active.end() && !pressure)
        found = std::find_if(active.begin(), active.end(), [&](size_t id) { return eligible(id, false); });
      if (found == active.end()) continue;
      job.id = *found;
      job.budget_wait = {};
      job.preparation = std::make_unique<Preparation>();
      job.preparation->path = directory / std::filesystem::u8path(paths[job.id]);
      job.preparation->bounds = bounds[job.id];
      const octaryn_native_schedule_runtime_job description{
          "authority_collision_prepare", nullptr, 0, nullptr, 0, 0, Preparation::execute, job.preparation.get()};
      job.task = octaryn_native_schedule_runtime_submit_worker(workers, &description, 1);
      if (!job.task) { entries[job.id].failed = true; ++counters.failed; job.preparation.reset(); continue; }
      counters.reserved_bytes += JobReservation;
      ++counters.preparing;
    }
  }
};

CollisionResidency::CollisionResidency(const MapManifest& manifest,const std::filesystem::path& directory,const std::filesystem::path& source) {
  if(!manifest.scene_catalog.empty()) {
    scene_=std::make_unique<character_motion::SceneCollisionResidency>();
    if(!scene_->load(manifest.scene_catalog,source,manifest.scene_catalog.parent_path()/"collision-scratch",configured_budget()))
      throw std::runtime_error(scene_->error());
  } else state_=std::make_unique<State>(manifest,directory);
}
CollisionResidency::~CollisionResidency() = default;

bool CollisionResidency::ready(float x, float y, float z, float radius) {
  if(scene_)return scene_->ready(x,y,z,radius);
  if (!std::isfinite(x) || !std::isfinite(z) || !std::isfinite(radius) || radius < 0 || radius > 4096) return false;
  auto& state = *state_;
  const auto now = Clock::now();
  state.required.clear();
  struct Query { State* state; Clock::time_point until; bool required; } query{&state, now + KeepTime, false};
  auto callback = [](int, uint64_t id, void* pointer) {
    auto& q = *static_cast<Query*>(pointer);
    auto& entry = q.state->entries[id];
    entry.wanted = q.until;
    if (!entry.queued) { entry.queued = true; q.state->active.push_back(id); }
    if (q.required) { entry.required = q.until; q.state->required.push_back(id); }
    return true;
  };
  const float keep = radius + 16;
  b3DynamicTree_Query(&state.tree, {{x - keep, -1e6f, z - keep}, {x + keep, 1e6f, z + keep}}, 1, false, callback, &query);
  query.required = true;
  b3DynamicTree_Query(&state.tree, {{x - radius, -1e6f, z - radius}, {x + radius, 1e6f, z + radius}}, 1, false, callback, &query);
  state.pump(now);
  const bool ready = !state.counters.failed && std::all_of(state.required.begin(), state.required.end(),
      [&](size_t id) { return state.entries[id].resident; });
  if (!ready) ++state.counters.waits;
  return ready;
}
character_motion::MeshCollision CollisionResidency::collision() { return scene_?scene_->scene()->view():state_->scene.view(); }
bool CollisionResidency::ready_bounds(const std::array<float,6>& bounds) {
  if(scene_)return scene_->ready_bounds(bounds);
  return ready((bounds[0]+bounds[3])*.5f,(bounds[1]+bounds[4])*.5f,(bounds[2]+bounds[5])*.5f,
      std::hypot(bounds[3]-bounds[0],bounds[5]-bounds[2])*.5f);
}
CollisionResidencyStats CollisionResidency::stats() const {
  if(!scene_)return state_->counters;
  const auto s=scene_->stats();
  return {2,s.resident,s.preparing,s.failed,s.reserved_bytes,s.loads,s.evictions,0,s.waits,s.resident_bytes,s.budget_bytes};
}
uint64_t CollisionResidency::triangle_count() const {
  if(scene_)return scene_->triangle_count();
  uint64_t result = 0;
  for (size_t id : state_->active) if (state_->entries[id].resident) result += state_->entries[id].triangles;
  return result;
}
}
