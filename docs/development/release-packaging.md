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
.\tools\build\windows.ps1 -Action build -Target @('octaryn_client_bundle', 'octaryn_server_bundle')
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
python tools/release/collect_notices.py --repo-root . --output build/release-windows/releases/notices-draft
```

The inventory must have an empty `missing_required` list. Notices alone are not
the static OpenAL Soft redistribution materials: also produce the companion
relink archive. It contains the corresponding OpenAL source, exact client object
files, non-system link libraries, a relocatable response file and instructions.

## Package and verify

Commit the frozen production source, release notes and documentation so the tag
identifies the actual build. Preserve unrelated working-tree changes. Pass that
full commit ID to the packagers:

```powershell
$sourceCommit = git rev-parse HEAD
python tools/release/package_windows.py --repo-root . --bundle build/release-windows/client/bundle --notices build/release-windows/releases/notices-draft --output build/release-windows/releases --source-commit $sourceCommit
python tools/release/package_relink.py --repo-root . --output build/release-windows/releases --source-commit $sourceCommit
```

The packagers refuse to replace existing release directories or archives. Each
archive has a per-file manifest and a separate SHA-256 file. The game packager
verifies the copied bundle and archived bytes and rejects inputs that change
during packaging. Extract the resulting game ZIP into a fresh directory outside
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
