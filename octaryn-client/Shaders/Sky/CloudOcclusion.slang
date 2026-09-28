#pragma once
// GBuffer .w is linear camera depth. Compare in the same D32 projection as
// CloudOutput.depth, with a margin that preserves ambiguous surface edges.
bool cloud_occluded_before_entry(float4 opaque, float entry_distance, float view_cosine, float2 projection)
{
    if (!all(isfinite(opaque)) || opaque.w <= 0 || !isfinite(entry_distance) || entry_distance <= 0 ||
        !isfinite(view_cosine) || view_cosine <= 0 || !all(isfinite(projection)) || projection.y <= 0)
        return false;
    float entry_depth = entry_distance * view_cosine;
    if (!isfinite(entry_depth) || entry_depth <= opaque.w)
        return false;
    float scene_z = projection.x - projection.y / max(opaque.w, 0.001);
    float entry_z = saturate(projection.x - projection.y / max(entry_depth, 0.001));
    return scene_z >= 0 && scene_z < 1 && scene_z + 0.000001 < entry_z;
}
