# Compiled ray-counter variants

Build32 introduced compiled ray-counter removal without changing ray counts,
sampling, materials, history or shadows. Matched Windows DX12 qualification now
supports default removal there. Vulkan's measured difference remained within
noise; build33 retains quiet compiled counters on Vulkan, Metal and other
unqualified platform/backend combinations. Explicit mode0 remains available.

Device creation freezes `OCTARYN_CLIENT_RAY_COUNTERS` and the owned collection
path before creating the Slang session:

| Control | Collection path | Compiled mode | Runtime collection |
|---|---|---|---|
| absent/empty, Windows DX12 | absent/empty | 0 | off |
| absent/empty, other backend/platform | absent/empty | 1 | off |
| absent/empty | nonempty | 1 | on |
| `0` | absent/empty | 0 | off |
| `1` | absent/empty | 1 | off |
| `1` | nonempty | 1 | on |
| `0` | nonempty | rejected | none |

Other nonempty control values fail startup. The collection path retains its
existing `OCTARYN_CLIENT_RAY_DIAGNOSTICS` name. Mode cannot change within an
existing device; mode and path are not reread at first frame. A new device may
select a new variant through a new Slang session.

`OCTARYN_RAY_COUNTERS=0` compiles both helper functions as no-ops, without the
UAV or runtime-enabled uniform. The CPU profile performs no GPU allocation,
upload, binding or readback for this mode. Mode1 retains the existing uniform
branch and atomics exactly; mode1 without collection is the quiet same-binary
legacy control. Only collecting runs allocate readback resources and record CSV.

The device descriptor supplies the macro on both initial and descriptor-capacity
retry attempts. Slang's entry-point dependency hash separates changed shader
variants. Persistent shader and pipeline cache directories additionally include
`raycounters0` or `raycounters1`; old directories are preserved. DX12 pipeline
keys also hash shader bytecode/root signatures; Vulkan uses its backend pipeline
key. The native fixture verifies real Slang hashes, rather than assuming macro
identity from a label.

`ray_diagnostic_mode` records requested/resolved/actual selected mode, runtime
collection and cache namespace. The shader binder rejects counter resources in
a compiled-off device. The marker records the device's compile configuration;
it is not a hardware-counter disassembly assertion. Source binding reflection
is separately tested on both emitted targets.

`python tools/validation/validate_ray_counter_variants.py` passes with receipt
`logs/tools/ray-counter-variants.json` and build output
`logs/build/build32-ray-counter-variants.log`. The fixture compiles the production
Diagnostics.slang include to DXIL and SPIR-V, verifies counter/uniform absence
in mode0 and presence in mode1, differing real entry hashes and bytecode, stable
fresh-session identities and alternating persistent-cache lookups in the same
directory. It tests invalid selection, owned/frozen path, one-time configuration
and allocation-free compiled-off begin/end/drain with null GPU interfaces.
Three affected renderer translation units pass syntax checks in
`logs/build/build32-ray-counter-render-syntax.log`. Five focused capture-option
tests pass, including the real CLI rejecting collection with compile0 before
creating an evidence directory or launching a process. These CPU checks do not
establish GPU timing or image parity.

The separate capture option module supports `--ray-counter-shader auto|0|1`,
records requested/resolved/actual values and rejects missing/mismatched activation.
It is now integrated in the map capture helper after the build31 fixture batch
finished. Subsequent captures require build32's marker; historical capture files
remain unchanged. Reusing the new helper against older binaries fails closed.

Build32 completed four native1440 quality captures, four separate warmups and
twelve fixed720-internal/1440-output timing captures with collection and deferred
material off. The timing receipt is
`logs/client/hq200-build32-counter-comparison.json`. Three pairs per backend give
these median run-mean whole-GPU results:

| Backend | Quiet counter1 | Counter0 | Reduction | Largest within-variant mean range |
|---|---:|---:|---:|---:|
| Windows DX12 | 6.400784 ms | 6.046835 ms | 0.353949 ms | 0.050750 ms |
| Windows Vulkan | 5.328327 ms | 5.315444 ms | 0.012883 ms | 0.030470 ms |

DX12 reflection trace improved 10.386%; the Vulkan whole-GPU difference does not
exceed noise. These are short camera-only fixed custom-scale workloads, not the
complete HQ200 gameplay/streaming contract. Whole-frame targets still fail.
Main plus explicit AS timing continues to omit private initialization queue work.

The native image receipt is
`logs/client/hq200-build32-counter-quality-review.json`. Twelve matched frames
per backend had no obvious structural difference in inspected full frames and
enlarged worst-difference crops, but are not pixel identical: DX12 had 86 changed
pixel-observations among 44,236,800 (maximum 9/255), Vulkan 26 (maximum 10/255).
The cause of those isolated differences is unknown. This is narrow static-view
evidence, not a moving-item reflection or all-platform quality claim.

Build33 extends the native CPU fixture with forced reflection wave attributes and
collector-only wave telemetry. Its current receipt is
`logs/tools/ray-counter-variants.json`; the build log is
`logs/build/build33-ray-wave-variants.log`. New cache suffixes additionally record
requested reflection width and telemetry compilation. See
[reflection-wave-size.md](reflection-wave-size.md) for scope and pending runtime
qualification. The original build32 logs and capture records remain unchanged.
