# DX12 through standalone Slang RHI

Updated 2026-09-13. User explicitly requested switching the active client to DX12.
This supersedes the Vulkan device selection in earlier renderer reports.

The dependency build enables D3D12 alongside Vulkan, links D3D12MemoryAllocator
and Windows graphics libraries, and stages DXC/dxil DLLs with the client. The
active world device selects D3D12 and shader model 6.8. Vulkan remains compiled
into the RHI dependency but is not the selected world device.

Active shaders now use portable vertex/instance semantics. The world vertex
shader adds the start-instance offset to the local instance ID, preserving its
indirect draw indexing. RmlUi varyings have explicit DXC-compatible semantics.
HDR scene and display-color targets receive first-use render-target clears before
compute writes and later render-target loads, as required by D3D12 validation.
Per-slot initialization resets on resize and becomes valid after submission.

A pinned RHI patch reuses identical complete sampler descriptor tables within
each command buffer. This fixes exhaustion of DX12's 2048-entry sampler heap
as per-column terrain draws accumulate. Failed arena allocations also return
without retaining invalid chunks. The patch is registered in the dependency
patch application script and survives clean dependency rebuilds.

Validation:

- Dependency and client bundle builds passed; logs/client/dx12-dependency-build.log
  and logs/client/dx12-client-build.log.
- DXIL compilation checked world, batch, player, sky, clouds, selection, RmlUi and
  HDR entries. World/batch vertex SPIR-V compilation also passed; this is not a
  new Vulkan runtime qualification.
- Packaged D3D12 runtime on AMD Radeon RX 9070 XT: 600 frames, 81 columns,
  320431 quads, 2560x1440, third-person player and HUD, clean exit. RmlUi checks:
  573 document/layout and 1175 inventory/menu checks, no GPU validation warnings
  or errors. Evidence: logs/client/validation/dx12/final/hud/client.log and frame.bmp.
- Screenshot inspected for terrain, player and HUD presentation.
- After the sampler fix, full radius 32 completed with exit code 0, 600 frames,
  4225 columns, 16481899 quads and 826868796 GPU bytes. The runtime diagnostic
  counters passed with no graphics validation warnings or errors. Evidence:
  logs/client/validation/dx12/full-distance/hud/client.log.
  The combined UI runner failed its screenshot requirement because no capture
  was emitted before frame-limit exit; full-distance visual verification is
  therefore incomplete. Document and inventory contracts passed.
- CPU draw-preparation target passed, including 140 draw checks, 3356 halo
  invalidation checks and 12006 camera-culling checks. No GPU device was used.

The existing batch capability gate rejects D3D12's advertised capabilities, so
this switch uses the existing per-column indirect drawing path. It does not
establish performance parity with Vulkan's bindless batching. Settled-world
DX12 performance, all material cases and long sessions remain unqualified.

Rebuild with tools/build/slang-rhi.ps1 followed by
tools/build/windows.ps1 -Action build -Target octaryn_client_bundle.
