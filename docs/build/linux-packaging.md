# Native Linux packaging

`python3 tools/build/linux.py --action package` runs the whole release flow
against the configured build tree: notice collection, game archive and relink
companion. It does not configure, build, execute or publish the application. Native build,
GPU/runtime qualification and dependency attribution must be completed first.

```sh
python3 tools/build/linux.py --action package --preset release-linux \
  --name octaryn-slang-rhi-preview-linux-x64-20260914
```

`--source-commit` defaults to the current HEAD; `--release-notes` defaults to
the preview notes; `--relink-name` defaults to `<name>-relink`;
`--prior-release` supplies the original-attribution ZIP. Notices land in
`build/release-linux/releases/notices`, archives in
`build/release-linux/releases/packages`. The underlying `tools/release/`
modules remain directly runnable with the same arguments as below. Use the
actual final release notes and source commit.
output name and produces a staged directory, `.tar.gz`, and archive SHA-256 file.
It rejects links/special files, missing payloads, unexpected saves/logs, incorrect
ELF architecture and missing executable permission. Tar members use normalized
0644/0755 permissions and zero owner IDs while retaining which files execute.
Every file's SHA-256, size and mode is recorded; the archive is reread and checked.
Inputs are checked again to detect changes during packaging.

`Launch-Octaryn.sh` selects Vulkan and X11 and forwards client arguments. The
package requires .NET 10, a Vulkan driver/loader, X11 or XWayland and compatible
system shared libraries. Distribution/glibc requirements belong in the qualified
release notes. Packaging does not establish WSL hardware rendering or broad Linux
portability. Static OpenAL requires separate matching source/relink materials.

## Native notice collection

The shared collector accepts platform, architecture and preset; the package
action invokes it as follows:

```sh
python3 tools/release/collect_notices.py \
  --repo-root . --platform linux --architecture x64 --preset release-linux \
  --prior-release /absolute/path/to/octaryn-old-architecture-windows-proton-20260430.zip \
  --output build/release-linux/releases/notices
```

Use a fresh output directory and install Python Pillow for atlas comparisons.
The collector reads the configured Slang SDK, RHI build and nethost source from
CMakeCache.txt; matching nethost bytes must be present in the bundle. It identifies
managed NuGet assemblies by name and exact SHA-256, collects current native
source licenses, and compares the actual bundled atlas images with validated
original-release attribution. Removed ImGui/SDL_image/SDL_ttf/ozz/recast/ktx/meshoptimizer
cache trees are not release dependencies. FreeType notices come from the directly
pinned `build/dependencies/src/freetype` source.

On Fedora, installed nethost-package license files are discovered using read-only
RPM queries. If the host pack's exact license and ThirdPartyNotices cannot be
located, collection fails; `--dotnet-notices` accepts a directory containing those
exact installed-version materials. It does not download or substitute arbitrary
notices. Windows still performs DXC checks; Linux does not require Windows DXC.

The output inventory identifies `linux-x64` (or `linux-arm64`) and must have an
empty `missing_required` list before packaging. The configured source inventory
can include build-only components; inclusion does not assert runtime use.
System shared-library/glibc requirements still need actual native dependency and
runtime qualification. The collector supplies notices, not OpenAL relink materials. Use the native
companion tool below; the Windows relink archive is not a substitute.

## Linux OpenAL source/relink companion

The package action invokes the native companion tool as follows:

```sh
python3 tools/release/package_relink_linux.py \
  --repo-root . --preset release-linux --architecture x64 \
  --notices build/release-linux/releases/notices \
  --output build/release-linux/releases/packages \
  --source-commit FULL_40_CHARACTER_GIT_COMMIT \
  --name octaryn-slang-rhi-preview-relink-linux-x64-20260914
```

This requires finished native link inputs. It reads the actual Ninja client link
edge, preserves object/static-library order, copies non-system shared link inputs
and their SONAME aliases as regular files, and replaces build RPATH with $ORIGIN.
System `-l` requirements remain explicit. It includes matching OpenAL source,
portable configured ALSOFT options, `Relink.sh`, `Rebuild-OpenAL.sh`, notices and
SHA-256 manifests. The tar.gz is reread for member/hash/mode verification.

The script does not run the linker or game. Before release, execute the supplied
relink command on native Linux and verify that the resulting client works with
the companion game package. Archive creation alone is not relink qualification.

## Draft tool qualification

A Fedora 44/WSL2 x64 draft collected 140 notice files with no missing requirements.
The draft relink archive included 57 exact link inputs and 655 files and passed
archive readback. Its generated `Relink.sh` exited 0; the resulting ELF64 x64
client has `$ORIGIN:$ORIGIN/libraries` RUNPATH and no unresolved `ldd` dependencies.
This check did not execute the game or rebuild a modified OpenAL library. Final
release archives must be regenerated against the final source commit and qualified
separately; the draft's source marker was `1d6eb8dc689dc05d9a7cb1d1fd22e18bbe9f4107`.
