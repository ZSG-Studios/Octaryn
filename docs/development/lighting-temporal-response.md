# Temporal response to moving lights and shadows

The local-light filter and sun-shadow filter now compare a short history with
current illumination on matching neighboring receiver surfaces. Coherent changes
reduce temporal reuse even when the receiver geometry is stationary. Variance
sets the confidence tolerance so isolated stochastic samples can still accumulate.
RGB confidence also detects light-color changes with similar luminance.

The existing minimum/maximum clipping remains. It alone cannot reject a moving
shadow when neighboring pixels retain the entire bright/dark interval. The new
confidence scales history weight and effective age, while a two-frame fast
history follows the new signal. Invalid reprojection and explicit resets use the
current sample immediately.

This is original Slang code through standalone Slang RHI. The design uses the
short-history and confidence concepts described by
[NRD](https://github.com/NVIDIA-RTX/NRD/blob/master/README.md) and the statistical
temporal filtering principles in
[SVGF](https://research.nvidia.com/sites/default/files/pubs/2017-07_Spatiotemporal-Variance-Guided-Filtering%3A/svgf_preprint.pdf).
It does not integrate or claim parity with either implementation. The local
filter consumes raw radiance independently of the local-light sampling algorithm.

## Production shader validation

`octaryn_client_world_mesh_probe --lighting-temporal-only` runs the actual
`Lighting/DenoiseTemporal.slang` and `Shadows/Temporal.slang` compute entrypoints
against uploaded G-buffer/history fixtures, then reads their output textures.

DX12 and Vulkan on the RX 9070 XT both passed eleven 16-by-16 cases: six local
radiance cases and five sun visibility cases. Logs are
`logs/client/lighting-temporal-dx12.log` and
`logs/client/lighting-temporal-vulkan.log`. Both runs reported no graphics
validation warnings or errors. The two production shaders also compile to DXIL
and SPIR-V with the pinned Slang compiler.

- A newly dark pixel with a bright neighbor responds below 0.1 on its first frame.
- A newly bright pixel with a dark neighbor responds above 0.9 on its first frame.
- The neighbors deliberately preserve the [0,1] range that defeated clipping alone.
- Similar-luminance red-to-green local lighting drops stale red and retains green.
- Disoccluded receivers and explicitly reset histories return the current sample.
- Stable alternating 0.2/0.8 samples retain mature 0.5 history. Interior MSE is
  0.000352 for local lighting and 0.000088 for sun shadows, against raw MSE 0.09.

The MSE fixture establishes that adaptive response preserves stationary averaging;
it is not a live-scene convergence, visual-quality, or performance benchmark.
Integrated moving-camera, digging, and moving-light captures remain separate
qualification recorded by the lighting repair report.

## Resource cost

The local filter adds two RGBA16Float fast-history textures, totaling 16 bytes per
active render pixel. Sun history changes from two RG32Float textures to two
RGBA32Float textures, adding 16 bytes per allocation pixel. When both extents are
1280 by 720, the total increment is 28.125 MiB; at 1920 by 1080 it is 63.28125 MiB.
Dynamic-resolution allocation and active extents can differ.

There are no additional dispatches. Both temporal passes add matching-neighbor
history reads. The existing local and sun-filter GPU profiling scopes include
this cost; an integrated before/after GPU measurement is required to quantify it.
