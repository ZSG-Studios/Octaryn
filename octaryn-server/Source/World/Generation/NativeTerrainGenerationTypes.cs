using System.Runtime.InteropServices;
namespace Octaryn.Server.World.Generation;

[StructLayout(LayoutKind.Sequential)]
internal readonly struct NativeTerrainMaterialRules(
    int waterHeight,
    ushort waterBlock,
    ushort sandBlock,
    ushort grassBlock,
    ushort dirtBlock,
    ushort stoneBlock,
    ushort snowBlock)
{
    public readonly int WaterHeight = waterHeight;
    public readonly ushort WaterBlock = waterBlock;
    public readonly ushort SandBlock = sandBlock;
    public readonly ushort GrassBlock = grassBlock;
    public readonly ushort DirtBlock = dirtBlock;
    public readonly ushort StoneBlock = stoneBlock;
    public readonly ushort SnowBlock = snowBlock;
}
