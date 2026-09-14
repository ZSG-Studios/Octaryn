#include "BlockStore.h"

#include <cstdio>
#include <initializer_list>
#include <memory>
#include <vector>

namespace {
using Edit = octaryn_server_block_edit;

bool same(const Edit &left, const Edit &right) {
  return left.position.x == right.position.x &&
         left.position.y == right.position.y &&
         left.position.z == right.position.z && left.block == right.block;
}

bool check(void *store, const char *label, std::initializer_list<Edit> expected) {
  const auto count = octaryn_server_block_store_snapshot_count(store);
  std::vector<Edit> filled(expected.size());
  const auto written = octaryn_server_block_store_snapshot_fill(
      store, filled.data(), static_cast<uint64_t>(filled.size()));
  bool ok = count == expected.size() && written == expected.size();
  size_t index = 0;
  for (const auto &edit : expected) ok &= same(filled[index++], edit);
  if (!ok) std::fprintf(stderr, "snapshot_count_abi: %s count/fill mismatch\n", label);
  return ok;
}
} // namespace

bool validate_snapshot_count_abi() {
  using Store = std::unique_ptr<void, decltype(&octaryn_server_block_store_destroy)>;
  Store store(octaryn_server_block_store_create(), &octaryn_server_block_store_destroy);
  if (!store) return false;
  bool ok = check(nullptr, "null", {}) && check(store.get(), "empty", {});
  const Edit negative{{-33, -1, -32}, 7};
  const Edit air{{0, 0, 0}, 0};
  const Edit positive{{33, 32, 1}, 9};
  octaryn_server_block_store_set_block(store.get(), &positive, 0);
  octaryn_server_block_store_set_block(store.get(), &negative, 0);
  ok &= check(store.get(), "signed chunks", {negative, positive});

  octaryn_server_block_store_set_block(store.get(), &air, 1);
  ok &= check(store.get(), "preserved air counts", {negative, air, positive});
  octaryn_server_block_store_set_block(store.get(), &air, 1);
  octaryn_server_block_store_set_block(store.get(), &negative, 0);
  ok &= check(store.get(), "unchanged edits", {negative, air, positive});

  const Edit replacement{negative.position, 11};
  octaryn_server_block_store_set_block(store.get(), &replacement, 0);
  ok &= check(store.get(), "replacement", {replacement, air, positive});
  const Edit removed{positive.position, 0};
  octaryn_server_block_store_set_block(store.get(), &removed, 0);
  ok &= check(store.get(), "non-preserved air removes", {replacement, air});
  octaryn_server_block_store_clear_block_override(store.get(), &air.position);
  ok &= check(store.get(), "clear preserved air", {replacement});

  const Edit loaded[]{positive, negative, air, replacement};
  octaryn_server_block_store_load(store.get(), loaded, 4);
  ok &= check(store.get(), "load with duplicate and air", {replacement, air, positive});
  octaryn_server_block_store_load(store.get(), nullptr, 0);
  ok &= check(store.get(), "load clears", {});
  if (ok) std::puts("snapshot_count_abi=passed signed=passed air=passed replacement=passed load=passed");
  return ok;
}
