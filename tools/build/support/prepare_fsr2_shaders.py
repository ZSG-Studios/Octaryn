"""Create an auditable Slang-compatible derivative; immutable downloads stay intact."""
import argparse
import re
import hashlib
import json
from pathlib import Path


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError(f"Pinned FSR2 patch context changed: {old[:80]}")
    return text.replace(old, new, 1)


def prepare(root):
    source = root / "upstream/shaders"
    output = root / "slang"
    output.mkdir(parents=True, exist_ok=True)
    for path in source.iterdir():
        if path.suffix == ".glsl":
            continue
        text = path.read_text(encoding="utf-8")
        if path.name == "ffx_fsr2_callbacks_hlsl.h":
            text = replace_once(text, "FfxFloat32    fViewSpaceToMetersFactor;",
                                "FfxFloat32    fViewSpaceToMetersFactor;\n"
                                "        FfxFloat32 fPad;\n"
                                "        column_major float4x4 mReprojectionMatrix;")
            text = replace_once(text, "return r_reactive_mask[iPxPos];",
                                "return min(r_reactive_mask[iPxPos], 0.9f);")
            text = replace_once(text,
                "FfxFloat32x2 fUvMotionVector = fSrcMotionVector * MotionVectorScale();",
                """#if FFX_FSR2_OPTION_GODOT_DERIVE_INVALID_MOTION_VECTORS
    if (all(fSrcMotionVector <= float2(-1.0f, -1.0f))) {
        float2 uv = (float2(iPxDilatedMotionVectorPos) + 0.5f) / float2(RenderSize());
        float4 previous = mul(mReprojectionMatrix, float4(uv * 2.0f - 1.0f,
            LoadInputDepth(iPxDilatedMotionVectorPos), 1.0f));
        fSrcMotionVector = (previous.xy / previous.w * 0.5f + 0.5f - uv)
            / MotionVectorScale();
    }
#endif
    FfxFloat32x2 fUvMotionVector = fSrcMotionVector * MotionVectorScale();""")
        if path.name in ("ffx_fsr2_accumulate_pass.hlsl", "ffx_fsr2_tcr_autogen_pass.hlsl"):
            text = replace_once(text, '#include "ffx_fsr2_callbacks_hlsl.h"',
                '#if FFX_FSR2_OPTION_GODOT_DERIVE_INVALID_MOTION_VECTORS\n'
                '#define FSR2_BIND_SRV_INPUT_DEPTH 63\n#endif\n'
                '#include "ffx_fsr2_callbacks_hlsl.h"')
        if path.name == "ffx_fsr2_rcas_pass.hlsl":
            # RCAS samples a cross extending beyond the image at its border.
            # Define clamp addressing there instead of backend-dependent OOB
            # texel loads, which introduce visible dark edges on constant HDR.
            text = replace_once(text, "return r_rcas_input[iPxPos];",
                "return r_rcas_input[clamp(iPxPos, FfxInt32x2(0, 0), DisplaySize() - 1)];")
        if path.name == "ffx_fsr2_upsample.h":
            # The original ClampLoad only clamps along a NONZERO offset. This
            # call supplies zero after already applying offsets, so its border
            # loads were out of bounds and polluted the rectification box.
            text = replace_once(text,
                "ClampLoad(iSrcSamplePos, FfxInt32x2(0, 0), FfxInt32x2(RenderSize()))",
                "clamp(iSrcSamplePos, FfxInt32x2(0, 0), FfxInt32x2(RenderSize()) - 1)")
        # RHI binds through Slang reflection. Remove D3D-only fixed register
        # annotations to avoid overlapping Vulkan descriptor namespaces.
        text = re.sub(r":\s*FFX_FSR2_DECLARE_(?:SRV|UAV|CB)\([^)]*\)", "", text)
        text = re.sub(r":\s*register\([stu b][0-9]+\)", "", text)
        # Slang samples float textures; normalization belongs to the exact
        # R8/RG8/RGBA8 view format, not HLSL's normalized scalar type modifier.
        text = re.sub(r"(Texture[123]D)<(?:unorm|snorm)\s+", r"\1<", text)
        # The HLSL declarations already express the Godot storage-format fixes:
        # RWTexture2D<float> depth and RWTexture2D<float4> prepared color.
        (output / path.name).write_text(text, encoding="utf-8")
    for name, original in (("AMD-LICENSE.txt", root / "godot/LICENSE.txt"),
                           ("GODOT-LICENSE.txt", root / "GODOT-LICENSE.txt"),
                           ("PROVENANCE.txt", root / "PROVENANCE.txt")):
        (output / name).write_bytes(original.read_bytes())
    manifest = {"godot": "2f698aa5fe31d0be68f205ec41aec9365081d364",
                "amd": "1680d1edd5c034f88ebbbb793d8b88f8842cf804",
                "files": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in sorted(output.iterdir()) if p.name != "manifest.json"}}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"fsr2_shader_derivative=prepared path={output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    prepare(parser.parse_args().root)
