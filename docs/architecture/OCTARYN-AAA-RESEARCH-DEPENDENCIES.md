# Octaryn AAA Research Dependencies And Output

## Current Native Dependency Inventory

These dependencies already exist or are planned through CMake wrapper aliases. Research should evaluate whether each is sufficient, whether it should remain hidden behind Octaryn APIs, and what gaps remain.

### Native Support And Core Utility Dependencies

| Alias | Version/Tag | Current Intended Use | Expected Owner Boundary |
| --- | --- | --- | --- |
| `octaryn::native_threads` | system Threads | native threading primitive for host-owned scheduler implementation | client/server/native support only |
| `octaryn::deps::spdlog` | `v1.17.0` | native logging | native logging support, client/server/tools |
| `octaryn::deps::cpptrace` | `v1.0.4` | stack traces and crash diagnostics | diagnostics/executable crash paths |
| `octaryn::deps::mimalloc` | `v3.3.1` | allocator backend | native memory support only |
| `octaryn::deps::tracy` | `v0.13.1` | profiling | client/server/tools/native support |
| `octaryn::deps::taskflow` | `v4.0.0` | job graph and worker execution | host-owned scheduler/native jobs |
| `octaryn::deps::eigen` | `5.0.0` | math/linear algebra | shared value math or owner-local optimized math |
| `octaryn::deps::unordered_dense` | `v4.8.1` | dense maps/sets | owner-local data structures |
| `octaryn::deps::glaze` | `v7.4.0` | JSON/metadata serialization | shared pure contracts, client settings, server persistence, basegame tools, root tools |
| `octaryn::deps::zlib` | `v1.3.2` | compression | server persistence, asset tools, support wrappers |
| `octaryn::deps::lz4` | `v1.10.0` | fast compression | server saves/caches, tools |
| `octaryn::deps::zstd` | `v1.5.7` | stronger compression | server saves/caches, tools |

### Client And Presentation Dependencies

| Alias | Version/Tag | Current Intended Use | Expected Owner Boundary |
| --- | --- | --- | --- |
| `octaryn::deps::sdl3` | `release-3.4.4` | windowing, input, SDL GPU, platform events | client only, isolated tools if needed |
| `octaryn::deps::sdl3_image` | `3.4.2` tarball | image loading | client UI/assets and asset import tools |
| `octaryn::deps::sdl3_ttf` | `release-3.2.2` | text rendering | client UI/overlays/tools |
| `octaryn::deps::imgui` | commit `285b38e2a7cfb2850ef27385f4e70df0f74f6b97` | immediate-mode debug/editor UI | client debug UI/tools |
| `octaryn::deps::implot` | commit `e6c36daf587b5eafebb533af1826b6d114b45421` | debug/profiling plots | client debug UI/tools |
| `octaryn::deps::implot3d` | commit `eb4ccd75f34b07646dfefb13b14f2df728bfd7ca` | debug 3D plots | client debug UI/tools |
| `octaryn::deps::imgui_node_editor` | commit `432c515535f4755c89235d58e71343c7c62ed317` | node/editor tooling | tools first, explicit client debug/editor only |
| `octaryn::deps::imguizmo` | commit `bbf06a1b0a1f18668acc6687ae283d6a12368271` | transform gizmos | tools first, explicit client debug/editor only |
| `octaryn::deps::imanim` | commit `51b78e795cf4d64f7d016d148b46a02e837e4023` | UI/editor animation helpers | tools first |
| `octaryn::deps::imfiledialog` | commit `c9819dd90450262efe7682839bb751c38173e1d8` | debug/editor file dialogs | tools/client debug only |
| `octaryn::deps::openal` | `1.25.1` | audio | client audio |
| `octaryn::deps::miniaudio` | `0.11.25` | audio helpers | client audio |
| `octaryn::deps::ozz_animation` | `0.16.0` | runtime skeletal animation | client animation runtime, basegame content data |

### Shader, Asset, And Tool Dependencies

| Alias | Version/Tag | Current Intended Use | Expected Owner Boundary |
| --- | --- | --- | --- |
| `octaryn::deps::shaderc` | `v2026.2` | shader compilation | shader tooling only |
| `octaryn::deps::shadercross` | commit `6b06e55c7c5d7e7a09a8a14f76e866dcfad5ab99` | SDL shader cross-compile path | shader tooling only |
| `octaryn::deps::spirv_tools` | `vulkan-sdk-1.4.341.0` | SPIR-V validation/optimization | shader tooling only |
| `octaryn::deps::spirv_cross` | `vulkan-sdk-1.4.341.0` | shader reflection/cross-compile | shader tooling only |
| `SPIRV-Headers` | `vulkan-sdk-1.4.341.0` | shader tool dependency | shader tooling only |
| `glslang` | `vulkan-sdk-1.4.341.0` | shader tool dependency | shader tooling only |
| `octaryn::deps::fastgltf` | `v0.9.0` | glTF import | asset import tools, client runtime only if intentional |
| `octaryn::deps::ktx` | `v4.4.2` | texture containers/GPU texture pipeline | asset tools and client texture loading |
| `octaryn::deps::meshoptimizer` | `v1.1.1` | mesh optimization | import processing tools, client intentional runtime |

## Current Managed Dependency Inventory

Pinned package versions:

| Package | Version | Current Policy |
| --- | --- | --- |
| `Arch` | `2.1.0` | approved module package for ECS; basegame/private owner-local; never shared public API |
| `Arch.LowLevel` | `1.1.5` | pinned transitive/support package; must be explicitly classified before module runtime use |
| `Arch.System` | `1.1.0` | approved module package for system update patterns |
| `Arch.System.SourceGenerator` | `2.1.0` | analyzer/private only where defining Arch systems |
| `Arch.EventBus` | `1.0.2` | approved module package for gameplay events, basegame/private owner-local |
| `Arch.Relationships` | `1.0.1` | approved module package for ECS relationships, basegame/private owner-local |
| `LiteNetLib` | `2.1.3` | host-only client/server transport; never basegame/shared/modules |
| `LiteEntitySystem` | `1.2.2` | host-only client/server replication/sync; never basegame/shared/modules |
| `Collections.Pooled` | `2.0.0-preview.27` | pinned support package; classify carefully before module use |
| `CommunityToolkit.HighPerformance` | `8.2.2` | pinned support package; classify carefully before module use |
| `Humanizer.Core` | `2.2.0` | pinned support package; classify carefully before module use |
| `Microsoft.Bcl.AsyncInterfaces` | `5.0.0` | pinned support package; classify carefully before module use |
| `Microsoft.CodeAnalysis.*` | `4.1.0` | analyzer/tooling support; not runtime module API |
| `Microsoft.CodeAnalysis.Analyzers` | `3.3.3` | analyzer/tooling support |
| `Microsoft.Extensions.ObjectPool` | `7.0.0` | pinned support package; classify carefully before module use |
| `Microsoft.NETCore.Platforms` | `1.1.0` | build/runtime metadata support |
| `NETStandard.Library` | `1.6.1` | compatibility/build support |
| `System.Composition.*` | `1.0.31` | composition/tooling support; deny broad module discovery unless explicitly approved |
| `ZeroAllocJobScheduler` | `1.1.2` | pinned support package; modules still cannot own threads/schedulers |

Current package policy:

- Allowed module direct packages: `Arch`, `Arch.EventBus`, `Arch.Relationships`, `Arch.System`, `Arch.System.SourceGenerator`.
- Allowed host direct packages: allowed module packages plus `LiteNetLib` and `LiteEntitySystem`.
- Host-only packages: `LiteNetLib`, `LiteEntitySystem`.
- `octaryn-shared` should stay package-free/BCL-only unless a contract-only dependency is deliberately approved.
- `Directory.Packages.props` pins versions only; it is not permission for module use.
- Any transitive package reachable from modules needs explicit allow/deny rules, owner, purpose, scope, and enforcement location.

## Capability Model To Preserve

Current and planned capabilities should stay explicit and deny-by-default. Research should expand this list where needed.

Known capability areas:

```text
content.blocks
content.items
content.entities
content.ui
gameplay.systems
gameplay.interactions
world.queries.read
world.blocks.edit.intent
entities.spawn.intent
inventory.mutate.intent
ui.contribute
input.actions
math.core
geometry.queries
random.deterministic
time.tick
diagnostics.module
physics.declare
physics.query
physics.intent
network.replicated_components
network.messages
persistence.components
native.systems
```

Research should identify missing capabilities for:

- fluids
- gases
- recipes
- loot
- tags
- worldgen
- biomes/features
- audio contributions
- animation contributions
- UI style/theme contributions
- localization
- assets
- data migrations
- save schema declarations
- debug surfaces
- editor tools
- tests/probes
- multiplayer compatibility declarations
- mod dependency declarations
- content override/extension rules
- server admin commands
- permission/security model

## Required Research Output Format

Produce a structured report with these sections:

1. Executive architecture summary.
2. Complete system inventory: every major engine/game/module/mod system that must be planned.
3. Owner map: client, server, shared, basegame, tools, cmake.
4. Library map: current libraries, missing libraries, recommended libraries, rejected libraries, and owner placement.
5. ECS backend design options and recommendation.
6. C# API ergonomics design, with examples.
7. Component model, entity model, item model, block model, and UI model.
8. Module/game/mod boundary model.
9. Capability and sandbox model.
10. Networking and replication model.
11. Physics model.
12. Persistence/save model.
13. UI system model for screen-space and world-space UI.
14. Input/action model.
15. World model, chunking, 512-block height, coordinate system, and world bounds.
16. Fluid/gas/liquid/block interaction model.
17. Asset/content pipeline model.
18. Tooling and editor model.
19. Build/package/dependency model.
20. Validation/probe model.
21. Performance targets and data-oriented design risks.
22. Security, mod trust, package enforcement, and binary inspection.
23. Milestones and migration phases.
24. Gaps, unresolved decisions, and high-risk unknowns.
25. Research references to inspect next.

For every system, include:

- owner
- source of truth
- public API surface
- native backend responsibility
- managed/frontend responsibility
- data formats
- capabilities needed
- validation required
- dependencies used
- risks
- open questions
- suggested milestone

For every proposed dependency, include:

- exact purpose
- owner placement
- whether it is module-facing or host-hidden
- why existing dependencies are insufficient
- alternatives compared
- license/risk questions to verify
- build/platform concerns
- validation required
- reason to reject it if it does not fit

