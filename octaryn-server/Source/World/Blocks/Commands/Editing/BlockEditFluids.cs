using System.Runtime.InteropServices;

namespace Octaryn.Server.World.Blocks;

internal sealed unsafe partial class BlockEditService
{
    internal NativeFluidTickReport TickFluids(IntPtr simulation, BlockChangeQueue? queue, double deltaSeconds)
    {
        var context = GCHandle.Alloc(this);
        try
        {
            var report = default(NativeFluidTickReport);
            var result = NativeFluidLibrary.Tick(simulation, blocks.NativeHandle,
                queue?.NativeHandle ?? IntPtr.Zero, deltaSeconds, &GeneratedBlock,
                &IsKnownBlock, &CanApplyEdit, &CanStaySupported,
                (void*)GCHandle.ToIntPtr(context), &report);
            if (result != 0)
                throw new InvalidOperationException($"Native fluid step failed: {result}.");
            return report;
        }
        finally { context.Free(); }
    }
}
