# Fused reflection wave-size experiment

Build33 completed focused native1440 quality and three matched timing runs per
variant. Forced32 did not improve beyond observed variation; forced64 regressed
whole-GPU time by 22.55%. Neither becomes the default. This remains an opt-in DX12
experiment. `OCTARYN_CLIENT_REFLECTION_WAVE_SIZE` accepts
absent/`0` (driver selected), `32`, or `64`. Other values, including empty explicit
values, fail startup. Forced widths on Vulkan/Metal fail. Device setup freezes
the option, checks actual RHI `WaveOps` and `DeviceInfo.limits.minWaveSize` and
`maxWaveSize`, and passes the macro to the Slang session on both device attempts.

Only the map-only fused temporal entry point receives `[WaveSize(...)]`, through
`Shaders/Hdr/ReflectionWaveAttributes.slang`. Its workgroup remains 8×8×1.
Shadows, forward reflections, queued reflections, filters and other shaders keep
their previous wave selection. The generic temporal entry does not receive the
attribute. A forced request rejects incompatible map-only/temporal/queue/RT/range
settings. The existing actual triangle-scene guard remains before dispatch.
The ready pipeline marker is mandatory in capture qualification for forced modes.

The pinned DX12 backend targets shader model6.8; its native wave limits come from
the D3D12 device. DXIL's required-wave metadata supplies enforcement. In contrast,
the pinned Vulkan pipeline builder does not consume Slang's reflected wave size
into a required-subgroup-size pipeline structure. This slice therefore does not
offer forced Vulkan widths. Relevant specifications:
[Slang WaveSize](https://docs.shader-slang.org/en/latest/external/core-module-reference/attributes/wavesize-04.html),
[DirectX wave-size contract](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_WaveSize.html),
[Vulkan required subgroup size](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineShaderStageRequiredSubgroupSizeCreateInfo.html).

`reflection_wave_mode` reports requested/forced configuration, device limits and
whether diagnostic observation is compiled. `actual_observed=unknown` is
intentional: neither the request nor reported limits measures a running shader.
`reflection_wave_path` reports the prepared fused pipeline. Collection produces
`reflection_wave_observed` after frame-fenced readback and schema5 ray CSV fields
`reflection_wave_observations`, `reflection_wave_max`, `reflection_wave_min`.
The shader observes `WaveGetLaneCount()` once per subgroup at fused-entry start,
before bounds or material rejection. This counts dispatched subgroups, not rays,
occupancy, utilization or traversal throughput.

Observation has its own compile-time macro, enabled only with a diagnostic output
path. Quiet counter1 keeps the previous runtime branches without the new wave
instruction; counter0 has no counter UAV, runtime flag or observation body.
Diagnostic collection requires actual WaveOps support. Zero observations become
unknown in the reader, never a claimed zero-width wave. Min/max are reduced as
widths, not summed. An expected forced width with no samples or mismatched samples
fails validation. Historical schema1–4 remain readable with unknown wave width.

Both SDK entry hashes and persistent cache namespaces distinguish the forced
width and observation macro. The namespace includes `reflectionwave0|32|64` and
`waveobserve0|1`, alongside `raycounters0|1`; no cache is deleted. CPU probe tests
compile DXIL0/32/64, inspect real Slang reflected wave attributes, test a generic
entry with no attribute, compare hashes and alternate exact binary cache reads.
DXIL/SPIR-V default observation variants compile separately from quiet controls.
Capability/path rejection, counter removal and frozen collection-path checks
also pass. Evidence: `logs/build/build33-ray-wave-variants.log`,
`logs/build/build33-reflection-wave-syntax.log`, and
`logs/tools/ray-counter-variants.json`; 19 focused Python cases pass.

`capture_map_world.py --reflection-wave-size 0|32|64` records configuration and
requires its runtime markers. With `--ray-diagnostics`, it also validates actual
GPU observations. Diagnostic results cannot qualify FPS. The completed runtime work below separates native 1440p quality with equivalent
sampling from quiet same-binary three-run timing controls. Every rendered output remains
2560×1440. No existing DX12/Vulkan timing gap is attributed to wave size: previous
offline RGA reports lacked reliable runtime wave-size evidence.

## Completed build33 evidence

Canonical `octaryn_all` and all seven native quality captures, six separate
warmups and eighteen timing captures completed. Receipts:
`logs/client/hq200-build33-material-wave-quality-review.json` and
`logs/client/hq200-build33-material-wave-comparison.json`. All output is 2560×1440;
quality is native, timing uses the explicitly separate fixed 1280×720 internal
custom profile. This is camera-only comparison, not sustained gameplay/streaming
or complete HQ200 acceptance. All measured-window outliers remain included.

| Windows DX12 quiet deferred variant | Median run-mean GPU | Change versus driver selection |
|---|---:|---:|
| Driver selected | 5.991951 ms | reference |
| Forced32 | 5.976903 ms | 0.015049 ms faster; below 0.020687 ms variation |
| Forced64 | 7.343304 ms | 1.351353 ms slower; 22.55% regression |

Quiet DX12 uses production counter0. Instrumented native quality observed 32 for
the driver-selected DX12 shader, 32 for forced32 and 64 for forced64. That does
**not** establish the quiet driver-selected shader's width: instrumentation can
change compilation and driver selection. The forced-width attribute contract
and CPU reflection tests cover quiet forced variants; their widths are not
measured by quiet CSV records.

Across twelve matched native cut frames, driver-observed versus forced32 is
exact RGBA and all 21 pre-wave counter fields match across 360 rows. Forced64 has
900 changed pixel-observations among 44,236,800, maximum 6/255, mean absolute
channel error 0.0000084093 and RMS 0.0031657. Ray totals also differ slightly:
primary queries−11 among 1,085,079,514 and secondary visibility−1090
among 492,503,614. The inspected full-frame sheets and enlarged pavement/grate
crop show no obvious structural change, but exact equivalence is not established
and the cause of the differences is unproven. Forced64 is unsuitable as a
performance default regardless of this narrow visual result.

The same build separately qualified deferred material evaluation under production
counter policies: DX12 counter0 GPU 6.058963→5.991951 ms (1.106% reduction versus
0.037735 ms within-variant range), Vulkan quiet counter1
GPU 5.326147→5.116040 ms (3.945% versus 0.043913 ms range). Every paired run improved.
DX12 quiet eager/deferred native frames were exact RGBA. Vulkan instrumented
eager/deferred frames were exact RGBA and old 17 counters matched; the two intended
material-work counters changed. Both Vulkan diagnostic shaders observed 64, which
still cannot prove quiet selection. These measured gains are not added to gains
from a different counter/shader configuration.

Whole-frame acceptance remains failed: Vulkan deferred measured windows include
48.327 and 63.635 ms wall frames. The latter contains 61.992 ms inside the frame-slot
fence wait, while the former contains 29.057 ms there plus 18.002 ms outside the
recorded render substages. GPU timestamp work on those submitted frame IDs is
4.477 and 7.237 ms respectively; the CPU waits retire older slot submissions,
not the current frame's GPU interval. This does not identify OS scheduling,
presentation or shader execution as the cause. The source-level measurement gaps
and exact joined rows are retained in
`logs/tools/build33-vulkan-wall-attribution.json` and its accompanying Markdown
analysis. Private initialization GPU work and precise active CPU execution remain
outside established complete-budget coverage.
