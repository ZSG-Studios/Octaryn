# Static opaque visibility witness experiment

Status: rejected after build34 matched GPU measurements and retired from runtime
source for build35. No runnable witness variant remains. Fewer ray queries did
not produce faster rendering.

## Build34 result

Three matched quiet runs per variant/backend, 2560x1440 output reconstructed from
1280x720, show these medians of run means (milliseconds):

| Backend | GPU original / witness | Reflection trace original / witness | GPU regression |
|---|---:|---:|---:|
| Windows DX12 | 5.99825 / 6.74805 | 3.05121 / 3.83351 | +12.50% |
| Windows Vulkan | 5.07169 / 5.96904 | 2.53828 / 3.42728 | +17.69% |

Every matched pair regressed. GPU increases of 0.74980 ms DX12 and 0.89735 ms
Vulkan exceed the largest within-variant ranges (0.02906 and 0.01109 ms).
Production quiet diagnostic modes were DX12 counter0 and Vulkan counter1, without
collection. This is fixed-resolution custom-profile comparison evidence, not a
completed HQ200 gameplay budget qualification.

Separate native1440 diagnostic captures preserved all twelve consecutive cut
images exactly on each backend. All 360 earlier counter rows matched when actual
secondary queries plus saved queries were balanced against the original. Static
eligibility was positive with zero dynamic instances. DX12 saved 132,599,571 of
492,503,614 secondary queries (26.924%); Vulkan saved 128,167,521 of 492,985,340
(25.998%). Reviewed street/foliage/storefront sheets and native cobble/bollard
crops showed no added artifact in that scope. This did not qualify controlled
alpha/gap/edge/material cases or populated fallback; those tests are cancelled
because the performance gate failed. Existing temporal artifacts remain outside
this exact static-scene comparison.

Reports: `logs/client/hq200-build34-quality-review.json` and
`logs/client/hq200-build34-witness-comparison.json`. Original images, CSVs, build
identities and commands remain under `logs/client/hq200-build34` and
`logs/tools/build34-qualification.json`. There is no measured attribution yet to
leader serialization, interval ALU, register pressure or traversal scheduling.

## Archived implementation and numerical evidence

Eight DXIL/SPIR-V shader variants, native syntax and 12 focused tool tests passed.
The production Slang CPU predicate passed 5,965 exact-rational cases with 803
accepted intersections in each of gradual and FTZ/DAZ modes. Evidence remains at
`build/release-windows/tools/opaque-witness`; passing these checks did not imply
performance acceptance.

The retired `OCTARYN_CLIENT_RT_VISIBILITY_WITNESS=1` selection previously chose a
dedicated fused deferred-map reflection shader. It now fails explicitly; the
capture helper rejects the removed witness/original options. Generic/eager/queued or active transport-GI paths are ineligible.
The existing triangle-scene check remains required. An item instance anywhere in
the current scene, or enabled player-shadow scene, uses the original visibility
query. This first experiment does not qualify populated gameplay acceleration.

The first active sun-query lane traces its ordinary current-frame visibility ray.
Only a committed static opaque map triangle can become a witness. Other lanes
test their own exact biased origin, twice-normalized direction, Tmin and Tmax
against that triangle. A certified interior intersection means the ray is
occluded, irrespective of which static blocker a hardware any-hit traversal would
return. A miss, uncertain predicate, alpha material, player hit, dynamic scene or
lit leader falls back to the original hardware query. No lit result is shared.
Static map instances currently use identity transforms; translated item instances
are excluded. Both triangle orientations remain eligible because original rays
do not cull backfaces.

The predicate uses outward-rounded float32 intervals, following the treatment in
[PBRT's Managing Rounding Error](https://pbr-book.org/4ed/Shapes/Managing_Rounding_Error).
For signed Moller-Trumbore determinant D and numerators U,V,T, the determinant is
made strictly positive; acceptance requires interval lower bounds U,V>0, upper
bound U+V<D, and strict Tmin*D<T<Tmax*D. No division or geometric epsilon is used
in the certificate. A preliminary approximate test only rejects reuse; it never
accepts occlusion. Every bound operation expands with bit-level next-float steps;
subnormal inputs/results expand to minimum-normal bounds to cover FTZ/DAZ.
Overflow, NaN, degenerate and range/edge-overlap intervals reject reuse.

The interval argument depends on observable float32 rounding at endpoint bit
conversions. A `precise` annotation alone is insufficient: a
[Slang SPIR-V issue](https://github.com/shader-slang/slang/issues/12198) reports
that qualifiers can be lost. Production DXIL/SPIR-V must be inspected to confirm
rounding barriers remain. The current emitted DXIL/SPIR-V retains those integer
rounding dependencies, finite checks and minimum-normal widening; this is not
hardware ISA validation. CPU generated-Slang comparisons use exact rational
arithmetic on the same float32 inputs, under gradual and FTZ/DAZ modes. This
checks shader arithmetic, not undocumented hardware traversal tolerances.

Diagnostics schema 6 preserves reserved historical fields for logical requests,
interval tests, saved queries and
actual witness-path hardware queries. Logical = saved + actual; saved <= tests
<= logical. The existing `reflection_visibility_queries` remains actual hardware
work, including other reflection paths. Primary directions, history and material
evaluation remain unchanged. The witness cannot use dynamic blockers because
their negative-history tag would otherwise change even when visibility remains
zero.

The original qualification plan required the numerical oracle and shader codegen,
native 1440p consecutive cuts and controlled opaque/alpha/gap/edge cases
on DX12 and Vulkan, and positive saved-query evidence. The moving-item
fixture would additionally need complete fallback with unchanged history/tag
behavior. The matched uninstrumented timing gate failed first. Leader
serialization, interval ALU and register pressure may cost more than the avoided
traversal. No qualification is inferred from a passing build or oracle.
The counter-disabled experimental binary is larger than the deferred reference:
53,236 versus 30,636 bytes DXIL; 282,020 versus 129,476 bytes SPIR-V. These are
compiled artifact sizes, not measured GPU costs.

## Retirement

The candidate shader, predicate, probe and controls, together with the complete
Slang include tree, are preserved under
`logs/client/hq200-build34/rejected-source`. Its `manifest.json` records 118
SHA-256-verified source files before cleanup. Compiled artifacts and build34
results remain unchanged. Runtime selection/state/binding, candidate shader
wrappers and probe tooling were removed; the shared ray query again normalizes
its direction without a witness-only option. Schema6 readers and reserved
counters remain intact for historical evidence. No WorldFrame, GPA or diagnostic
writer ownership changes were included in retirement.
