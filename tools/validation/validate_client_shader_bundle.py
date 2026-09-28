#!/usr/bin/env python3
import argparse
import filecmp
import pathlib
import sys
from fsr2_vendor import vendor_root, verified_files


EXPECTED_COMPILED_SHADERS = set()
REQUIRED_SLANG_SHADER_SOURCES = {
    "BlockTransportGI/Reconstruct.slang",
    "BlockTransportGI/Reflection.slang",
    "BlockTransportGI/Sample.slang",
    "BlockTransportGI/Types.slang",
    "Fsr2/Accumulate.slang",
    "Fsr2/DepthClip.slang",
    "Fsr2/Lock.slang",
    "Fsr2/Luminance.slang",
    "Fsr2/Options.slang",
    "Fsr2/Rcas.slang",
    "Fsr2/Reactive.slang",
    "Fsr2/Reconstruct.slang",
    "Fsr2/Tcr.slang",
    "Hdr/Composite.slang",
    "Hdr/CompositeRT.slang",
    "Hdr/MapReflectionTemporal.slang",
    "Hdr/MapReflections.slang",
    "Hdr/Present.slang",
    "Lighting/Ambient.slang",
    "Lighting/ClusteredLocalLights.slang",
    "Lighting/ClusteredLocalResolve.slang",
    "Lighting/ClusteredLocalSort.slang",
    "Lighting/DiffuseEnvironment.slang",
    "Lighting/HistoryConfidence.slang",
    "Lighting/LocalDirect.slang",
    "Lighting/LocalDirectRT.slang",
    "Lighting/LocalLight.slang",
    "Lighting/LocalLighting.slang",
    "Lighting/LocalVisibility.slang",
    "Lighting/Sampling.slang",
    "Lighting/Surface.slang",
    "Map/MapGeometry.slang",
    "Map/MapLocalShadow.slang",
    "Map/MapShadow.slang",
    "Map/WorldMap.slang",
    "Map/WorldMapRT.slang",
    "Materials/LabPbr.slang",
    "Materials/MaterialSampling.slang",
    "Materials/MaterialTypes.slang",
    "Materials/MetallicRoughness.slang",
    "Materials/PbrEnvironment.slang",
    "Materials/PbrEvaluate.slang",
    "Materials/SpriteSampling.slang",
    "Materials/TerrainParallax.slang",
    "Materials/TextureArrays.slang",
    "RayTracing/BoundsDebug.slang",
    "RayTracing/Debug.slang",
    "RayTracing/Shadow.slang",
    "RayTracing/WorldRayBounds.slang",
    "RayTracing/WorldRayQuery.slang",
    "RayTracing/WorldSkyVisibility.slang",
    "Shadows/LocalSample.slang",
    "Shadows/Reprojection.slang",
    "Shadows/Temporal.slang",
    "Sky/Atmosphere.slang",
    "Sky/Celestial.slang",
    "Sky/Clouds.slang",
    "Sky/Fog.slang",
    "Sky/Sky.slang",
    "Sky/SkyRay.slang",
    "Temporal/Inputs.slang",
    "Ui/Rml.slang",
    "World/WorldFluidTypes.slang",
    "World/WorldGeometry.slang",
    "World/WorldPatches.slang",
    "World/WorldPlantSurface.slang",
    "World/WorldSprites.slang",
    "World/WorldSurfaceKey.slang",
}


def relative_files(root):
    return {
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_file() and path.name != ".gitkeep"
    }


def validate(source_root, bundle_shader_root):
    errors = []

    if not source_root.exists():
        errors.append(f"{source_root}: client shader source root is missing")
    if not bundle_shader_root.exists():
        errors.append(f"{bundle_shader_root}: client bundle shader root is missing")
    if errors:
        return errors

    source_files = relative_files(source_root)
    bundled_files = relative_files(bundle_shader_root)
    vendor_files = set()
    if (source_root / "Fsr2/DepthClip.slang").is_file():
        try:
            reference = vendor_root(source_root.parents[1])
            expected_vendor = verified_files(reference)
            actual_vendor = verified_files(bundle_shader_root / "Fsr2/Vendor")
            if actual_vendor != expected_vendor:
                raise ValueError("FSR2 bundled vendor inventory differs from prepared source")
            for name in expected_vendor:
                if not filecmp.cmp(reference / name, bundle_shader_root / "Fsr2/Vendor" / name, shallow=False):
                    raise ValueError(f"FSR2 bundled vendor differs from prepared source: {name}")
            vendor_files = {"Fsr2/Vendor/" + name for name in expected_vendor}
        except (OSError, ValueError, KeyError) as error:
            errors.append(f"FSR2 pinned vendor validation: {error}")
    bundled_source_files = {
        path for path in bundled_files if not path.startswith("Compiled/") and path not in vendor_files
    }
    compiled_files = {
        path.removeprefix("Compiled/")
        for path in bundled_files
        if path.startswith("Compiled/")
    }
    if not source_files:
        errors.append(f"{source_root}: client shader source root has no shader files")
    non_slang_sources = sorted(path for path in source_files if not path.endswith(".slang"))
    if non_slang_sources:
        errors.append(f"{source_root}: active shader sources must be Slang-only: {non_slang_sources}")

    missing_slang = sorted(REQUIRED_SLANG_SHADER_SOURCES - source_files)
    if missing_slang:
        errors.append(f"{source_root}: missing required client Slang pipeline sources: {missing_slang}")

    missing = sorted(source_files - bundled_source_files)
    extra = sorted(bundled_source_files - source_files)
    if missing:
        errors.append(f"{bundle_shader_root}: missing client-owned shader files: {missing}")
    if extra:
        errors.append(f"{bundle_shader_root}: contains undeclared client shader files: {extra}")

    if compiled_files:
        errors.append(f"{bundle_shader_root / 'Compiled'}: legacy compiled GLSL runtime shaders are not allowed in the Slang-first renderer: {sorted(compiled_files)}")

    legacy_glsl = sorted(path for path in source_files | bundled_source_files if path.endswith(".glsl"))
    if legacy_glsl:
        errors.append(f"{source_root}: legacy GLSL shader sources are quarantined and must not be active: {legacy_glsl}")

    for relative in sorted(source_files & bundled_source_files):
        source_file = source_root / relative
        bundled_file = bundle_shader_root / relative
        if not filecmp.cmp(source_file, bundled_file, shallow=False):
            errors.append(f"{bundled_file}: does not match client-owned source {source_file}")

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--bundle-shader-root", required=True)
    args = parser.parse_args()

    errors = validate(
        pathlib.Path(args.source_root).resolve(),
        pathlib.Path(args.bundle_shader_root).resolve())
    if errors:
        for error in errors:
            print(f"client shader bundle policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
