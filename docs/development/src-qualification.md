# Split Radiance Cascades qualification pack

Status: active procedure. Nothing in this file claims SRC is done or that the
DDGI default may flip. The flip happens only after every row of the pass
criteria table below reports `passed` on a single current build, with the
evidence directories retained.

Scope: this pack proves the SRC runtime path on Windows x64 / DX12 through the
isolated validation bundle. Vulkan and Metal require separate runs of the same
runners on those platforms; Windows results are not evidence for them.

## Runners and commands

All commands run from the repository root with the isolated bundle at
`build/release-windows/client/validation/lighting-response` (exe and shaders
staged by the bundle owner). Every runner creates fresh fixture worlds under
`logs/client/src-qualification/<case>`; none touches `saves/` or the shipping
`build/release-windows/client/bundle`, injects input, or automates UI.

1. SRC counters and merge/resolve bounds (outdoor fixture, readback-free):

   ```
   python tools/validation/qualify_src_counters.py \
     --client-bundle-root build/release-windows/client/validation/lighting-response \
     --evidence-root logs/client/src-qualification/counters
   ```

   Defaults: `--frames 600 --captures 8 --stride 31 --min-frame 300
   --budget-ms 50 --timeout 1500`. Evidence: `<case>/result.json`,
   `<case>/lighting.csv`, `<case>/client.log`, eight `frame.bmp*` captures.
   Status is `passed`, or `measured` when only the merge/resolve budget is
   exceeded (recorded, never asserted away). `--strict` converts budget
   overruns into failures; use it only after the Resolve performance fix lands.
   The runner passes `--validate-lighting-edits` so the client freezes the
   pose, keeps captures enabled, and writes the local server log per case
   (`<case>/world/logs/server/local-session.log`) instead of the shared
   `logs/server/local-session.log`, which is opened without write sharing and
   fails whenever another session is live (observed 2026-09-17,
   `logs/client/src-qualification/counters/src-counters-savj01o7`).

2. Integrated lighting architecture with SRC selected:

   ```
   python tools/validation/validate_lighting_architecture.py \
     --client-bundle-root build/release-windows/client/validation/lighting-response \
     --evidence-root logs/client/src-qualification/architecture-src \
     --backend dx12 --gi src --no-rhi-validation --frames 600 --timeout 1500
   ```

   With `--gi src` the runner additionally requires nonzero steady GPU time in
   `src_seed_ms`, `src_trace_ms`, `src_deposit_ms`, `src_merge_ms`,
   `src_contact_ms`, `src_evaluate_ms` and requires `ddgi_trace_ms` /
   `ddgi_update_ms` to be exactly zero on every frame (SRC replaced DDGI, not
   ran beside it).

3. Torch cold/add/remove response tunnel, both backends:

   ```
   python tools/validation/qualify_torch_response.py --gi src \
     --client-bundle-root build/release-windows/client/validation/lighting-response \
     --evidence-root logs/client/src-qualification/torch-src
   python tools/validation/qualify_torch_response.py --gi ddgi \
     --client-bundle-root build/release-windows/client/validation/lighting-response \
     --evidence-root logs/client/src-qualification/torch-ddgi
   ```

   Same fixture, 64 captures, stride 31, warmup/min frame 600, both frame-slot
   parities enforced. SRC runs default to `--timeout 1500` and no RHI debug
   layer; `--rhi-validation` re-enables the layer explicitly. Per-region output
   includes luminance curves, `residual_to_added_ratio`, removed-phase
   direct-light zero checks, and an `src_residual_criterion` block
   (threshold 0.15) reported with `status=measured` until the Resolve
   performance fix lands.

4. DDGI vs SRC outdoor daylight comparison:

   ```
   python tools/validation/compare_gi_backends.py \
     --client-bundle-root build/release-windows/client/validation/lighting-response \
     --evidence-root logs/client/src-qualification/gi-comparison
   ```

   Runs the `validate_lighting_architecture` block-lights fixture at start hour
   12 under both backends (each through `--validate-lighting-edits` for the
   fixed pose and per-case server log), converts each `frame.bmp` to
   `frame.png`, and writes
   `comparison.json` with matched-region luminance statistics (`open_stone`,
   `shaded_pillar`, `sky`; boxes overridable with `--boxes regions.json`) plus
   per-backend GI pass timings. Values are measured; cross-backend equality is
   never asserted.

## Environment knobs

| Knob | Meaning in this pack |
| --- | --- |
| `OCTARYN_CLIENT_GI=src` | Selects Split Radiance Cascades; default remains DDGI |
| `OCTARYN_CLIENT_RHI_VALIDATION` | Presence-based: the client enables the D3D12 debug layer whenever the variable exists, regardless of value (`WorldRendererDevice.cpp` reads `getenv(...)!=nullptr`). Runners therefore unset it entirely for `--no-rhi-validation`; a literal `0` value does not disable the layer |
| `OCTARYN_CLIENT_LIGHTING_PROFILE_PATH` | `lighting.csv` GPU timestamp source for every timing claim |
| `OCTARYN_CLIENT_CAPTURE_COUNT/STRIDE/MIN_FRAME/STABLE_FRAMES` | Capture sequence control; stride 31 keeps both frame-slot parities |
| `OCTARYN_CLIENT_CAPTURE_PATH` | `frame.bmp` base capture path |
| `OCTARYN_SRC_*` | Runtime tuning (`SPACING`, `CASCADES`, `ANGULAR_RESOLUTION`, `BASE_CAPACITY`, `VISIBLE_LIFETIME`, `SECONDARY_LIFETIME`, `MAX_SURFACE_RAYS`, `HASH_SEARCH_LIMIT`, `MEMORY_MIB`, `CONTACT_LENGTH`, `BASE_INTERVAL`, `INTERVAL_GROWTH`, `DECAY`, `MAX_TRACE_DISTANCE`, `FEEDBACK`, `LOD_RADIUS`, `LOD_BLEND`) |
| `OCTARYN_SERVER_START_HOUR` | Daylight fixture hour for the comparison runner |

Timeouts: first launches after shader edits spend minutes compiling the SRC
pipeline set; all capture runs use `--timeout 1500`.

## Pass criteria

Steady state is the second half of completed `lighting.csv` rows.

| # | Criterion | Threshold / rule | Runner | Status |
| --- | --- | --- | --- | --- |
| 1 | SRC passes execute with real GPU time while terrain resident | `src_seed_ms`, `src_trace_ms`, `src_deposit_ms`, `src_merge_ms`, `src_contact_ms`, `src_evaluate_ms` all nonzero steady median and nonzero maximum (probe seeding and ray tracing implied by nonzero seed/trace) | qualify_src_counters | pending |
| 2 | SRC replaced DDGI, not accompanied it | `ddgi_trace_ms` and `ddgi_update_ms` zero on every frame | qualify_src_counters, validate_lighting_architecture --gi src | pending |
| 3 | Merge bounded | steady `src_merge_ms` max < 50 ms | qualify_src_counters | pending |
| 4 | Resolve bounded | steady `src_contact_ms` (Resolve column) max < 50 ms | qualify_src_counters | pending; open defect, enclosed fixture measured ~1.0-1.3 s/frame on 2026-09-17 |
| 5 | Frame parity captures exist | 8 captures (torch tunnel: 64), stride 31, min frame 300 (torch: 600), both `frame % 2` parities, each with `.lighting.json` sidecar | qualify_src_counters, qualify_torch_response | pending |
| 6 | Clean production exit | `open_world_exit code=0`, full 81-column terrain window, nonzero quads/GPU bytes, no `world_src_*_failed` / `src_pass_failed` markers | all runners | pending |
| 7 | Torch add response | added phase issues real GPU receiver work; removed phase zeroes `local_light_count`, `block_source_count`, `evaluated_lights`, `local_visibility_rays` | qualify_torch_response --gi src | pending |
| 8 | Torch removal convergence | per-region `residual_to_added_ratio` < 0.15 and within cold noise | qualify_torch_response --gi src | pending; emitted `status=measured` until row 4 passes |
| 9 | DDGI regression parity | `qualify_torch_response --gi ddgi` still measures its historical sequence after runner changes | qualify_torch_response --gi ddgi | pending |
| 10 | Outdoor comparison completes | both backends capture the daylight fixture; matched-region table + PNGs written | compare_gi_backends | pending |
| 11 | Visual inspection | checklist below, on actual GPU PNGs, recorded with evidence paths | human | pending |

### Capture inspection checklist

Inspect the PNGs in each evidence directory (never synthetic screenshots):

- `open_stone` receiver is lit by sun plus indirect bounce; no unoccluded-sky
  look on shaded faces.
- `shaded_pillar` shows contact darkening at the base, no light leak through
  the one-block-thick geometry.
- `sky` region free of probe artifacts, streaking or cascade-boundary seams.
- Torch tunnel: `added.png` shows torch pool on floor/walls; `removed.png`
  matches `cold.png` within visible noise, no stale torch glow.
- Both frame-slot parities visually equivalent (no alternate-frame flicker).

## Recorded baselines (current build, before concurrent fixes)

Filled from the runs executed on 2026-09-17 against the current isolated
bundle; these are measurements, not passes. See the evidence directories for
raw artifacts.

- Counters run: `logs/client/src-qualification/counters/`
  - `src-counters-rxgqj7j_` (2026-09-17): debug layer accidentally active
    (presence-based env semantics), `src_previous_fp32` duplicate-barrier
    warnings then `DXGI_ERROR_DEVICE_HUNG` TDR at frame 0.
  - `src-counters-savj01o7`: shared `logs/server/local-session.log` lock
    contention with a concurrent session; fixed by `--validate-lighting-edits`.
  - `src-counters-5mtmkjcr`: bundle exe was replaced mid-run (rebuilt 3:29:59
    PM while the run was live); `world_frame_failed stage=submit` at frame 0
    with the first five frames already profiled (`src_seed_ms` 1.6-8.7,
    `src_contact_ms` 0.02-0.03 outdoor). Not evidence against any build.
  - `src-counters-hv81hrj3` (stable bundle, quiet window): reproducible
    `world_frame_failed stage=submit` right after the first terrain columns
    stream in. `lighting.csv` frames 3-6 show `src_contact_ms` (Resolve) at
    **1553-1978 ms** with only two resident columns; the ~2 s GPU command list
    trips the Windows TDR, which surfaces as a submit failure with no debug
    layer message. The open Resolve defect therefore currently blocks *every*
    SRC capture run (counters, architecture, comparison, torch tunnel) on this
    build, outdoor fixtures included.
  - Qualification runs must be serialized against bundle rebuilds and other
    client sessions; timing rows are also polluted by unrelated GPU load.
- Architecture run: `logs/client/src-qualification/architecture-src/`
- Torch tunnel: `logs/client/src-qualification/torch-src/`,
  `logs/client/src-qualification/torch-ddgi/`
- Backend comparison: `logs/client/src-qualification/gi-comparison/comparison.json`

## Blocked on concurrent repairs

- **All SRC capture rows (1-8) are blocked on the SRC Resolve performance
  fix.** On the 2026-09-17 3:29 PM bundle the Resolve pass reaches
  1553-1978 ms/frame as soon as the first terrain columns become resident
  (`src-counters-hv81hrj3`), which trips the Windows TDR and fails the frame
  submit; no SRC run can currently reach its capture sequence, outdoor
  fixtures included. The runners record partial `lighting.csv` timings on
  failure so each attempt still leaves measured Resolve evidence
  (`partial_profile` in `result.json`).
- Row 4 (Resolve bound) and row 8 (removal convergence) remain blocked on the
  same fix (`Shaders/` + `SplitRadianceCascades/Dispatch.cpp` owner). Until it
  lands, the enclosed tunnel fixture spends second-scale time in
  `src_contact_ms` and the torch sequence either times out or records
  unconverged residuals; both outcomes are recorded with `status=measured`.
- The `--no-rhi-validation` requirement is blocked on the D3D12 debug-layer
  overhead fix (`RenderBackend/WorldSrcIntegration.cpp` owner). When that fix
  lands, re-run every row with `--rhi-validation` and drop the limitation.
  Measured with the layer accidentally active (2026-09-17,
  `logs/client/src-qualification/counters/src-counters-rxgqj7j_`): repeated
  `rhi_validation severity=warning ID3D12CommandList::ResourceBarrier` for
  `src_previous_fp32` (same subresource in separate barrier descs) followed by
  `DXGI_ERROR_DEVICE_HUNG` TDR removal at frame 0. Two concrete defects for the
  debug-layer owner: the duplicated `src_previous_fp32` barrier, and the
  presence-based `OCTARYN_CLIENT_RHI_VALIDATION` semantics that make `=0`
  enable validation.
- The DDGI default flip is blocked on every row above reporting `passed` on
  one build, plus separate Vulkan and Metal qualification runs.
