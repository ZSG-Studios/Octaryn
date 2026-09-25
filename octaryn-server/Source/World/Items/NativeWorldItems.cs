using System.Runtime.InteropServices;

namespace Octaryn.Server.World.Items;

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct ItemState
{
    public uint Version, Size;
    public ulong NextItem, NextGrant, LastCommand, AcknowledgedGrant;
    public double Seconds, NextDrop;
    public uint ReceiptBlock, ReceiptCount, ReceiptResult, ItemCount, GrantCount, Reserved;
    public fixed byte Items[256 * 56];
    public fixed byte Grants[64 * 16];
}

internal static unsafe class NativeWorldItems
{
    internal static readonly delegate* unmanaged[Cdecl]<ItemState*, void> Initialize;
    internal static readonly delegate* unmanaged[Cdecl]<ItemState*, int> Validate;
    internal static readonly delegate* unmanaged[Cdecl]<ItemState*, ulong, uint, uint, uint,
        float, float, float, float, float, int> Drop;
    internal static readonly delegate* unmanaged[Cdecl]<ItemState*, ulong, int> Acknowledge;
    internal static readonly delegate* unmanaged[Cdecl]<ItemState*, double, float, float, float,
        delegate* unmanaged[Cdecl]<void*, int, int, int, uint>, void*, int> Tick;

    static NativeWorldItems()
    {
        if (sizeof(ItemState) != 15440 || !BitConverter.IsLittleEndian)
            throw new PlatformNotSupportedException("World items require the little-endian v1 ABI.");
        const string name = "octaryn_server_world_items";
        var explicitPath = Environment.GetEnvironmentVariable("OCTARYN_SERVER_WORLD_ITEMS_LIBRARY");
        var file = OperatingSystem.IsWindows() ? name + ".dll" :
            OperatingSystem.IsMacOS() ? "lib" + name + ".dylib" : "lib" + name + ".so";
        var adjacent = Path.Combine(Path.GetDirectoryName(typeof(NativeWorldItems).Assembly.Location) ?? "", file);
        var library = NativeLibrary.Load(!string.IsNullOrWhiteSpace(explicitPath) ? explicitPath :
            File.Exists(adjacent) ? adjacent : Path.Combine(AppContext.BaseDirectory, file));
        Initialize = (delegate* unmanaged[Cdecl]<ItemState*, void>)NativeLibrary.GetExport(library, "octaryn_items_initialize");
        Validate = (delegate* unmanaged[Cdecl]<ItemState*, int>)NativeLibrary.GetExport(library, "octaryn_items_validate");
        Drop = (delegate* unmanaged[Cdecl]<ItemState*, ulong, uint, uint, uint, float, float, float, float, float, int>)
            NativeLibrary.GetExport(library, "octaryn_items_drop");
        Acknowledge = (delegate* unmanaged[Cdecl]<ItemState*, ulong, int>)NativeLibrary.GetExport(library, "octaryn_items_acknowledge");
        Tick = (delegate* unmanaged[Cdecl]<ItemState*, double, float, float, float,
            delegate* unmanaged[Cdecl]<void*, int, int, int, uint>, void*, int>)
            NativeLibrary.GetExport(library, "octaryn_items_tick");
    }
}
