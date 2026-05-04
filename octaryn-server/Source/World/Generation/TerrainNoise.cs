namespace Octaryn.Server.World.Generation;

internal static class TerrainNoise
{
    public const int Seed = 1337;

    public static float SampleHeight(int worldX, int worldZ)
    {
        return SampleFbm(worldX, worldZ, frequency: 0.005f, octaves: 6, seedOffset: 0);
    }

    public static float SampleLowland(int worldX, int worldZ)
    {
        return SampleFbm(-worldX, worldZ, frequency: 0.01f, octaves: 6, seedOffset: 101);
    }

    public static float SampleBiome(int worldX, int worldZ)
    {
        return SampleFbm(worldX, worldZ, frequency: 0.2f, octaves: 6, seedOffset: 211);
    }

    public static float SamplePlant(int worldX, int worldZ)
    {
        return SampleFbm(worldX, worldZ, frequency: 0.2f, octaves: 3, seedOffset: 307);
    }

    private static float SampleFbm(int worldX, int worldZ, float frequency, int octaves, int seedOffset)
    {
        var amplitude = 1.0f;
        var amplitudeTotal = 0.0f;
        var value = 0.0f;
        var x = worldX * frequency;
        var z = worldZ * frequency;

        for (var octave = 0; octave < octaves; octave++)
        {
            value += SmoothValueNoise(x, z, seedOffset + octave * 9973) * amplitude;
            amplitudeTotal += amplitude;
            amplitude *= 0.5f;
            x *= 2.0f;
            z *= 2.0f;
        }

        return amplitudeTotal > 0.0f ? value / amplitudeTotal : 0.0f;
    }

    private static float SmoothValueNoise(float x, float z, int seedOffset)
    {
        var x0 = (int)MathF.Floor(x);
        var z0 = (int)MathF.Floor(z);
        var tx = SmoothStep(x - x0);
        var tz = SmoothStep(z - z0);
        var a = Lerp(HashNoise(x0, z0, seedOffset), HashNoise(x0 + 1, z0, seedOffset), tx);
        var b = Lerp(HashNoise(x0, z0 + 1, seedOffset), HashNoise(x0 + 1, z0 + 1, seedOffset), tx);
        return Lerp(a, b, tz);
    }

    private static float HashNoise(int x, int z, int seedOffset)
    {
        unchecked
        {
            var value = (uint)(Seed + seedOffset);
            value ^= (uint)x * 0x9E3779B9u;
            value = (value << 13) | (value >> 19);
            value ^= (uint)z * 0x85EBCA6Bu;
            value ^= value >> 16;
            value *= 0x7FEB352Du;
            value ^= value >> 15;
            value *= 0x846CA68Bu;
            value ^= value >> 16;
            return value / (float)uint.MaxValue * 2.0f - 1.0f;
        }
    }

    private static float SmoothStep(float value)
    {
        return value * value * (3.0f - 2.0f * value);
    }

    private static float Lerp(float start, float end, float amount)
    {
        return start + (end - start) * amount;
    }
}
