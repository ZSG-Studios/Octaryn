#include "StreamSnapshot.h"
#include "StreamGenerationOrder.h"
#include "PreloadedHalo.h"
#include "TerrainGeneration.h"
#include "TerrainDensity.h"

#include <array>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <thread>
#include <tuple>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace octaryn::client::world_presentation;
namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
template <typename T> void write(std::ofstream& out, T value) {
  out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
void snapshot_file(const std::filesystem::path& path, std::uint64_t seed = 1337,
                   std::uint32_t mode = 0, std::uint32_t revision = 3,
                   std::uint32_t schema = 3, std::uint16_t top_block = 5) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write("OCSTRM01", 8);
  write(out, schema); write(out, std::uint64_t{42});
  if (schema == 3) write(out, std::uint64_t{});
  write(out, -1); write(out, -1); write(out, 2u); write(out, seed);
  if (schema >= 2) { write(out, mode); write(out, revision); }
  write(out, std::uint64_t{}); write(out, 0u); write(out, 0.0); write(out, 0.0f);
  for (int i = 0; i < 8; ++i) write(out, 0.0f);
  write(out, 0u); write(out, 1u); write(out, 1u); write(out, 2u);
  write(out, -1); write(out, -1); write(out, -32); write(out, -32);
  write(out, 0u); write(out, 2u);
  write(out, -32); write(out, -256); write(out, -32); write(out, std::uint16_t{0});
  write(out, -1); write(out, 255); write(out, -1); write(out, top_block);
  require(static_cast<bool>(out), "write snapshot");
}
std::size_t index(int x, int y, int z) {
  return static_cast<std::size_t>(x + 32 * (y + 256 + 512 * z));
}
std::uint64_t voxel_hash(const StreamColumn& column) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const auto block : column.blocks) {
    hash ^= block;
    hash *= 1099511628211ull;
  }
  return hash;
}
std::uint64_t validate_generation() {
  const OctarynServerTerrainMaterialRules rules{30, 14, 3, 1, 2, 5, 4};
  constexpr std::array<std::pair<int, int>, 10> coordinates{{
      {0, 0}, {-1, -1}, {1, 0}, {0, 1}, {-1, 0}, {4, -3},
      {-31, 27}, {99, 105}, {-1024, 1024}, {-1000000, 1000000}}};
  std::array<std::uint64_t, coordinates.size()> hashes{};
  std::uint64_t checked = 0;
  std::array<std::uint64_t, 15> vegetation{};
  unsigned seam_leaves = 0;
  bool removal_verified = false;
  std::size_t retained_bytes = 0, maximum_retained_bytes = 0;
  double elapsed_ms = 0.0, maximum_ms = 0.0;
  const auto generate = [&](int cx, int cz) {
    const auto start = std::chrono::steady_clock::now();
    auto column = generate_stream_column({cx, cz, 7, {}}, 42);
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    elapsed_ms += ms;
    maximum_ms = std::max(maximum_ms, ms);
    return column;
  };
  for (std::size_t i = 0; i < coordinates.size(); ++i) {
    const auto [cx, cz] = coordinates[i];
    const auto column = generate(cx, cz);
    require(column.blocks.size() == 32 * 512 * 32, "full depth voxel count");
    require(column.blocks.is_compact(), "generated columns must publish compact storage");
    const auto bytes = column.blocks.storage_bytes();
    require(bytes < column.blocks.size(), "representative terrain must use less than half dense uint16 storage");
    retained_bytes += bytes; maximum_retained_bytes = std::max(maximum_retained_bytes, bytes);
    auto shared_column = column;
    require(shared_column.blocks.storage_identity() == column.blocks.storage_identity(),
        "query/renderer column copy must share generated payload");
    require(column.x == cx && column.z == cz && column.epoch == 42 && column.revision == 7,
        "generated column identity");
    hashes[i] = voxel_hash(column);
    const auto compare = [&](int x, int y, int z) {
      require(y >= -256 && y < 256, "sample in client volume");
      std::uint16_t expected{};
      require(octaryn_server_terrain_generated_block(cx * 32 + x, y, cz * 32 + z, &rules, &expected) == 0,
          "server generated block");
      if (column.blocks[index(x, y, z)] != expected) {
        std::cerr << "parity mismatch world=" << cx * 32 + x << ',' << y << ',' << cz * 32 + z << '\n';
        require(false, "authority/client reconstruction parity");
      }
      ++checked;
    };
    for (const auto z : {0, 1, 15, 30, 31}) for (const auto x : {0, 1, 15, 30, 31}) {
      OctarynServerTerrainColumnPlan plan{};
      require(octaryn_server_terrain_plan_column(cx * 32 + x, cz * 32 + z, &rules, &plan) == 0, "server column plan");
      for (const auto y : {-256, -253, -252, -192, -64, -1, 0, 29, 30,
               plan.terrain_height - 9, plan.terrain_height - 8, plan.terrain_height - 4,
               plan.terrain_height - 1, plan.terrain_height, plan.terrain_height + 1, 255}) {
        compare(x, y, z);
      }
    }
    // Full vertical seams include both sides of positive and negative chunk borders.
    for (int z = 0; z < 32; ++z) for (int x = 0; x < 32; ++x)
      if (x == 0 || x == 31 || z == 0 || z == 31)
        for (int y = -256; y < 256; ++y) compare(x, y, z);
    for (int z = 0; z < 32; ++z) for (int x = 0; x < 32; ++x) {
      OctarynServerTerrainColumnPlan plan{};
      require(octaryn_server_terrain_plan_column(cx * 32 + x, cz * 32 + z, &rules, &plan) == 0,
          "vegetation surface plan");
      for (int y = plan.terrain_height + 1; y <= plan.terrain_height + 6; ++y) {
        compare(x, y, z);
        const auto block = column.blocks[index(x, y, z)];
        if (block < vegetation.size()) ++vegetation[block];
        seam_leaves += block == 7 && (x == 0 || x == 31 || z == 0 || z == 31);
        if (block == 6 || block == 7 || (block >= 9 && block <= 13)) {
          using namespace octaryn::basegame::terrain;
          require(sample_block(sample_column(cx * 32 + x, cz * 32 + z), y, rules) == AirBlock,
              "natural vegetation only occupies terrain air");
          if (block == 6 && !removal_verified) {
            SnapshotColumn edited_source{cx, cz, 8, {{cx * 32 + x, y, cz * 32 + z, 0}}};
            const auto removed = generate_stream_column(edited_source, 43);
            require(removed.blocks[index(x, y, z)] == 0, "authoritative removed tree stays air");
            edited_source.edits.front().block = 5;
            const auto replaced = generate_stream_column(edited_source, 44);
            require(replaced.blocks[index(x, y, z)] == 5, "authoritative placement replaces generated tree");
            removal_verified = true;
          }
        }
      }
    }
    using namespace octaryn::basegame::terrain;
    for (int z = 0; z < 32; ++z) for (int x = 0; x < 32; ++x) {
      const auto terrain = sample_column(cx * 32 + x, cz * 32 + z);
      const CaveColumnSampler caves(terrain); const auto fill = classify_materials(terrain, rules);
      for (int y = -256; y < 256; ++y) {
        const auto base = sample_block_cached(caves, y, rules, fill);
        require(base == AirBlock || base == column.blocks[index(x, y, z)],
            "vegetation must preserve every terrain and water cell");
      }
    }
  }
  require(vegetation[6] > 0 && vegetation[7] > 0 && vegetation[9] > 0 && seam_leaves > 0,
      "natural world must contain trunks bushes and canopy across signed chunk boundaries");
  require(removal_verified, "tree edit precedence exercised");
  for (unsigned flower = 10; flower <= 13; ++flower)
    require(vegetation[flower] > 0, "all registered flower species must generate");
  std::cout << "natural_vegetation=passed logs=" << vegetation[6] << " leaves=" << vegetation[7]
      << " bushes=" << vegetation[9] << " seam_leaves=" << seam_leaves << " revision=3 full_chunk_faces=passed\n";
  std::array<std::size_t, coordinates.size()> order{};
  std::iota(order.begin(), order.end(), std::size_t{});
  std::mt19937 random(5719);
  std::shuffle(order.begin(), order.end(), random);
  for (const auto i : order) {
    const auto [cx, cz] = coordinates[i];
    require(voxel_hash(generate(cx, cz)) == hashes[i], "shuffled whole-column deterministic generation");
  }
  std::cout << "terrain_column_generation columns=" << coordinates.size() * 2
      << " mean_ms=" << elapsed_ms / static_cast<double>(coordinates.size() * 2)
      << " max_ms=" << maximum_ms << " order=passed\n";
  std::cout << "terrain_column_storage columns=" << coordinates.size()
      << " mean_bytes=" << retained_bytes / coordinates.size() << " max_bytes=" << maximum_retained_bytes
      << " dense_bytes=1048576 sharing=passed\n";
  return checked;
}
void validate_delivery_queries(const std::filesystem::path& path) {
  WorldStream stream(path);
  stream.request(-1, -1, 2);
  // Withhold delivery while the real worker parses/generates. Camera/target
  // queries must stay on the last delivered data, including edited air.
  const auto hold = [&](bool available, std::uint16_t expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    do {
      std::uint16_t block{};
      const bool found = stream.try_block(-1, 255, -1, block);
      require(found == available && (!found || block == expected),
          "query changed before corresponding renderer delivery");
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } while (std::chrono::steady_clock::now() < deadline);
  };
  const auto deliver = [&](std::uint16_t expected) {
    StreamColumn result;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    bool received = false;
    while (std::chrono::steady_clock::now() < deadline && !(received = stream.poll(result)))
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    require(received && result.x == -1 && result.z == -1 &&
        result.blocks[index(0, -256, 0)] == 0 && result.blocks[index(31, 255, 31)] == expected,
        "async authoritative delivery");
    std::uint16_t block{};
    require(stream.try_block(-1, 255, -1, block) && block == expected,
        "query must match delivered column immediately");
  };
  hold(false, 0);
  deliver(5);
  snapshot_file(path, 1337, 0, 3, 3, 0);
  hold(true, 5);
  deliver(0);
  snapshot_file(path);
  hold(true, 0);
  deliver(5);
  // No worker maintenance is required between these two frame requests.
  stream.request(100, 100, 2);
  std::uint16_t block{};
  require(!stream.try_block(-1, 255, -1, block), "retired column query must be unavailable");
  stream.request(-1, -1, 2);
  hold(false, 0);
  deliver(5);
}
using Coordinate=std::pair<int,int>;
constexpr auto delivery_coordinates=[] {
  std::array<Coordinate,25> result{};std::size_t at{};
  for(int z=2;z>=-2;--z)for(int x=2;x>=-2;--x)result[at++]={x,z};
  return result;
}();
static_assert(delivery_coordinates.size()>StreamReadyCapacity+StreamGenerationWorkers);
void delivery_snapshot(const std::filesystem::path& path,std::uint64_t authority,bool edit_center=false) {
  std::ofstream out(path,std::ios::binary|std::ios::trunc);
  out.write("OCSTRM01",8);
  write(out,3u);write(out,std::uint64_t{42});write(out,authority);
  write(out,0);write(out,0);write(out,2u);write(out,std::uint64_t{1337});
  write(out,0u);write(out,3u);write(out,std::uint64_t{});write(out,0u);write(out,0.0);write(out,0.0f);
  for(int i=0;i<8;++i)write(out,0.0f);
  write(out,0u);write(out,1u);
  write(out,static_cast<std::uint32_t>(delivery_coordinates.size()));
  write(out,static_cast<std::uint32_t>(delivery_coordinates.size()));
  std::uint32_t offset{};
  for(const auto [x,z]:delivery_coordinates) {
    write(out,x);write(out,z);write(out,x*32);write(out,z*32);write(out,offset++);write(out,1u);
  }
  for(const auto [x,z]:delivery_coordinates) {
    write(out,x*32);write(out,255);write(out,z*32);
    write(out,std::uint16_t(edit_center && x==0 && z==0?4:5));
  }
  require(bool(out),"write bounded delivery snapshot");
}
void validate_generation_resets() {
  StreamResidency state;state.change_window(0,0,1);
  StreamSnapshot snapshot{42,1337,{{0,0,1,{}},{1,0,1,{}},{0,1,1,{}}}};
  StreamGenerationOrder order;order.reset(snapshot,0,0,1);
  const auto* first=order.next(state);require(first && first->x==0 && first->z==0,"generation reset fixture center");
  order.consumed();
  const auto* second=order.next(state);require(second && second->x==1,"generation reset fixture neighbor");
  // An old task can finish after a current task has already dispatched. Reset
  // the actual selector while that current task still lacks completion metadata.
  const std::array<const SnapshotColumn*,2> pending{first,second};
  order.reset(snapshot,0,0,1);
  const auto* next=order.next(state,pending);
  require(next && next->x==0 && next->z==1,"reset dispatched an already-running current generation again");
  auto changed=*first;++changed.revision;
  const std::array<const SnapshotColumn*,1> old_content{&changed};
  order.reset(snapshot,0,0,1);
  require(order.next(state,old_content)==first,"stale content task suppressed current generation");
  changed=*first;++changed.authoritative_revision;
  order.reset(snapshot,0,0,1);
  require(order.next(state,old_content)==first,"stale authority task suppressed current publication");
  order.reset(snapshot,0,0,1);
  require(order.next(state)==first,"retired window task suppressed regenerated column");
  for(std::uint64_t revision=1;revision<=2;++revision) {
    StreamColumn source;source.revision=revision;source.authoritative_revision=revision;
    auto query=std::make_shared<const StreamColumn>(source);
    require(state.retain(std::move(source),std::move(query)),"queued reversal fixture payload");
  }
  state.change_window(100,100,1);state.change_window(0,0,1);
  require(!state.needs_publication(0,0,2,2),"rapid reversal forgot newest queued completion");
  require(!state.query(0,0),"restored queued completion became prematurely queryable");
  StreamColumn delivered;
  require(state.deliver(delivered) && delivered.revision==1,"reversal changed oldest-first publication");
  require(state.deliver(delivered) && delivered.revision==2 && !state.needs_publication(0,0,2,2),
      "consumed queued payload was regenerated after reversal");
}
void validate_bounded_delivery(const std::filesystem::path& path) {
  std::filesystem::remove(path);
  WorldStream stream(path);stream.request(0,0,2);
  delivery_snapshot(path,1);
  auto expected=delivery_coordinates;
  std::stable_sort(expected.begin(),expected.end(),[](Coordinate a,Coordinate b) {
    const auto priority=[](Coordinate value) {
      return std::tuple{std::max(std::abs(value.first),std::abs(value.second)),
          std::abs(value.first)+std::abs(value.second)};
    };
    return priority(a)<priority(b);
  });
  const auto peek=[&](std::span<const StreamColumn* const> excluded={}) {
    StreamColumn result;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!stream.peek(result,excluded)) {
      require(std::chrono::steady_clock::now()<deadline,"bounded delivery timed out");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return result;
  };
  const auto hold_mailbox=[&](bool verify_overlap=false) {
    std::array<StreamColumn,StreamReadyCapacity> staged;
    std::array<const StreamColumn*,StreamReadyCapacity> excluded{};
    for(std::size_t i=0;i<staged.size();++i) {
      staged[i]=peek({excluded.data(),i});excluded[i]=&staged[i];
      require(Coordinate(staged[i].x,staged[i].z)==expected[i],
          "worker preserves center-out order throughout the GPU staging mailbox");
      if(verify_overlap)validate_private_halo(stream,staged[i]);
    }
    // Simulate a stalled renderer: the full mailbox plus CPU tasks may finish,
    // but no additional terrain or query-visible column may escape those bounds.
    constexpr auto maximum=StreamReadyCapacity+StreamGenerationWorkers;
    if(verify_overlap) {
      const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
      while(stream.generated_columns()<maximum) {
        require(std::chrono::steady_clock::now()<deadline,"CPU generation stalled behind occupied GPU delivery slots");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if(verify_overlap)require(stream.generated_columns()==maximum,"generation exceeded ready plus pending payload bounds");
    StreamColumn excess;
    require(!stream.peek(excess,std::span<const StreamColumn* const>(excluded)),"ready mailbox exceeded its fixed capacity");
    for(const auto [x,z]:expected) {
      std::uint16_t block{};
      require(!stream.try_block(x*32,255,z*32,block),"generated-ahead payload became queryable before publication");
    }
  };
  hold_mailbox(true);
  std::array<StreamColumn,delivery_coordinates.size()> previous;
  const auto drain=[&](std::uint64_t authority,bool edit_center,bool cached) {
    for(std::size_t i=0;i<expected.size();++i) {
      const auto column=peek();
      require(Coordinate(column.x,column.z)==expected[i],"generated and cached delivery order changed");
      require(column.authoritative_revision==authority,"stale snapshot generation escaped publication");
      if(cached && (!edit_center || i!=0))
        require(column.blocks.storage_identity()==previous[i].blocks.storage_identity(),"metadata update regenerated cached terrain");
      const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
      auto publication=StreamPublication::Busy;
      while((publication=stream.publish(column))==StreamPublication::Busy) {
        require(std::chrono::steady_clock::now()<deadline,"bounded query retirement did not progress");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      require(publication==StreamPublication::Published,"staged column retired unexpectedly");
      std::uint16_t block{};
      require(stream.try_block(column.x*32,255,column.z*32,block) && block==(edit_center && i==0?4:5),
          "published query lost its exact generated payload");
      previous[i]=column;
    }
    StreamColumn duplicate;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    require(!stream.peek(duplicate),"bounded generation delivered a duplicate column");
  };
  drain(1,false,false);
  require(stream.generated_columns()==delivery_coordinates.size(),"initial window performed duplicate generation");
  delivery_snapshot(path,2);drain(2,false,true);
  require(stream.generated_columns()==delivery_coordinates.size(),"cached metadata regenerated terrain");
  delivery_snapshot(path,3,true);drain(3,true,true);
  require(stream.generated_columns()==delivery_coordinates.size()+1,"mixed cached and edited window regenerated unchanged terrain");
  // Retire a full mailbox and generated-ahead futures together, then reenter.
  stream.request(100,100,2);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  stream.request(0,0,2);
  hold_mailbox();
  // Reverse synchronously while the ready mailbox and generated futures are
  // retained. No worker-maintenance delay may be required for correct selection.
  stream.request(100,100,2);stream.request(0,0,2);
  drain(3,true,false);
  stream.request(100,100,2);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  StreamColumn retired;
  require(!stream.peek(retired),"retired generated-ahead payload remained deliverable");
  stream.request(0,0,2);drain(3,true,false);
  std::cout << "bounded_stream_delivery=passed mailbox="<<StreamReadyCapacity<<" generation_ahead="<<StreamGenerationWorkers
      <<" ordered=passed cached=passed stale=passed\n";
}
}

int main() {
  const auto path = std::filesystem::temp_directory_path() /
      ("octaryn-world-stream-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin");
  try {
    const auto checked = validate_generation();
    validate_preloaded_halo();
    snapshot_file(path);
    StreamSnapshot snapshot;
    std::string error;
    require(read_stream_snapshot(path, snapshot, error), "read authoritative binary");
#if defined(_WIN32)
    // A publisher may already hold delete access while a consumer opens its reader.
    const auto publisher = CreateFileW(path.c_str(), DELETE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    require(publisher != INVALID_HANDLE_VALUE, "open replacement publisher");
    const bool shared_read = read_stream_snapshot(path, snapshot, error);
    CloseHandle(publisher);
    require(shared_read, "snapshot reader shares publisher delete access");
#endif
    require(snapshot.epoch == 42 && snapshot.columns.size() == 1, "snapshot metadata");
    const auto edited = generate_stream_column(snapshot.columns.front(), snapshot.epoch);
    require(edited.blocks[index(0, -256, 0)] == 0 && edited.blocks[index(31, 255, 31)] == 5, "air and top boundary overrides");
    const auto revision = snapshot.columns.front().revision;
    require(snapshot.columns.front().generator_revision == 3, "only current vegetation generator is reconstructed");
    for (const auto unsupported : {0u, 1u, 2u, 4u}) {
      snapshot_file(path, 1337, 0, unsupported);
      require(!read_stream_snapshot(path, snapshot, error) && snapshot.columns.front().generator_revision == 3 &&
          snapshot.columns.front().revision == revision, "reject unsupported revision without mutating valid snapshot");
      auto invalid = snapshot.columns.front(); invalid.generator_revision = unsupported;
      bool rejected = false;
      try { (void)generate_stream_column(invalid, 42); } catch (const std::invalid_argument&) { rejected = true; }
      require(rejected, "direct column generation rejects unsupported revision");
    }
    snapshot_file(path, 99);
    require(!read_stream_snapshot(path, snapshot, error) && snapshot.columns.front().revision == revision, "reject unsupported seed without changing valid snapshot");
    for (const auto mode : {1u, 2u, 3u}) {
      snapshot_file(path, 1337, mode);
      require(!read_stream_snapshot(path, snapshot, error) && snapshot.epoch == 42 && snapshot.columns.front().revision == revision,
          "reject unsupported generator mode atomically");
    }
    snapshot_file(path, 1337, 0, 1);
    require(!read_stream_snapshot(path, snapshot, error) && snapshot.epoch == 42, "reject generator revision mismatch atomically");
    snapshot_file(path, 1337, 0, 3, 1);
    require(!read_stream_snapshot(path, snapshot, error) && snapshot.epoch == 42, "reject unversioned terrain identity atomically");
    snapshot_file(path);
    std::filesystem::resize_file(path, 115);
    require(!read_stream_snapshot(path, snapshot, error) && snapshot.epoch == 42, "reject truncated snapshot atomically");
    snapshot_file(path);
    validate_delivery_queries(path);
    validate_generation_resets();
    delivery_snapshot(path,1);
    validate_stream_request_activation(path);
    validate_bounded_delivery(path);
    std::filesystem::remove(path);
    std::cout << "client_world_stream_probe=passed parity_samples=" << checked << " parser=passed edits=passed async=passed query_delivery=passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::cerr << "client_world_stream_probe=failed " << error.what() << '\n';
    return 1;
  }
}
