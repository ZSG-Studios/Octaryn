# Third-Party Notices

This documentation site vendors a small PrismJS subset for local syntax highlighting.

## PrismJS

- Package: `prismjs`
- Version: `1.30.0`
- Repository: <https://github.com/PrismJS/prism>
- License: MIT
- Vendored files:
  - `docs/assets/vendor/prism/prism-core.min.js`
  - `docs/assets/vendor/prism/prism-clike.min.js`
  - `docs/assets/vendor/prism/prism-csharp.min.js`
  - `docs/assets/vendor/prism/LICENSE`

## Texture Pack Sources

No third-party texture pack image assets are currently vendored in this repository.

The basegame atlas builder can import Minecraft-style resource packs. Generated atlas assets must keep release metadata for the exact pack source, pack version or commit, SHA-256, license, required credit, and required license file.

### Classic Faithful 32x Jappa

- Status: default import source for `octaryn-basegame/Tools/build_atlas_from_pack.py` when `--pack` is omitted.
- Repository: <https://github.com/ClassicFaithful/Classic-32x-Jappa-Java>
- Website: <https://faithfulpack.net/>
- Credits: <https://github.com/ClassicFaithful/Classic-32x-Jappa-Java/blob/main/credits.txt>
- License: Faithful License Version 3, <https://faithfulpack.net/license>
- Release rule: generated assets containing this work must include clear credit, link back to Faithful, avoid monetization of content containing the work, avoid implying official Faithful status, and include the unmodified Faithful license file with any distributed content that contains the work.

Release packages that include generated atlas assets from this pack must include:

- `texture_pack_*` metadata in the atlas manifest.
- Faithful license and credit files under `THIRD_PARTY/ClassicFaithful/`.
- A package-level `THIRD_PARTY_NOTICES.txt` that keeps the texture-pack obligations visible.

## Runtime Library Sources

Configured source dependencies are fetched into generated build caches such as `build/dependencies/`. They are not source-vendored in this repository, but source archives, binary releases, and generated release packages must keep the matching upstream license and notice files.

Core native dependencies:

- spdlog `v1.17.0`, <https://github.com/gabime/spdlog>
- cpptrace `v1.0.4`, <https://github.com/jeremy-rifkin/cpptrace>
- mimalloc `v3.3.1`, <https://github.com/microsoft/mimalloc>
- Tracy `v0.13.1`, <https://github.com/wolfpld/tracy>
- Taskflow `v4.0.0`, <https://github.com/taskflow/taskflow>
- Eigen `5.0.0`, <https://gitlab.com/libeigen/eigen>
- ankerl::unordered_dense `v4.8.1`, <https://github.com/martinus/unordered_dense>
- zlib `v1.3.2`, <https://github.com/madler/zlib>
- LZ4 `v1.10.0`, <https://github.com/lz4/lz4>
- Zstandard `v1.5.7`, <https://github.com/facebook/zstd>

Client, rendering, audio, and UI dependencies:

- SDL3 `release-3.4.4`, <https://github.com/libsdl-org/SDL>
- OpenAL Soft `1.25.1`, <https://github.com/kcat/openal-soft>
- miniaudio `0.11.25`, <https://github.com/mackron/miniaudio>
- glaze `v7.4.0`, <https://github.com/stephenberry/glaze>
- SDL3_image `release-3.4.2`, <https://github.com/libsdl-org/SDL_image>
- SDL3_ttf `release-3.2.2`, <https://github.com/libsdl-org/SDL_ttf>
- Dear ImGui, <https://github.com/pthom/imgui>
- ImPlot, <https://github.com/pthom/implot>
- ImPlot3D, <https://github.com/pthom/implot3d>
- imgui-node-editor, <https://github.com/pthom/imgui-node-editor>
- ImGuizmo, <https://github.com/pthom/ImGuizmo>
- ImAnim, <https://github.com/pthom/ImAnim>
- ImFileDialog, <https://github.com/pthom/ImFileDialog>
- ozz-animation `0.16.0`, <https://github.com/guillaumeblanc/ozz-animation>

Slang shader, texture, and asset tooling dependencies:

- Slang and Slang RHI, <https://github.com/shader-slang/slang> and <https://github.com/shader-slang/slang-rhi>
- fastgltf `v0.9.0`, <https://github.com/spnda/fastgltf>
- KTX-Software `v4.4.2`, <https://github.com/KhronosGroup/KTX-Software>
- meshoptimizer `v1.1.1`, <https://github.com/zeux/meshoptimizer>

Additional contained runtime dependencies:

- FastNoise2, <https://github.com/Auburn/FastNoise2>
- Jolt Physics, <https://github.com/jrouwe/JoltPhysics>

## .NET Package Sources

Package pins in `Directory.Packages.props` are tracked here even when a package is build-only or analyzer-only. A pin is not permission for module use; module-facing package access is controlled by the architecture allowlist.

- Arch `2.1.0`
- Arch.LowLevel `1.1.5`
- Arch.System `1.1.0`
- Arch.System.SourceGenerator `2.1.0`
- Arch.EventBus `1.0.2`
- Arch.Relationships `1.0.1`
- Collections.Pooled `2.0.0-preview.27`
- CommunityToolkit.HighPerformance `8.2.2`
- Humanizer.Core `2.2.0`
- Microsoft.Bcl.AsyncInterfaces `5.0.0`
- Microsoft.CodeAnalysis.Analyzers `3.3.3`
- Microsoft.CodeAnalysis.Common `4.1.0`
- Microsoft.CodeAnalysis.CSharp `4.1.0`
- Microsoft.CodeAnalysis.CSharp.Workspaces `4.1.0`
- Microsoft.CodeAnalysis.Workspaces.Common `4.1.0`
- Microsoft.Extensions.ObjectPool `7.0.0`
- Microsoft.NETCore.Platforms `1.1.0`
- NETStandard.Library `1.6.1`
- System.Composition `1.0.31`
- System.Composition.AttributedModel `1.0.31`
- System.Composition.Convention `1.0.31`
- System.Composition.Hosting `1.0.31`
- System.Composition.Runtime `1.0.31`
- System.Composition.TypedParts `1.0.31`
- ZeroAllocJobScheduler `1.1.2`
- LiteNetLib `2.1.3`
- LiteEntitySystem `1.2.2`

Packaged runtime payloads may also include transitive NuGet assemblies and package-local notices. The current Linux shareable runtime package includes:

- Microsoft .NET `nethost`
- Arch `2.1.0`
- Arch.EventBus `1.0.2`
- Arch.LowLevel `1.1.5`
- Arch.Relationships `1.0.1`
- Arch.System `1.1.0`
- Collections.Pooled `2.0.0-preview.27`
- CommunityToolkit.HighPerformance `8.2.2`
- K4os.Compression.LZ4 `1.3.8`
- LiteEntitySystem `1.2.2`
- LiteNetLib `2.1.3`
- Microsoft.Extensions.ObjectPool `7.0.0`
- ZeroAllocJobScheduler `1.1.2`
- RefMagic, from LiteEntitySystem
- Schedulers, from ZeroAllocJobScheduler

## Packaged Runtime Payloads

Release packages that bundle binaries must include a package-level `THIRD_PARTY_NOTICES.txt` and license files under `THIRD_PARTY/` for the exact files shipped.

Linux release runtime libraries:

- `libSDL3.so.0`
- `libSDL3_image.so.0`
- `libmimalloc-debug.so.3`
- `libopenal.so.1`
- `libminiaudio.so`
- `libFastNoiseD.so`
- `libJolt.so`
- `libozz_animation.so`
- `libozz_base.so`
- `libz.so.1`
- `libzstd.so.1`
- `libnethost.so`

Windows release runtime libraries:

- `libgcc_s_seh-1.dll`
- `libstdc++-6.dll`
- `libwinpthread-1.dll`
- `libzd.dll`
