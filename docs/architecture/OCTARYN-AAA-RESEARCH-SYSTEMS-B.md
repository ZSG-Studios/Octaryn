# Octaryn AAA Research Systems B

### 15. World Generation And Terrain

Research:

- flat blank core world
- basegame worldgen modules
- chunk generation
- feature placement
- biome rules if needed
- deterministic RNG
- noise API
- content-driven generation
- server authority
- async generation jobs
- save interaction
- streaming
- world height 512
- vertical centering
- coordinate systems
- dimensions/world spaces if needed
- mod extension rules
- validation and profiling

### 16. Rendering And Client Presentation

Research:

- SDL3 GPU/Vulkan path
- render graph or frame graph needs
- shader pipeline
- shader variants
- material system
- texture atlas
- KTX texture flow
- mesh upload
- chunk mesh presentation
- fluid rendering
- entity rendering
- animation rendering
- particles
- UI rendering
- world-space UI
- debug rendering
- visibility/culling
- LOD
- asset streaming
- frame pacing
- profiling
- RenderDoc capture workflow

Do not work on DDGI/skylight until the dedicated plan exists. Research may identify future lighting plan topics only.

### 17. Assets, Content Pipeline, And Data

Research:

- content IDs
- data schema
- module asset declarations
- content validation
- block/item/entity JSON or binary schemas
- texture atlas build
- model import
- animation import
- glTF policy
- KTX texture policy
- meshoptimizer use
- shader compile pipeline
- basegame tools
- root tools
- generated source
- asset hashes
- package layout
- hot reload if allowed
- editor importers
- mod asset validation
- missing asset handling

Current asset/tool dependencies include fastgltf, KTX, meshoptimizer, shaderc, shadercross, SPIR-V tools, SPIRV-Cross, SDL3_image, and ozz-animation.

### 18. Audio

Research:

- OpenAL Soft role
- miniaudio role
- clip/streaming audio
- spatial audio
- mixer
- entity audio components
- UI audio
- basegame sound declarations
- mod audio assets
- attenuation
- occlusion if planned
- music/state transitions
- client-only playback
- server-authoritative sound events if needed

### 19. Animation

Research:

- ozz-animation role
- skeletal animation
- item/block animation
- entity animation state
- client-owned animation runtime
- basegame animation declarations
- mod assets
- animation events
- networking of animation state
- prediction/interpolation
- tooling/import pipeline

### 20. Diagnostics, Profiling, And Debugging

Research:

- structured logging
- module diagnostics API
- crash reports
- stack traces
- Tracy zones
- GPU captures
- performance counters
- validation reports
- module error reporting
- server console diagnostics
- client debug overlays
- debug UI boundaries
- logs under `logs/<owner>/`
- no module direct console/file logging unless approved

Current dependencies include spdlog, cpptrace, and Tracy.

### 21. Tools And Editors

Research:

- workspace control UI
- shader compiler
- atlas builder
- content validators
- module validators
- package policy validators
- save validators
- schema generators
- ECS/component codegen
- API docs generator
- editor/debug world tools
- performance capture wrappers
- package/mod build tooling
- external game template tooling

Basegame-specific tools stay in `octaryn-basegame/Tools/`. Repo-wide tools stay in root `tools/`.

### 22. Security, Sandbox, And Dependency Enforcement

Research:

- package allowlist
- transitive package validation
- framework API group allowlist
- denied namespaces/types/members
- binary inspection
- source inspection
- native interop denial
- reflection/dynamic loading denial
- filesystem/process/network/threading denial
- module trust levels
- signed packages
- server/client mod compatibility
- content hash validation
- manifest validation
- failure UX
- admin overrides if any

Default stance is deny-by-default.

### 23. Developer Experience And API Ergonomics

Research:

- fluent APIs
- builder patterns
- source generation
- analyzer errors
- excellent error messages
- component declaration examples
- system declaration examples
- UI examples
- physics examples
- networking examples
- item/block/entity examples
- mod templates
- documentation generation
- examples that do not leak internals
- API naming conventions
- capability request ergonomics
- debugging tools for mod authors

The API should feel elegant, powerful, and safer than typical game scripting APIs while still mapping to fast native systems.

### 24. Performance And Data-Oriented Design

Research:

- native backend layout
- ECS memory layout
- cache efficiency
- job scheduling overhead
- bridge overhead
- serialization overhead
- networking bandwidth
- chunk streaming
- mesh generation cost
- physics cost
- fluid/gas simulation cost
- UI update cost
- profiling targets
- benchmarks
- failure thresholds
- debug vs release build behavior

### 25. Build, Packaging, And Platform Support

Research:

- owner-partitioned build output
- owner bundles
- native archive format
- .NET managed outputs
- hostfxr bridge packaging
- Linux toolchain
- Windows cross toolchain
- dependency source cache
- third-party download policy
- reproducibility
- CI-ready commands later
- package layout
- distribution layout
- mod package layout
- game package layout
- logs and crash dumps per owner

### 26. Multiplayer And Mod Compatibility

Research:

- multiplayer compatibility manifests
- required/optional mods
- server-enforced module lists
- client-only UI/cosmetic mods
- server-only admin modules
- version ranges
- content ID conflicts
- content override policy
- save compatibility
- network protocol compatibility
- capability mismatches
- asset mismatches
- downgrade/upgrade policy

### 27. Future DDGI/Lighting Plan Placeholder

Research may list future questions for a dedicated DDGI/lighting plan:

- DDGI probe volumes
- skylight representation
- voxel GI inputs
- block opacity metadata
- client/server split
- content-driven material/opacity declarations
- performance capture plan
- migration from old CPU skylight

But do not include active DDGI/skylight implementation tasks in the current AAA plan.

## Research Questions To Answer

Answer these directly:

1. Are the current native and managed libraries enough for an AAA-quality modular voxel platform?
2. Which missing libraries should be considered, and which should be avoided?
3. Should Octaryn use custom native ECS storage, EnTT, Flecs, or another ECS backend while preserving C# authoring through explicit APIs?
4. What physics backend is best for this architecture, and how should the API hide it?
5. What UI layout/text stack is needed for screen-space and world-space UI?
6. Is SDL3_ttf enough for product UI, or is text shaping/font fallback/localization support missing?
7. What networking model should sit above LiteNetLib/LiteEntitySystem without exposing them?
8. What save format should support voxel chunks, entities, module data, migration, compression, and corruption safety?
9. How should basegame, games, and mods define custom components and systems without exposing unsafe internals?
10. How should native C++ systems be allowed for performance while keeping modules capability-scoped and safe?
11. What validations must exist before external mods can run?
12. What data formats should be JSON, binary, generated source, or hybrid?
13. What editor/tools should exist before content scale grows?
14. What system boundaries are most likely to rot into a monolith if not planned now?
15. What is the staged migration plan from current code to the full target architecture?

## Libraries To Specifically Evaluate As Missing Or Optional

Research these only as candidates. Do not assume they should be added.

Native ECS:

- EnTT
- Flecs
- custom Octaryn ECS

Physics:

- Jolt
- PhysX
- Bullet
- ReactPhysics3D
- custom voxel collision plus narrow physics backend

UI/text/layout:

- HarfBuzz
- FreeType policy beyond SDL3_ttf
- Yoga
- Taffy
- RmlUi
- Nuklear
- Dear ImGui only for debug/tools
- custom retained UI

Navigation/AI:

- Recast/Detour
- custom voxel navigation

Serialization/schema:

- FlatBuffers
- Cap'n Proto
- Protobuf
- custom binary schema plus glaze JSON metadata

Networking:

- keeping LiteNetLib/LiteEntitySystem host-hidden
- custom protocol over LiteNetLib
- other transports only if justified

Audio:

- OpenAL Soft plus miniaudio
- whether one should be removed or one should be the clear backend

Asset pipeline:

- Basis Universal/KTX path
- glTF pipeline with fastgltf
- ozz-animation for runtime animation
- meshoptimizer for mesh processing

Any dependency recommendation must include owner placement, module exposure decision, license/risk checks, platform/build implications, and validation.

## Hard No List

Reject proposals that require:

- a new top-level `engine/` folder
- a new top-level `runtime/` folder
- `Octaryn.Engine.*` namespaces
- broad `common`, `helpers`, `misc`, or catch-all modules
- basegame reaching into client/server/native internals
- modules reaching into host internals
- server rendering/UI ownership
- client authority over persistence/simulation
- shared owning implementation
- raw ECS storage access from modules
- raw physics backend access from modules
- raw networking/transport access from modules
- module-owned threads, schedulers, timers, sockets, file watchers, or native loops
- unapproved NuGet packages in modules
- unapproved framework API groups in modules
- module-facing third-party backend types
- DDGI/skylight implementation work before a dedicated plan exists

## Expected Final Report Shape

Return a concise but comprehensive AAA architecture research plan. Prefer tables and owner maps. The report should make it easy to convert into Octaryn docs and implementation milestones.

End with:

- a full missing-system checklist
- a full dependency decision checklist
- a full validation checklist
- a full API surface checklist
- a staged roadmap
- unresolved questions that require user decisions
