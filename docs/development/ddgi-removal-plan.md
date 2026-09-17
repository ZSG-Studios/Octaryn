# DDGI removal plan — preparation only

Status: **prepared, not executed.** Written 2026-09-17 at user request once SRC
runs proved functional. Nothing here may be executed until
`docs/development/src-qualification.md` reports every row `passed` on one
current build and main has visually inspected the comparison captures. SRC must
be the default before any DDGI symbol is deleted.

## Replacement map

| DDGI responsibility | SRC replacement |
| --- | --- |
| Probe irradiance for composite (`ddgi_sample`) | `srcIrradiance` texture from `Resolve.slang` (donor-oct gather + C-1 contact) |
| Recursive bounce inside trace (`DDGITrace` uses `ddgi_sample`) | `src_feedback` previous-frame oct in `HitRadiance.slang` |
| Fluid indirect (`WorldFluidShade.slang` line ~98 `ddgi_sample`) | New SRC fluid sample: donor oct gather at hit position via `src_spatial`+`src_oct_sample`, coverage from donor weights; zero when no donors (caves stay dark) |
| Edit invalidation (`ddgi_invalidate`, occupancy follow) | Geometry epoch + `WorldTracePublication` republication; SRC links/visibility re-evaluated per frame |
| Probe debug views 2–7, 27 | SRC debug views (cascade/hit-cascade/age/quality) to be added in `SrcDebug` pass before removal |
| Capture sidecar `ddgi_*` counters and `.ddgi-probes.json` | SRC counters (probe/ray/merge/contact stats) added to `LightingCapture` before removal |
| Settings `giVoxelRadius`/`giCoarseRadius` | SRC tuning (`OCTARYN_SRC_*` env today; settings entries later if product-facing) |

## Delete list (after flip)

- `octaryn-client/Source/Rendering/RenderBackend/`: `DDGISystem.h/.cpp`,
  `DDGISchedule.cpp`, `DDGIOccupancy.h/.cpp`, `DDGIDebug.h/.cpp`,
  `DDGIVolumeConfig.h`; remove their lines from
  `cmake/Owners/ClientTargets/ClientNativeLibraryTargets.cmake`.
- `octaryn-client/Shaders/DDGI/` entire folder (`DDGISample`, `DDGITrace`,
  `DDGIUpdate`, `DDGISeed`, `DDGISkyVisibility`, `DDGIEnvironment`, `DDGITypes`,
  `ProbeDebug`, others).
- `WorldRendererInternal.h`: `#include "DDGISystem.h"`, `DDGISystem ddgi;`
  member; `WorldRenderer.h`: `open_world_renderer_set_ddgi_range` declaration;
  `LightingSystem.cpp`: range setter, `world_ddgi_initialize/debug/update`
  graph pass and calls; `WorldHdr.cpp` and `WorldDraw.cpp`: `world_ddgi_bind`.
- `LightingQuality.h`: `ddgi_voxel_radius/ddgi_coarse_radius`; settings JSON
  keys `giVoxelRadius/giCoarseRadius` deserialization in
  `Settings/AppSettings` + `Ui/RuntimeControls` menu fields; `WorldSession.cpp`
  `open_world_renderer_set_ddgi_range` call.
- `GameUiUpdate.cpp` debug-view names 2–7/27–30 → SRC names.
- `LightingCapture.cpp`: `ddgi_*` JSON fields, `CAPTURE_DDGI_STATES` sidecar,
  `DDGIProbe/Control` reads; `LightingProfile.h`: `DDGITrace/DDGIUpdate` enum
  entries and CSV columns (update every runner that parses them:
  `validate_lighting_architecture.py`, `lighting_profile_summary.py`,
  `qualify_torch_response.py`, `validate_lighting_motion.py`).
- `Composite.slang`: DDGI include/sample/fallback and `OCTARYN_COMPOSITE_RAY_SKY`
  sky-visibility branch (SRC resolve owns uncovered receivers); keep
  `CompositeRT.slang` only while ray-sky path still needed by SRC? SRC does not
  need it → delete `CompositeRT.slang` and its pipeline slot too.
- Tools referencing DDGI probe states: `qualify_torch_response.py` ddgi probe
  analysis branch, `lighting_tunnel_fixture.py`/`cave_lighting_fixture.py`
  ddgi-state assertions, `validate_lighting_motion.py --probe-states`.

## Ordered execution

1. SRC default flip in `WorldRenderer.cpp` (`OCTARYN_CLIENT_GI` unset ⇒ src;
   `ddgi` escape hatch retained one release).
2. Fluid shader SRC sampling landed and captured (water/lava indirect visible).
3. SRC debug views + capture counters landed; runners updated to SRC fields.
4. Delete DDGI graph pass/bindings; verify DDGI escape hatch still compiles?
   No: escape hatch removed in same step; keep flip commit revertable.
5. Delete files/CMake/settings/UI/capture fields; update runners; full
   qualification rerun (architecture, torch convergence, counters, comparison).
6. Docs: mark `lighting-architecture.md` historical, update
   `repair-progress.md`, `feature-parity.md`, this file status.

Each step is its own commit; step 5 only after step 4 qualification green.
Vulkan/Metal SRC runs remain separate evidence and do not block Windows DDGI
deletion, but the deletion commit message must list which platforms still lack
SRC runtime proof.
