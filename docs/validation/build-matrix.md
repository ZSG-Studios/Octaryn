# Build and runtime qualification matrix

Current cleanup qualification on 2026-09-14:

| Platform | Build / logic | Rendering / runtime |
| --- | --- | --- |
| Windows x64 / DX12 | Native bundle, static checks and CPU aggregate pass. | Fresh 600-frame Native AA runs pass at radius 4 and 32 on Radeon RX 9070 XT. |
| Windows x64 / Vulkan | Same native bundle with explicit backend selection. | Fresh 600-frame Native AA run passes at radius 4 on the same hardware. |
| Linux x64 / Vulkan | Native release bundle, static checks and CPU aggregate pass on Fedora 44 under WSL2, including 35,529 fluid checks. | Relocated 600-frame Native AA run passes on software llvmpipe; hardware Vulkan remains unqualified. |
| macOS / Metal | Native dependency integration and Metal source emission. | Native application/FSR execution unqualified. |
| ARM64 | Architecture branches exist. | No general runtime support claim. |

## Fresh Windows rendering checks

All three isolated runs used 1280×720, completed 600 frames, exited 0 and recorded
empty warning/error/failure lists. Their actual screenshots were inspected.
Both radius-4 Native AA cases retained 81 columns and 320,431 quads. The DX12
radius-32 case retained 4,225 columns, 15,751,869 quads and 671,628,875 tracked GPU
bytes. Its complete test took 246.141 seconds including loading and qualification;
this is **not an FPS benchmark**.

Result manifests are local evidence under
`logs/client/validation/clean-release-20260914/{dx12,vulkan,dx12-radius32}/result.json`.
They record the executable hash, settings, capture hash, command, timings and
counts. These checks qualify the tested cleaned bundle and hardware, not every
GPU, sustained travel, complete gameplay parity or an as-yet unbuilt final archive.

Use the [native build guide](../build/README.md) and
[validation groups](README.md). Build, CPU logic, shader compilation, actual GPU
execution and final extracted-package verification remain separate gates.

The completed static/CPU aggregate logs are local evidence at
`logs/tools/windows-cpu-complete.log` and
`logs/tools/linux/octaryn-linux-checks-final.log`; both runs exited 0.

The Linux relocated software run also passes: 600 Native AA frames, 81 columns,
320,431 quads and 219,803,099 tracked GPU bytes, with no warnings/errors and an
inspected screenshot. Evidence is under
`logs/client/validation/clean-release-20260914/linux-llvmpipe-v2/`.

This is experimental Fedora 44/WSL2 **software llvmpipe** qualification, not a
hardware-performance or general Linux support claim. Dozen hardware execution
remains unqualified because the required `VK_KHR_external_memory_capabilities`
instance extension is unavailable. The built Linux binary requires glibc 2.43+
and GLIBCXX_3.4.35, verified using ELF symbol versions.

All Windows GPU constituent targets passed. The initial aggregate timeout is
retained in the evidence; the corrected remaining targets were rerun and exited
0. This is not a claim that the original aggregate invocation passed.
