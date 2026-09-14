// Staged alongside the exact production vertex/ray implementation.
#include "SkyRay.slang"
[shader("fragment")]
float4 fragment_main(SkyVertex input) : SV_Target0 {
    return float4(.5 + input.direction * .2, 1);
}
