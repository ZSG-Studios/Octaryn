"""Collect release attribution from a configured native Windows or Linux build.

This copies notices, not source/relink materials. Its inventory explicitly lists
that separate requirement for the statically linked OpenAL Soft dependency.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import sys
import subprocess
import xml.etree.ElementTree as ET
import zipfile


NATIVE = (
    "cpptrace eigen3 fastgltf glaze joltphysics lz4 mimalloc miniaudio openalsoft "
    "rmlui sdl3 spdlog taskflow tracy unordered_dense zlib zstd"
).split()
LEGAL = re.compile(r"^(licen[cs]e|copying|copyright|notice|third.?party.?notices|ofl)([._-]|$)", re.I)
PRIOR = "octaryn-old-architecture-windows-proton-20260430.zip"
PRIOR_URL = "https://github.com/ZSG-Studios/Octaryn/releases/tag/old-architecture-shareable-20260430"


def sha(data):
    return hashlib.sha256(data).hexdigest()


class Collector:
    def __init__(self, repo, output, preset="release-windows", platform="windows", architecture="x64",
                 prior=None, dotnet_notices=None, rhi_dependencies=None):
        self.repo, self.output = repo, output
        self.platform, self.architecture, self.preset = platform, architecture, preset
        self.bundle = repo / "build" / preset / "client/bundle"
        self.cache = (repo / "build" / preset / "cmake/CMakeCache.txt").read_text(encoding="utf-8")
        self.sdk = Path(self.cache_value("OCTARYN_SLANG_SDK_ROOT"))
        self.rhi = Path(self.cache_value("OCTARYN_SLANG_RHI_BUILD_ROOT"))
        self.rhi_dependencies = rhi_dependencies or self.rhi / "_deps"
        self.prior = prior or repo / "build/release-windows/releases/prior-release" / PRIOR
        self.dotnet_notices = dotnet_notices
        self.files, self.components, self.errors = [], [], []

    def cache_value(self, name):
        match = re.search(r"^" + re.escape(name) + r":[^=]+=(.+)$", self.cache, re.M)
        if not match or match[1].strip().endswith("NOTFOUND"):
            raise ValueError(f"Configured dependency path missing: {name}")
        return match[1].strip()

    def origin(self, path):
        try:
            return path.relative_to(self.repo).as_posix()
        except ValueError:
            return path.as_posix()

    def put(self, target, data, origin):
        path = self.output / target
        if path.resolve().is_relative_to(self.output) is False:
            raise ValueError(f"Invalid notice destination: {target}")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        self.files.append(dict(path=target, sha256=sha(data), source=origin))

    def copy(self, source, target):
        if not source.is_file():
            self.errors.append(f"Missing required file: {self.origin(source)}")
            return
        self.put(target, source.read_bytes(), self.origin(source))

    def tree(self, name, source, recursive=True):
        matches = sorted(p for p in (source.rglob("*") if recursive else source.glob("*"))
                         if (LEGAL.match(p.name) or any(part.lower() == "licenses" for part in p.parts))
                         and not p.is_symlink() and p.is_file()
                         and ".git" not in p.parts)
        if not matches:
            self.errors.append(f"No license texts found for {name}: {self.origin(source)}")
        for path in matches:
            self.copy(path, f"THIRD_PARTY/{name}/{path.relative_to(source).as_posix()}")
        self.components.append(dict(name=name, source=self.origin(source),
                                    scope="configured dependency; may include build-only subcomponents"))

    def nuget(self):
        cache = self.repo / "build/dependencies/nuget"
        bundle = self.bundle
        dlls = {}
        for path in bundle.rglob("*.dll"):
            dlls.setdefault(path.name.lower(), set()).add(sha(path.read_bytes()))
        for package in sorted(cache.glob("*/*")):
            specs = sorted(package.glob("*.nuspec"))
            if not specs:
                continue
            root = ET.parse(specs[0]).getroot()
            metadata = root.find("{*}metadata")
            def value(key):
                return metadata.findtext("{*}" + key, "")
            name, version = value("id"), value("version")
            runtime = any(p.name.lower() in dlls and sha(p.read_bytes()) in dlls[p.name.lower()]
                          for p in package.glob("lib/**/*.dll"))
            if not runtime:
                continue
            folder = f"THIRD_PARTY/NuGet/{name}/{version}"
            self.copy(specs[0], f"{folder}/{specs[0].name}")
            license_node = metadata.find("{*}license")
            expression = license_node.text if license_node is not None else ""
            kind = license_node.get("type") if license_node is not None else ""
            # This package explicitly declares Apache2.0 in its copyright field.
            if name == "Arch.Relationships" and value("copyright") == "Apache2.0":
                expression, kind = "Apache-2.0", "expression"
            texts = sorted(p for p in package.rglob("*") if p.is_file() and LEGAL.match(p.name))
            for path in texts:
                self.copy(path, f"{folder}/{path.relative_to(package).as_posix()}")
            if kind == "file":
                self.copy(package / expression, f"{folder}/{Path(expression).as_posix()}")
            elif kind == "expression":
                if expression not in ("MIT", "Apache-2.0"):
                    self.errors.append(f"Unsupported NuGet license expression: {name}: {expression}")
                else:
                    self.copy(self.sdk / f"LICENSES/{expression}.txt",
                              f"{folder}/LicenseExpression-{expression}.txt")
            elif not texts:
                self.errors.append(f"No NuGet license text or known expression: {name}/{version}")
            self.components.append(dict(name=name, version=version, license=expression,
                                        source=specs[0].relative_to(self.repo).as_posix(),
                                        scope="runtime assembly or .NET hosting/runtime materials"))

    def faithful(self):
        from PIL import Image
        manifest = self.bundle / "Assets/Atlases/basegame-color.txt"
        self.copy(manifest, "THIRD_PARTY/ClassicFaithful/atlas-manifest.txt")
        prior = self.prior
        if not prior.is_file():
            self.errors.append(f"Missing prior release attribution archive: {self.origin(prior)}")
            return
        with zipfile.ZipFile(prior) as archive:
            names = sorted(archive.namelist())
            materials = [n for n in names if "/third_party/classicfaithful/" in ("/" + n.lower())
                         and not n.endswith("/")]
            if not any("license" in Path(n).name.lower() for n in materials):
                self.errors.append("Prior release contains no ClassicFaithful license")
            if not any("credit" in Path(n).name.lower() for n in materials):
                self.errors.append("Prior release contains no ClassicFaithful credits")
            for name in materials:
                relative = re.split(r"third_party/classicfaithful/", name, flags=re.I)[1]
                self.put(f"THIRD_PARTY/ClassicFaithful/prior-release/{relative}", archive.read(name),
                         f"{PRIOR_URL} :: {name}")
            comparisons = []
            old_names = {"basegame-color.png": "atlas.png", "basegame-normal.png": "atlas_n.png",
                         "basegame-specular.png": "atlas_s.png", "basegame-animation.png": "atlas_anim.png"}
            for filename in old_names:
                current = manifest.parent / filename
                if not current.is_file():
                    self.errors.append(f"Missing bundled atlas: {filename}")
                    continue
                candidates = [n for n in names if n.lower().endswith("/assets/textures/" + old_names[current.name])]
                match = next((n for n in candidates if sha(archive.read(n)) == sha(current.read_bytes())), None)
                tile_map = []
                if not match and candidates:
                    old = Image.open(io.BytesIO(archive.read(candidates[0]))).convert("RGBA")
                    new = Image.open(current).convert("RGBA")
                    if old.height == new.height == 32 and old.width % 32 == new.width % 32 == 0:
                        prior_tiles = [old.crop((x, 0, x + 32, 32)).tobytes() for x in range(0, old.width, 32)]
                        for x in range(0, new.width, 32):
                            tile = new.crop((x, 0, x + 32, 32)).tobytes()
                            tile_map.append(prior_tiles.index(tile) if tile in prior_tiles else None)
                        if tile_map and None not in tile_map:
                            match = candidates[0]
                comparisons.append(dict(file=current.name, sha256=sha(current.read_bytes()), prior_match=match,
                                        prior_tile_indices=tile_map,
                                        comparison="RGBA tile subset" if tile_map else "whole-file SHA-256"))
                if not match:
                    self.errors.append(f"Cannot establish prior release provenance for atlas: {current.name}")
            provenance = dict(source_release=PRIOR_URL, archive=PRIOR, archive_sha256=sha(prior.read_bytes()),
                              atlas_comparison=comparisons,
                              qualification="Matching images establish prior-release asset identity, not a missing upstream pack commit.")
            self.put("THIRD_PARTY/ClassicFaithful/provenance.json",
                     (json.dumps(provenance, indent=2) + "\n").encode(), "comparison with prior release")
        self.components.append(dict(name="Classic Faithful 32x Jappa", source=PRIOR_URL,
                                    scope="generated atlas artwork"))

    def hosting(self):
        host = Path(self.cache_value("OCTARYN_DOTNET_NETHOST_RUNTIME"))
        shipped = self.bundle / host.name
        if not host.is_file() or not shipped.is_file() or sha(host.read_bytes()) != sha(shipped.read_bytes()):
            self.errors.append(f"Bundled {host.name} does not match the configured host pack")
            return
        dotnet = next((p for p in host.parents if p.name.lower() == "dotnet"), None)
        candidates = []
        if self.dotnet_notices:
            candidates.extend(self.dotnet_notices.rglob("*"))
        elif dotnet:
            candidates.extend(dotnet.glob("*"))
        # Fedora packages retain exact installed-package license files in /usr/share/licenses.
        if self.platform == "linux" and not self.dotnet_notices:
            import shutil
            if shutil.which("rpm"):
                query = subprocess.run(["rpm", "-qf", "--qf", "%{NAME}\n", str(host)],
                                       capture_output=True, text=True, check=False)
                for package in query.stdout.splitlines() if query.returncode == 0 else []:
                    listing = subprocess.run(["rpm", "-ql", package], capture_output=True, text=True, check=True)
                    candidates.extend(Path(line) for line in listing.stdout.splitlines()
                                      if "/licenses/" in line or "/doc/" in line)
        legal = sorted({p for p in candidates if p.is_file() and LEGAL.match(p.name)})
        if not any("license" in p.name.lower() or "copying" in p.name.lower() for p in legal):
            self.errors.append("Missing exact installed .NET host license; supply --dotnet-notices")
        if not any("third" in p.name.lower() for p in legal):
            self.errors.append("Missing exact installed .NET ThirdPartyNotices; supply --dotnet-notices")
        for index, path in enumerate(legal):
            self.put(f"THIRD_PARTY/DotNetHosting/{index}-{path.name}", path.read_bytes(), self.origin(path))
        self.components.append(dict(name=host.name, source=self.origin(host), sha256=sha(host.read_bytes()),
                                    scope="configured host pack, shipped binary identity checked"))

    def dxc(self):
        root = self.rhi_dependencies / "dxc-src"
        for name in ("dxcompiler.dll", "dxil.dll"):
            source = root / "bin" / self.architecture / name
            shipped = self.bundle / name
            if not source.is_file() or not shipped.is_file() or sha(source.read_bytes()) != sha(shipped.read_bytes()):
                self.errors.append(f"Bundled {name} does not match the DXC notice source archive")
            else:
                self.components.append(dict(name=name, source=self.origin(source),
                                            sha256=sha(source.read_bytes()), scope="DXC binary identity checked"))

    def freetype(self):
        root = self.repo / "build/dependencies/src/freetype"
        # LICENSE.TXT points to several differently named texts and source headers.
        # Preserve the referenced originals in full instead of guessing excerpts.
        for relative in ("LICENSE.TXT", "docs/FTL.TXT", "docs/GPLv2.TXT", "src/bdf/README",
                         "src/pcf/README", "src/base/fthash.c", "include/freetype/internal/fthash.h",
                         "src/gzip/zlib.h", "src/autofit/ft-hb.c", "src/autofit/ft-hb.h"):
            self.copy(root / relative, f"THIRD_PARTY/FreeType/{relative}")
        self.components.append(dict(name="FreeType", source=root.relative_to(self.repo).as_posix(),
                                    license="FTL", scope="RmlUi font rasterizer; alternative GPL text retained as upstream documentation"))

    def finish(self):
        obligations = (
            "OpenAL Soft 1.25.1 is statically linked into this release. Its COPYING is the GNU Library "
            "GPL version 2. Package the exact corresponding OpenAL Soft source (including modifications "
            "and build scripts), plus machine-readable application object files and the link inputs/commands "
            "needed to relink the application with a modified library. Preserve copyright/license notices. "
            "See COPYING sections 4 and 6; notice collection alone does not supply those materials. "
            "The release owner must provide and verify the separate relink/source archive."
        )
        notice = ("Octaryn third-party notices\n\n"
                  "Licenses remain the property of their respective authors. Exact copied files and SHA-256 "
                  "hashes are recorded in THIRD_PARTY/inventory.json. Configured caches include some tools "
                  "and optional dependencies; inclusion here is not a claim of runtime use.\n\n"
                  "Textures: Faithful Resource Pack and ClassicFaithful contributors. "
                  "https://faithfulpack.net/ and https://github.com/ClassicFaithful/Classic-32x-Jappa-Java\n"
                  "This is an unofficial Octaryn release. Preserve the supplied Faithful license and credits; "
                  "the pack's license restricts monetization. Atlas provenance is supplied separately.\n\n"
                  "Font: Silkscreen by The Silkscreen Project Authors; SIL Open Font License 1.1.\n\n"
                  "Portions of this software are copyright (C) 1996-2023 The FreeType Project "
                  "(https://www.freetype.org/). All rights reserved. Octaryn uses FreeType under "
                  "the FreeType License (FTL); its text and contributed-driver notices are included "
                  "in THIRD_PARTY/FreeType.\n\n"
                  + obligations + "\n")
        self.put("THIRD_PARTY_NOTICES.txt", notice.encode(), "release attribution inventory")
        inventory = dict(platform=f"{self.platform}-{self.architecture}", preset=self.preset, components=self.components, files=sorted({f["path"]: f for f in self.files}.values(), key=lambda f: f["path"]),
                         missing_required=self.errors, separate_release_materials=[obligations])
        target = self.output / "THIRD_PARTY/inventory.json"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8")
        for error in self.errors:
            print(f"ERROR: {error}", file=sys.stderr)
        print(f"notices={'failed' if self.errors else 'passed'} files={len(self.files)} output={self.output}")
        return 1 if self.errors else 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--platform", choices=("windows", "linux"), default="windows")
    parser.add_argument("--architecture", choices=("x64", "arm64"), default="x64")
    parser.add_argument("--preset", help="Configured build directory name, including architecture suffix")
    parser.add_argument("--prior-release", type=Path, help="Original attribution ZIP with validated atlas provenance")
    parser.add_argument("--rhi-build-dependencies", type=Path, help="Retained exact RHI _deps tree for relocated builds")
    parser.add_argument("--dotnet-notices", type=Path, help="Exact installed host-pack license/third-party notices")
    args = parser.parse_args(argv)
    repo, output = args.repo_root.resolve(), args.output.resolve()
    if output == repo or not output.is_relative_to(repo / "build"):
        parser.error("--output must be a dedicated directory under the repository build directory")
    preset = args.preset or f"release-{args.platform}" + ("-arm64" if args.architecture == "arm64" else "")
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", preset):
        parser.error("--preset must be a simple configured directory name")
    if output.exists() and any(output.iterdir()):
        parser.error("--output must be new or empty; stale notices must not carry into another platform")
    collector = Collector(repo, output, preset, args.platform, args.architecture,
                          args.prior_release.resolve() if args.prior_release else None,
                          args.dotnet_notices.resolve() if args.dotnet_notices else None,
                          args.rhi_build_dependencies.resolve() if args.rhi_build_dependencies else None)
    collector.copy(repo / "LICENSE", "LICENSE")
    for name in NATIVE:
        collector.tree(name, repo / "build/dependencies/src" / name)
    collector.tree("Slang", collector.sdk)
    collector.tree("SlangRHI", repo / "build/dependencies/slang-rhi")
    collector.tree("SlangRHI-BuildDependencies", collector.rhi_dependencies)
    fsr = repo / "build/dependencies/fsr2-2.2.1-godot-2f698aa5/slang"
    collector.copy(fsr / "AMD-LICENSE.txt", "THIRD_PARTY/FSR2/AMD-LICENSE.txt")
    collector.copy(fsr / "GODOT-LICENSE.txt", "THIRD_PARTY/FSR2/GODOT-LICENSE.txt")
    collector.copy(repo / "octaryn-basegame/Assets/Ui/Fonts/OFL.txt", "THIRD_PARTY/Silkscreen/OFL.txt")
    collector.copy(repo / "octaryn-basegame/Assets/Ui/Sources.txt", "THIRD_PARTY/Silkscreen/Sources.txt")
    collector.copy(repo / "docs/third-party/ActionAudio.md", "THIRD_PARTY/ActionAudio.md")
    collector.nuget()
    collector.hosting()
    if args.platform == "windows":
        collector.dxc()
    collector.freetype()
    collector.faithful()
    return collector.finish()


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, ImportError, ET.ParseError, zipfile.BadZipFile) as error:
        print(f"notices=failed: {error}", file=sys.stderr)
        sys.exit(1)
