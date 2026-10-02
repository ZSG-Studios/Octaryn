using System.Runtime.InteropServices;
using System.Text;
using Octaryn.Shared.GameModules;

namespace Octaryn.Shared.Host.Api;

[StructLayout(LayoutKind.Sequential, Pack = 8, Size = 24)]
internal unsafe struct HostContentApiTable
{
    public uint Version, Size;
    public delegate* unmanaged[Cdecl]<byte*, byte*, byte*, uint, uint*, int> ReadData;
    public uint MaximumReadBytes, Reserved;
}

internal unsafe sealed partial class NativeHostApiProvider
{
    public IHostContentApi? GetContentApi(GameModuleManifest manifest, string moduleRoot)
    {
        var declared = new DeclaredContentApi(manifest, moduleRoot);
        if (_query is null) return declared;
        var table = (HostContentApiTable*)_query(HostApiTableIds.Content, HostApiTableIds.ContentVersion);
        if (table is null) return declared; // Managed content belongs to the module host.
        if (table->Version < HostApiTableIds.ContentVersion || table->Size < sizeof(HostContentApiTable) ||
            table->ReadData is null || table->MaximumReadBytes == 0 ||
            table->MaximumReadBytes > IHostContentApi.MaximumReadBytes || table->Reserved != 0)
            return null;
        return new NativeContentApi(table, manifest.ModuleId, declared);
    }

    private sealed class NativeContentApi : IHostContentApi
    {
        private readonly HostContentApiTable* _table;
        private readonly byte[] _module;
        private readonly DeclaredContentApi _declarations;

        public NativeContentApi(HostContentApiTable* table, string module, DeclaredContentApi declarations)
        {
            _table = table;
            _module = Encoding.UTF8.GetBytes(module + '\0');
            _declarations = declarations;
        }

        public bool TryRead(string contentId, out ReadOnlyMemory<byte> data, out string error)
        {
            data = default;
            error = "Content ID is not declared by this module.";
            if (!_declarations.Contains(contentId)) return false;
            var id = Encoding.UTF8.GetBytes(contentId + '\0');
            fixed (byte* modulePointer = _module, idPointer = id)
            {
                uint requested = 0;
                var status = _table->ReadData(modulePointer, idPointer, null, 0, &requested);
                if (status != 0 || requested > _table->MaximumReadBytes)
                {
                    error = "Native declared content is unavailable or exceeds the read limit.";
                    return false;
                }
                var bytes = new byte[checked((int)requested)];
                uint actual = 0;
                fixed (byte* destination = bytes)
                    status = _table->ReadData(modulePointer, idPointer, destination, requested, &actual);
                if (status != 0 || actual != requested)
                {
                    error = "Native declared content changed or could not be read.";
                    return false;
                }
                data = bytes;
                error = string.Empty;
                return true;
            }
        }
    }
}
