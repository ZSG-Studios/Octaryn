using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Octaryn.Shared.World;

namespace Octaryn.Server.World.Generation;

internal sealed unsafe class TerrainGenerator : IDisposable
{
    private readonly IWorldGenerationRules _rules;
    private GCHandle _handle;

    public TerrainGenerator(IWorldGenerationRules rules)
    {
        _rules = rules;
        _handle = GCHandle.Alloc(this);
    }

    public BlockId GetGeneratedBlock(BlockPosition position)
    {
        ObjectDisposedException.ThrowIf(!_handle.IsAllocated, this);
        ushort block = 0;
        var result = NativeTerrainGenerationLibrary.GeneratedBlock(
            position.X,
            position.Y,
            position.Z,
            _rules.WaterHeight,
            _rules.WaterBlock.Value,
            &PlanColumn,
            (void*)GCHandle.ToIntPtr(_handle),
            &block);
        if (result != 0)
        {
            throw new InvalidOperationException("Native terrain generation failed.");
        }

        return new BlockId(block);
    }

    public void Dispose()
    {
        if (!_handle.IsAllocated)
        {
            return;
        }

        _handle.Free();
        GC.SuppressFinalize(this);
    }

    ~TerrainGenerator()
    {
        Dispose();
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    private static int PlanColumn(void* context, NativeTerrainColumnSample* sample, NativeTerrainColumnPlan* plan)
    {
        if (context is null || sample is null || plan is null)
        {
            return -1;
        }

        try
        {
            var handle = GCHandle.FromIntPtr((IntPtr)context);
            if (handle.Target is not TerrainGenerator generator)
            {
                return -1;
            }

            *plan = NativeTerrainColumnPlan.FromTerrainColumnPlan(generator._rules.PlanTerrainColumn(sample->ToTerrainColumnSample()));
            return 0;
        }
        catch
        {
            return -1;
        }
    }
}
