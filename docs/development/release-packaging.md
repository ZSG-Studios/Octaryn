# Preview release packaging

The Windows previews are native x64 client packages with their
local server in `server/`. It is framework-dependent: install the .NET 10 x64
Runtime and Visual C++ x64 Redistributable. It is not a multiplayer server release.

The earlier Slang RHI preview includes an experimental Fedora 44 Linux package with a separate [native packaging guide](../build/linux-packaging.md). The integrated Lighting Preview is Windows-only until its Linux path is separately qualified. Publish each game archive with its matching relink companion and checksums.

## Freeze the build

Finish runtime changes and qualification before selecting a release snapshot.
Do not build concurrently in the same CMake tree or replace a live bundle.
The maintained build entrypoint is:

```powershell
python tools/build/windows.py --action build --preset release-windows --target octaryn_client_bundle octaryn_server_bundle
```

Use the complete `build/release-windows/client/bundle` directory. The native
client, managed assemblies, DLLs, Slang source, assets, module data and nested
server are one runtime payload. No development saves, logs, credentials, caches
or reference checkouts belong in the game archive.

## Attribution

`tools/release/collect_notices.py` collects matching dependency notices from the
configured source caches, runtime NuGet metadata, .NET host pack, Slang SDK/RHI,
FSR/Godot, fonts and audio. Python with Pillow is required for the atlas check.
The historical archive supplies the preserved Faithful license/credits; the
collector verifies its artwork against every tile in the current atlases.

```powershell
gh release download old-architecture-shareable-20260430 --pattern octaryn-old-architecture-windows-proton-20260430.zip --dir build/release-windows/releases/prior-release
python tools/build/windows.py --action package --preset release-windows
```

The single action collects notices into `releases/notices-draft`, packages the
game archive and the relink companion into `releases/`, and refuses to replace
existing outputs. `--name` overrides the archive name. The underlying
`tools/release/` modules remain directly runnable; `validate_package.py` stays
standalone because it qualifies an extracted package anywhere, not the build
tree. The inventory must have an empty `missing_required` list. Notices alone are not
the static OpenAL Soft redistribution materials: also produce the companion
relink archive. It contains the corresponding OpenAL source, exact client object
files, non-system link libraries, a relocatable response file and instructions.

## Package and verify

Commit the frozen production source, release notes and documentation so the tag
identifies the actual build. Preserve unrelated working-tree changes. The
package action above uses the current HEAD unless `--source-commit` names the
frozen tag commit. Extract the resulting game ZIP into a fresh directory outside
the repository before running both APIs:

```powershell
python tools/release/validate_package.py --repo-root . --bundle '<extracted-package>' --evidence-root '<fresh-dx12-evidence>' --api dx12
python tools/release/validate_package.py --repo-root . --bundle '<extracted-package>' --evidence-root '<fresh-vulkan-evidence>' --api vulkan
```

These are actual 600-frame application runs with isolated settings/worlds,
Native AA, UI checks, local server authority, full radius-four residency and GPU
captures. They require native graphics validation; Vulkan uses the configured
`build/dependencies/vulkan-validation` layer files. Errors fail the run; warnings
remain explicit in `result.json`. Inspect the images. A radius-32 run is available
with `--radius 32`; it is not a sustained-travel benchmark.

Run `Relink.ps1` from the extracted companion archive in an x64 C++ developer
PowerShell with LLVM. Check the resulting executable with a separate copy of the
game payload. Do not replace the tested release EXE with a relinking experiment.

Before publishing, confirm the release notes describe those exact artifacts,
include the Windows ZIPs, Linux tar.gz archives and checksums, and target the source commit recorded in their
manifests. Preserve historical releases and use a new prerelease tag. Never claim
Linux/macOS or broad GPU support from the Windows results.
