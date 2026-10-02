namespace Octaryn.Shared.Host.Api;

internal sealed unsafe partial class NativeHostApiProvider
{
    public IHostGraphicsApi? GetGraphicsApi()
    {
        if (_query is null) return null;
        var table = (HostGraphicsApiTable*)_query(HostApiTableIds.Graphics, HostApiTableIds.GraphicsVersion);
        return table is null || table->Version < 1 || table->Size < sizeof(HostGraphicsApiTable) ||
            table->Get is null || table->Apply is null ? null : new NativeGraphicsApi(table);
    }
    private sealed class NativeGraphicsApi(HostGraphicsApiTable* table) : IHostGraphicsApi
    {
        public bool TryGet(out HostGraphicsSettings settings)
        {
            settings = default;
            var wire = new HostGraphicsNative { Version = 1, Size = 88 };
            if (table->Get(&wire) != 0 || wire.Version != 1 || wire.Size != 88 || (wire.Flags & ~4095u) != 0 ||
                (wire.Capabilities & ~1u) != 0) return false;
            var candidate = wire.Managed();
            if (!HostGraphicsValidation.Valid(candidate)) return false;
            settings = candidate; return true;
        }
        public HostGraphicsApplyResult Apply(in HostGraphicsSettings settings, bool persist)
        {
            if (!HostGraphicsValidation.Valid(settings)) return HostGraphicsApplyResult.Rejected;
            var wire = HostGraphicsNative.From(settings);
            var result = table->Apply(&wire, persist ? 1u : 0);
            return result is >= 0 and <= 3 ? (HostGraphicsApplyResult)result : HostGraphicsApplyResult.Unavailable;
        }
    }
}
