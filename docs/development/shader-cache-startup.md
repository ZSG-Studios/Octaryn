# Engine shader-cache startup comparison

`capture_shader_cache_pairs.py` runs three serialized pairs of fresh client
processes against identical executable, shader, authority, map, cooked asset,
driver and settings identities. Each pair receives its own new cache directory.
The first run starts with an empty engine-owned shader/pipeline cache; the
second reuses that directory. It never deletes or changes the user's normal
cache. OS, driver and filesystem cache states remain unknown in both runs.

Preview the six commands without starting GPU work:

```powershell
python tools/validation/capture_shader_cache_pairs.py --client-bundle-root build/release-windows/client/bundle --evidence-root logs/client/shader-cache-startup --backend dx12 --dry-run
```

Once the GPU is available, omit `--dry-run`. Use a separate invocation for
Vulkan or `--mode 1`/`--mode 2`; output defaults to native 2560x1440 and can be
specified with `--width`/`--height`. `--manifest` selects an existing adjacent
map variant without cooking it. Every run uses the same direct full-detail RT
reference workload, hidden and bounded to 240 ready frames, without image
readbacks. These are startup diagnostics, not engine FPS measurements.

Prelaunch directory inventories and SHA256 content identities are saved. The
runner refuses a contaminated first directory or any changed cache bytes
between the first launch and reuse. Capture-side initial file counts/bytes and
explicit-cache activation must agree. Both shader and pipeline caches must
report misses/writes on the empty run and hits on reuse. Reuse may still incur
misses or eviction; those counters are reported rather than called fully warm.
Corruption, lock contention, I/O errors or missing summaries invalidate a pair.

`report.json` preserves all six raw stage/readiness records, actual cache
counters, cache inventories and run identities. `report.md` shows three-run
medians and paired differences. Stage timings use the existing loading parser;
SDL uptime and session readiness retain separate clock origins. Overlapping
intervals are not added together. Three-sample p95/p99 are descriptive only.
Hashing cache files between runs is outside measured startup and can affect
filesystem cache warmth, reinforcing that only engine-cache state is controlled.

Rebuilds, recooking and concurrent GPU work must stay outside the six-run window.
Retain generated cache directories with the evidence; the runner never clears
them and never labels the first launch OS-cold or driver-cold.

## Build17 DX12 three-pair result

All six native2560x1440 direct/reference runs passed:
`logs/client/build17-asset-qualification/shader-cache/shader-cache-pairs-93aglzu2`.
The report was independently recomputed from saved cases and matched exactly.
All six build/asset/driver/settings identities agree. Each of the three isolated
directories started with zero files/bytes; its first run produced86 files totaling
567,412 bytes. The paired reused directory had exactly the same content inventory.
Every empty run reported46 shader and38 pipeline misses/writes, with zero hits;
every reused run reported46/38 hits and zero misses/writes. Rejection, lock and
I/O error counters were zero throughout.

| Startup record | Engine cache empty median ms | Reused median ms |
|---|---:|---:|
| Renderer-ready elapsed |2593.0|819.0|
| Client boot elapsed |2592.0|818.0|
| Map pipeline creation |930.53|110.85|
| Temporal setup |605.2|94.4|
| Map renderer total |3935.2|3109.5|
| Map worker elapsed |6277.0|4947.0|
| Map image stage |1736.25|1727.41|
| Collision preparation |1726.8|1735.2|
| Authoritative ready, SDL uptime |17457.357|14308.668|
| Authoritative ready, session elapsed |8346.576|8328.227|

Paired SDL-uptime reductions were3215.814,3196.833 and2898.133ms. Session-clock
reductions were20.246,100.364 and−96.608ms; that interval did not consistently
improve. Map pipeline reductions were819.68,822.50 and811.75ms. Boot and map
workers continued pumping window events; observed gaps remain in the report.
Do not sum nested timings or equate either clock origin with process launch.
Three samples do not establish reliable startup tail distributions.

This qualifies controlled engine shader/pipeline cache reuse for this DX12
configuration. OS, driver and filesystem caches remain uncontrolled, and no
engine FPS improvement is inferred. Vulkan and other resolutions/modes need
their own evidence. It also does not isolate the multipart-hash change: both
members of every pair use that same build17 implementation.
