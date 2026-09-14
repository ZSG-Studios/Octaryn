#include "ColumnBlocks.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
using octaryn::client::world_presentation::ColumnBlocks;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void same(const ColumnBlocks& blocks, const std::vector<std::uint16_t>& expected) {
  require(blocks.size() == expected.size(), "compact column size mismatch");
  require(std::equal(blocks.begin(), blocks.end(), expected.begin()), "compact indexed/iterator roundtrip mismatch");
  require(blocks.expand() == expected, "compact explicit expansion mismatch");
  const auto* identity=blocks.storage_identity();
  const auto bytes=blocks.storage_bytes();
  std::vector<std::uint32_t> actual(expected.size());
  blocks.read_range(0,actual);
  require(std::equal(actual.begin(),actual.end(),expected.begin()),"bulk range decode differs from exact original IDs");
  for(std::size_t first=0;first<expected.size();first+=31) {
    const auto count=std::min<std::size_t>(47,expected.size()-first);
    std::vector<std::uint32_t> window(count+2,0xdeadbeef);
    blocks.read_range(first,std::span<std::uint32_t>(window).subspan(1,count));
    require(window.front()==0xdeadbeef && window.back()==0xdeadbeef &&
        std::equal(window.begin()+1,window.end()-1,expected.begin()+static_cast<std::ptrdiff_t>(first)),
        "unaligned bulk range/page crossing writes outside span or changes IDs");
  }
  blocks.read_range(blocks.size(),{});
  require(blocks.storage_identity()==identity && blocks.storage_bytes()==bytes,
      "bulk reads must preserve shared compact identity and allocation size");
}
}

void check_column_blocks() {
  ColumnBlocks empty;
  empty.read_range(0,{});
  empty.compact();
  require(empty.empty() && empty.expand().empty() && empty.storage_bytes() == 0,
      "empty column must not allocate compact storage");
  unsigned cases = 0;
  for (const unsigned palette_size : {1u, 2u, 3u, 16u, 17u, 256u, 257u}) {
    std::vector<std::uint16_t> expected(4096 + 37);
    ColumnBlocks blocks; blocks.resize(expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
      expected[i] = static_cast<std::uint16_t>((i % palette_size) * 257u + 31u);
      blocks[i] = expected[i];
    }
    blocks.compact();
    require(blocks.is_compact(), "worker compact must publish packed representation");
    same(blocks, expected);
    if (palette_size <= 256)
      require(blocks.storage_bytes() < expected.size() * sizeof(std::uint16_t), "paletted pages must reduce retained bytes");
    auto copy = blocks;
    const auto* identity = blocks.storage_identity();
    const auto bytes = blocks.storage_bytes();
    require(copy.storage_identity() == identity, "snapshot copy must share exact payload");
    for (std::size_t i = 0; i < expected.size(); ++i)
      require(static_cast<std::uint16_t>(copy[i]) == expected[i], "mutable-object read must stay lossless");
    require(copy.storage_identity() == identity && copy.storage_bytes() == bytes && copy.is_compact(),
        "read-only access must never expand or detach shared compact data");
    copy[4096] = 0; // Authoritative edited air at the next page boundary.
    require(copy.storage_identity() != identity && !copy.is_compact(), "mutation must detach dense working copy");
    require(blocks[4096] == expected[4096], "COW mutation changed retained query snapshot");
    auto edited = expected; edited[4096] = 0;
    same(copy, edited);
    copy.compact(); same(copy, edited); same(blocks, expected);
    const auto* packed = copy.storage_identity(); copy.compact();
    require(copy.storage_identity() == packed, "repeated compact must preserve shared identity");
    auto filled = copy; filled.fill(65535); filled.compact();
    require(std::all_of(filled.begin(), filled.end(), [](auto value) { return value == 65535; }),
        "fill/COW lost maximum uint16 block ID");
    same(copy, edited);
    auto resized = copy; resized.resize(edited.size() + 2);
    require(resized[edited.size()] == 0 && resized[edited.size() + 1] == 0,
        "expanded mutation must retain vector zero-fill semantics");
    resized.pop_back(); resized.pop_back(); require(resized == copy, "resize/pop changed original bytes");
    ++cases;
  }
  // A bijection over all uint16 IDs forces raw16 pages, including air and65535.
  std::vector<std::uint16_t> entropy(65536 + 19);
  ColumnBlocks raw; raw.resize(entropy.size());
  for (std::size_t i = 0; i < entropy.size(); ++i)
    raw[i] = entropy[i] = static_cast<std::uint16_t>(i * 40503u + 17u);
  raw.compact(); same(raw, entropy);
  require(raw.storage_bytes() <= entropy.size() * 2 + 4096,
      "raw fallback must remain bounded near original dense size");
  auto shared = raw; raw.clear(); same(shared, entropy);
  require(raw.empty(), "clearing one owner must not clear its retained snapshot");
  ColumnBlocks assigned{1, 2, 3}; assigned = {65535, 0}; assigned.compact();
  same(assigned, {65535, 0});
  std::printf("column_blocks_probe=passed palette_cases=%u full_id_roundtrip=65536 shared_reads=no_expansion cow=isolated edited_air=preserved\n", cases);
}
