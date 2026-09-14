# Old engine restoration

Date: 2026-09-13

## Active project and provenance

Active root: C:\Users\Rose-X\Documents\Octaryn

The contents of the original octaryn-workspace-dev archive are promoted directly
to this root. The client, server, shared, and basegame owner folders, root CMake
files, Octaryn.DotNet.sln, tools, assets, shaders, and documentation are active.

The original compressed archive is also preserved at
ref/archives/octaryn-workspace-dev.tar.gz. SHA-256:

58a5cf954976225c157650e78d0db413bdec41ae57bb5d29032ac0e369df8c58

The Downloads original and the backup's full reference extraction remain intact.
Original root instructions and plans also live in docs/history/restored-archive.

## Complete previous workspace backup

C:\Users\Rose-X\Documents\Octaryn-Backups\2026-09-13-before-old-engine\workspace

The entire previous workspace was moved together, not selectively filtered:
source, .git, tools, dependencies, build outputs, published packages, reports,
profiles, references, and the recent C# stress harness are preserved.

Backup metadata lives beside workspace:
inventory.json, source-hashes.json, and backup-summary.json.

Verification:
- All 27,259 inventory files exist in the backup with their original sizes.
- Inventory total: 5,825,286,855 bytes.
- All 123 critical source/document/tool SHA-256 hashes match.
- The original archive manifest contains 794 files.
- Under the verification audit's five root-document/config exclusions,
  all 789 checked engine files match size and SHA-256 in both the preserved
  reference and the promoted root. Zero mismatches.
- Active documentation changes are intentional and separate from engine source.
- No engine source, dependency pins, or CMake implementation changed in this pass.

The previous prototype server was stopped before moving its directory. It was
not restarted from the backup. The restored engine was not built or launched.

## Git state

The previous .git directory was preserved in the backup and copied to the active
root. Its origin remains https://github.com/ZSG-Studios/zsg-studios-workspace.git.
No staging, commit, push, or remote retarget occurred.

That Git history belongs to the prior workspace checkout; the archive does not
gain its own historical commits by being restored. Expect restored engine files
and removed prior checkout paths in the working diff. Existing sparse-checkout
configuration is retained; inspect it before future checkout/sparse operations
so they do not rematerialize unwanted paths or hide the restored engine.

## Repair direction

Stop the wholesale new-engine rewrite proposal. Keep existing native and managed
systems while repairing the old engine. Preserve clean owner boundaries and use
profiling and behavior evidence to justify replacements. Do not automatically
resume the archive's blanket C# removal loops either.

Use the backup's networking work as recovery material. Its BEPU integration does
not directly apply to the restored Jolt owner. See networking-recovery.md.

The restored renderer includes slang-gfx.h and uses gfx::IDevice. Its SlangRhi
names and slang_rhi CMake alias do not mean standalone slang-rhi is integrated.
Slang/Vulkan remains the requested direction; an RHI migration needs a scoped
feature comparison and working baseline, not a package-name substitution.

## First Windows build repairs

These are inspection findings, not results of a configure/build attempt:

1. Make native Windows build tools and entrypoints usable. CMake and Ninja are
   absent from current PATH. Existing Windows presets require prefixed
   LLVM-MinGW tools via OCTARYN_WINDOWS_CLANG_ROOT, currently unset; ordinary
   installed clang/clang-cl do not satisfy those searches. Bash bootstrap scripts
   retain Linux/Podman assumptions.
2. Correct .NET hosting discovery. DOTNET_ROOT is unset and CMake defaults to
   /usr/share/dotnet. Windows SDK 10.0.401 and win-x64 hosting pack 10.0.12 exist
   under C:\Program Files\dotnet. Missing hosting must not silently turn a
   requested graphical application into a skipped placeholder target.
3. Resolve and stage the actual Slang GFX SDK dependency. SLANG_SDK_ROOT is unset.
   Existing CMake needs slang-gfx.h and gfx, not only slangc or standalone RHI.
   Missing GFX can leave a runtime_unavailable probe instead of a renderer.
4. Verify Windows runtime packaging, including hostfxr discovery and native GFX
   DLLs. Loading bare hostfxr.dll and unstaged SDK libraries can fail after a
   successful compile.
5. Build the real client/server bundles and run the actual graphical and headless
   paths. Then capture frame/stream timing and network clock behavior before
   transferring fixes or claiming platform support.

CMake source dependency fetching defaults ON, so configuration may download and
build dependencies. That work was not silently initiated during restoration.
Linux and macOS remain desktop targets requiring their own build/runtime proof.
Historical DONE.MD and profiling reports are not fresh measurements.