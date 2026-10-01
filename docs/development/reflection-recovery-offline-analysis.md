# Reflection recovery offline compiler analysis

RGA 2.14.2.8 completed **offline Vulkan** analysis for gfx1201 of three preserved
standalone Slang SPIR-V artifacts. These artifacts contain the build22 dynamic-hit
changes; they are not the measured build21 runtime shader binaries. Input/tool
hashes, commands, ISA, liveness and CSV statistics are preserved under
`logs/tools/rga-recovery-db17befd/`, summarized in `analysis.json`.

| Entry | VGPR requested | SGPR used | Scratch | VGPR/SGPR spills | LDS | ISA bytes |
|---|---:|---:|---:|---:|---:|---:|
| recovery_main | 113 | 99 | 0 | 0 / 0 | 4,096 B | 23,280 |
| recovery_refine_main | 113 | 94 | 0 | 0 / 0 | 4,096 B | 23,408 |
| shade_main | 114 | 82 | 0 | 0 / 0 | 4,096 B | 17,716 |

SHAE reports 120 allocated VGPRs for each kernel and maximum live counts of
113, 113 and 112 respectively. These are compiler-tool results, not measured
hardware occupancy. The CSV reports zero wavefront/workgroup dimensions, so
those fields cannot support an occupancy calculation. The live-analysis command
uses wave32. The offline compiler generates descriptor layout automatically;
the runtime Vulkan pipeline layout and DX12 root signature were not captured.

No spill or scratch regression is demonstrated by this analysis. The current
`referenceRecovery` uniform switches full-count and two-sample recovery within
the same `recovery_main` kernel; those settings have the same static register
allocation. Bounded recovery adds a separate classification/refinement dispatch,
and refinement does not demonstrate lower register allocation. The older kernel
before bounded recovery was introduced was not preserved as a standalone binary,
so this table cannot compare its register usage or explain its measured advantage.

Source inspection identifies candidate costs to measure next: per-ray moment
accumulation; a 32-byte initial sample record per recovery pixel; neighborhood
classification reads; extra dispatch/barrier traffic; and pixel-hashed ray phases
that change traversal coherence. These are hypotheses, not established causes.
Removing alpha tests, secondary visibility, material evaluation or required rays
would change the comparison contract and is not an acceptable diagnostic shortcut.

For a matched attribution, preserve both exact runtime shader binaries and their
pipeline metadata before editing. A DX12 compiler comparison needs the serialized
runtime root signature with the DXIL (`--cs-blob` and `--rs-bin`); do not substitute
an invented root layout and call it the hardware result. Use an actual RGP capture
for occupancy, cache/memory traffic, waits and traversal cost. Isolate sampling
phase, record writes and classification costs one at a time with identical views,
ray counts and quality checks, and report driver/compiler differences explicitly.

The reproducible offline command is recorded in each `*-command.json`:

```text
rga.exe -s vk-spv-offline -c gfx1201 --comp INPUT.spv
  --analysis OUTPUT-stats.csv --isa OUTPUT-isa.txt --livereg OUTPUT-live.txt -v
```

RGA's offline compiler requires the `.spv` extension; the byte-identical Slang
`.spirv` input initially failed and that log was retained. RGA returned exit code
zero for that failure. Verification therefore requires generated outputs and the
explicit successful-build marker, not the process exit code alone. No online
driver analysis or new GPU workload was launched for this investigation.
