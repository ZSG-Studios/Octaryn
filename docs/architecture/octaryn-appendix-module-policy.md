# Octaryn Appendix: Module Policy

## Game Module Loading And Compatibility

Octaryn should behave like a host for game modules. `octaryn-basegame` is just the bundled default module, not a privileged place for core host logic. Future games, mods, or content modules should follow the same contract shape.

Shared contracts define:

- Module identity: stable module ID, display name, version, API version, and dependency ranges.
- Required capabilities: client presentation features, server authority features, content systems, asset kinds, networking needs, and optional tool requirements.
- API exposure: a manifest-declared list of host API IDs the module requests, with each API granted or rejected by compatibility validation.
- Package exposure: manifest-declared lists of runtime packages, build/analyzer packages, and framework API groups the module requests, checked against allowlists before load.
- Registrations: blocks, items, materials, recipes, tags, loot, features, biomes, interactions, commands, snapshots, and game rules through API-owned registries.
- Compatibility declarations: supported host/API versions, required modules, incompatible modules, multiplayer compatibility, save compatibility, and asset compatibility.
- Validation reports: structured errors and warnings that tools, client, and server can show without loading unsafe gameplay code.

Sandbox and API exposure policy:

- Module APIs are least-privilege and capability-scoped. A module only receives APIs accepted from its manifest.
- In-process modules are an API isolation boundary, not a security boundary for hostile code. Truly untrusted code must run out-of-process under a separate module host or be rejected.
- Modules must not access raw client/server internals, native pointers, persistence backends, transport sockets, GPU resources, process state, threads, filesystem paths, reflection, dynamic assembly loading, unmanaged interop, or network endpoints except through explicit host APIs.
- C# ECS and host networking implementations may cross into C/C++ only through explicit owner host bridges. Modules still see capability-scoped shared APIs, not bridge internals, native pointers, transport packages, or interop surfaces.
- Host APIs expose stable IDs, immutable value types, validated commands, queries, snapshots, registries, events, and bounded resource handles instead of mutable implementation objects.
- Host scheduling APIs expose bounded scheduled work scopes instead of direct threads. Modules request work through the host, and the coordinator places eligible logic on the worker pool.
- Server APIs own authoritative simulation, saves, validation, world edits, and multiplayer state. Client APIs own presentation, input, audio, UI, prediction views, and assets.
- Tools may validate and package module assets offline, but tool access does not imply runtime access.
- Any new host capability required by a module must be added as a small shared contract and implemented by the correct owner before modules can request it.

Enforcement points:

- Shared contracts currently define `GameModuleManifest`, host API ID constants, module capability ID constants, runtime/build package allowlists, framework API group allowlists, denied sandbox group IDs, `ModuleValidationReport`, timing/input-only host frame contracts, and narrow host request command contracts. Typed request records can replace raw manifest string lists once external module packaging needs version ranges and richer diagnostics.
- `octaryn-shared/Source/ApiExposure/` owns API exposure contract shapes and capability names. It does not grant permissions by itself.
- `octaryn-shared/Source/FrameworkAllowlist/` owns allowlist contract shapes for package IDs and framework API groups. It does not scan assemblies by itself.
- `octaryn-shared/Source/ModuleSandbox/` owns sandbox policy contracts and validation result types. It does not contain client/server enforcement logic.
- Client/server/tool hosts currently enforce the contracts with MSBuild/package checks, resolved asset graph checks, source scans, post-build binary metadata checks, manifest validation, and pre-load activation checks. Richer artifact identity binding remains Phase 0 enforcement work before external binary-only modules are trusted.
- Runtime access must go through capability handles supplied by the activating host. Modules must not discover services by scanning assemblies, globals, or implementation namespaces.

.NET package allowlist policy:

`octaryn-shared` must stay package-free or BCL-only unless a contract-only dependency is deliberately approved in this plan. Module-facing contracts must use Octaryn-owned value types and interfaces. Do not expose third-party package types in shared public APIs.

| Package or API group | Allowed owners | Purpose | Version policy | Runtime scope | Validation rule | Enforcement location |
| --- | --- | --- | --- | --- | --- | --- |
| `Arch` | `octaryn-basegame`, approved full games/game modules/mods, `octaryn-client`, `octaryn-server` | Intended managed ECS for gameplay, basegame, modules, mods, and approved owner-local managed worlds. | Exact central pin in `Directory.Packages.props`. | Module implementation and client/server host integration only. | Allowed package ID and version must match the pin; no public shared API types; host/native backends meet Arch-managed worlds through Octaryn descriptors and validators. | Module/host package validation and project restore checks. |
| `Arch.LowLevel` | Transitive package for approved Arch runtime packages. | Low-level Arch runtime support. | Exact central pin. | Transitive module runtime only. | May not be referenced directly by module code unless promoted to an explicit approved direct package. | Resolved package validation. |
| `Arch.System` | `octaryn-basegame`, approved full games/game modules/mods, `octaryn-client`, `octaryn-server` | Intended managed ECS system authoring/execution support, driven by Octaryn host scheduling declarations. | Exact central pin. | Module implementation and client/server host integration only. | Allowed package ID and version must match the pin; no public shared API types; systems must declare Octaryn phases and read/write sets. | Module/host package validation and project restore checks. |
| `Arch.System.SourceGenerator` | `octaryn-basegame`, approved full games/game modules/mods, `octaryn-client`, `octaryn-server` | Compile-time ECS system generation. | Exact central pin. | Build/analyzer only. | Must be listed as a requested build package and use `PrivateAssets=\"all\"`, `OutputItemType=\"Analyzer\"`, and `IncludeAssets=\"analyzers;build;buildTransitive\"`. | MSBuild/package validation and resolved asset validation. |
| `Arch.EventBus` | `octaryn-basegame`, approved full games/game modules/mods, `octaryn-client`, `octaryn-server` | Gameplay and host integration events. | Exact central pin. | Module implementation and client/server host integration only. | Allowed package ID and version must match the pin; no public shared API types. | Module/host package validation and project restore checks. |
| `Arch.Relationships` | `octaryn-basegame`, approved full games/game modules/mods, `octaryn-client`, `octaryn-server` | Gameplay and host entity relationships. | Exact central pin. | Module implementation and client/server host integration only. | Allowed package ID and version must match the pin; no public shared API types. | Module/host package validation and project restore checks. |
| `Collections.Pooled` | Transitive package for approved Arch runtime packages. | Pooled collection implementation used by Arch. | Exact central pin. | Transitive module runtime only. | May not be referenced directly by module code unless promoted to an explicit approved direct package. | Resolved package validation. |
| `CommunityToolkit.HighPerformance` | Transitive package for approved Arch runtime packages. | High-performance memory/collection primitives used by Arch. | Exact central pin. | Transitive module runtime only. | May not be referenced directly by module code unless promoted to an explicit approved direct package. | Resolved package validation. |
| `Microsoft.Extensions.ObjectPool` | Transitive package for approved Arch runtime packages. | Object pooling used by Arch package graph. | Exact central pin. | Transitive module runtime only. | May not be referenced directly by module code unless promoted to an explicit approved direct package. | Resolved package validation. |
| `ZeroAllocJobScheduler` | Transitive package for approved Arch runtime packages. | Scheduler support used by Arch.System. | Exact central pin. | Transitive module runtime only. | May not be referenced directly by module code unless promoted to an explicit approved direct package. | Resolved package validation plus source and binary API denial for `Schedulers.*`. |
| `Humanizer.Core` | Transitive build package for approved source generators. | Source-generator support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `Microsoft.Bcl.AsyncInterfaces` | Transitive build package for approved source generators. | Source-generator support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `Microsoft.CodeAnalysis.Analyzers` | Transitive build package for approved source generators. | Analyzer support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `Microsoft.CodeAnalysis.Common` | Transitive build package for approved source generators. | Compiler API support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `Microsoft.CodeAnalysis.CSharp` | Transitive build package for approved source generators. | C# compiler API support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `Microsoft.CodeAnalysis.CSharp.Workspaces` | Transitive build package for approved source generators. | Workspace support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `Microsoft.CodeAnalysis.Workspaces.Common` | Transitive build package for approved source generators. | Workspace support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `Microsoft.NETCore.Platforms` | Transitive build package for approved source generators. | Platform metadata support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `NETStandard.Library` | Transitive build package for approved source generators. | Reference assembly support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `System.Composition` | Transitive build package for approved source generators. | Composition support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `System.Composition.AttributedModel` | Transitive build package for approved source generators. | Composition support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `System.Composition.Convention` | Transitive build package for approved source generators. | Composition support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `System.Composition.Hosting` | Transitive build package for approved source generators. | Composition support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `System.Composition.Runtime` | Transitive build package for approved source generators. | Composition support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| `System.Composition.TypedParts` | Transitive build package for approved source generators. | Composition support. | Exact central pin. | Build graph only. | Must be reachable from approved build packages. | Resolved package validation. |
| Safe BCL value APIs | `octaryn-shared`, `octaryn-basegame`, approved full games/game modules/mods | Primitives, collections, spans/memory, math/numerics, dates/times, text, and diagnostics abstractions. | Runtime-provided .NET 10 APIs only. | Shared contracts and module implementation. | Allowed namespace/API group only; no filesystem, network, reflection, process, environment, or threading APIs. | Analyzer/source scan and pre-load validation. |
| Host-routed JSON/data parsing | `octaryn-basegame`, approved full games/game modules/mods, tools | Content data parsing through approved host APIs. | Runtime-provided .NET 10 APIs unless a package is explicitly approved. | Offline tools or bounded host API runtime path. | Module cannot open arbitrary paths; host supplies bounded data streams/handles. | Tool validation and host capability checks. |
| `LiteNetLib` | `octaryn-client`, `octaryn-server` | Reliable UDP transport. | Exact central pin. | Host transport only. | Rejected in `octaryn-shared`, `octaryn-basegame`, game modules, and mods. | Package validation and owner project checks. |
| `LiteEntitySystem` | `octaryn-client`, `octaryn-server` | Host-side entity replication/synchronization backend behind Octaryn networking contracts. | Exact central pin. | Host implementation only. | Rejected in `octaryn-shared`, `octaryn-basegame`, game modules, and mods; public entities, RPCs, SyncVars, and replication declarations must be Octaryn API shapes. | Package validation and owner project checks. |

Denied to modules by default: `System.IO`, raw filesystem paths, `System.Net`, sockets, HTTP clients, `System.Diagnostics.Process`, unmanaged interop, unsafe native bridges, reflection/dynamic loading, runtime code generation, arbitrary threading/task scheduling, timers, custom worker pools, environment variables, direct host service discovery, direct console/stdout/stderr writes, and unlisted NuGet packages. If a game or mod needs a new package or framework API group, add it here with owner, purpose, version policy, allowed runtime scope, validation rule, and enforcement location before using it.

Package validation currently has two layers:

- MSBuild item validation catches direct package misuse, host-only package misuse, and analyzer metadata mistakes.
- `tools/validation/validate_module_manifest_packages.py` checks that module manifest requested package lists match the module project’s direct runtime/build `PackageReference`s.
- `tools/validation/validate_module_manifest_files.py` checks that module manifest content/assets point at real module files and that non-placeholder `Data/`, `Assets/`, and `Shaders/` files are declared.
- `octaryn-basegame/Data/Module/octaryn.basegame.module.json` is the checked-in bundled module package descriptor. It mirrors `BasegameModuleRegistration.Manifest`, is copied to client/server bundles, and is compared by `tools/validation/Octaryn.ModuleManifestProbe/` so package metadata can become the future discovery source without replacing the current in-process registration path yet.
- `tools/validation/Octaryn.ModuleManifestProbe/` writes the generated validation manifest under `build/<preset>/basegame/generated/octaryn.basegame.manifest.json` for CMake/manual validation and compares the checked-in package descriptor with the code manifest.
- `tools/validation/validate_all_project_reference_boundaries.py` discovers every active `.csproj` under shared, client, server, basegame, tools, games, modules, and mods, then applies `validate_project_reference_boundaries.py` so new projects cannot bypass owner-reference rules. Modules may reference shared contracts only.
- `tools/validation/validate_dotnet_package_assets.py` is owner-aware. It parses module and host `project.assets.json` target graphs, rejects unclassified direct, runtime-transitive, build-direct, and build-transitive packages for modules, and rejects unclassified or unapproved direct packages in client/server host graphs. Shared and old-architecture projects stay outside resolved-package validation unless a future policy explicitly opts them in.
- `tools/validation/Octaryn.ModuleApiProbe/` scans module C# source with Roslyn before compile and rejects denied filesystem, networking, process, reflection, native interop, console, environment, dynamic-loading, raw-threading APIs, transitive scheduler APIs, and unapproved shared networking contracts. Its denied groups must stay aligned with `DeniedFrameworkApiGroups`.
- `tools/validation/validate_module_layout.py` checks module `Data/`, `Assets/`, and `Shaders/` layout, rejects unsupported file suffixes, duplicate normalized file IDs, and empty content/asset/shader files.
- `tools/validation/validate_package_policy_sync.py` keeps `tools/package-policy/module-packages.json`, `ModulePackagePolicy.props`, `Directory.Packages.props`, and shared package allowlist constants synchronized.
- `tools/package-policy/module-packages.json` is the machine-readable package policy source used by the asset graph validator. MSBuild and shared constants must stay aligned with it until those surfaces are generated from the policy file.

External full game projects should live under `octaryn-games/`, external module projects under `octaryn-modules/`, and external mod projects under `octaryn-mods/`, or set an owner through an early-imported props file before the root `Directory.Build.props` is evaluated. Setting `OctarynModuleOwner` only in the `.csproj` body is too late for output routing.

Client/server hosts own activation:

- The server validates authoritative content, gameplay rules, save compatibility, command shapes, snapshot shapes, and multiplayer compatibility before simulation starts.
- The current server project validates the bundled basegame manifest and server compatibility before returning from `ServerHost.Run`; deeper authoritative content/save/multiplayer validation is still a porting task under `octaryn-server/Source/Validation/`.
- The client validates assets, presentation capabilities, UI/overlay declarations, local prediction hooks, and client-compatible snapshot views before presenting a module.
- Tools validate content and assets offline before packaging a game module.
- Basegame and future modules register high-level content and mechanics only after the host accepts their manifest.

No module may bypass shared registries or write directly into client/server/core voxel internals. If a gameplay feature needs a new host capability, add a small shared API contract and implement it in the correct client or server owner.

