// Original linear LDR sky colors are resolved into HDR by the composite pass.
uniform float4 LightDirectionAndSkyVisibility;
uniform float4 TwilightCelestialCloudTime;
uniform float4 CelestialToggles;
#include "SkyRay.slang"
#include "Atmosphere.slang"
#include "Celestial.slang"
[shader("fragment")]
float4 fragment_main(SkyVertex input) : SV_Target {
    return evaluate_original_sky(input.direction);
}
