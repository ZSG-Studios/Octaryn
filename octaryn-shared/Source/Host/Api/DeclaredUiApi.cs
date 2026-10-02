using System.Text;
using System.Text.Json;
using Octaryn.Shared.GameModules;

namespace Octaryn.Shared.Host.Api;

internal interface IDeclaredScreenBackend
{
    bool PresentDeclaredScreen(string declaration, string fields);
    bool HideDeclaredScreen(string id);
}

internal sealed class DeclaredUiApi(IHostUiApi backend, GameModuleManifest manifest, string moduleRoot) : IHostUiApi, IDisposable
{
    internal sealed record Screen(string Json, HashSet<string> Fields);
    private readonly Dictionary<string, Screen> _screens = new(StringComparer.Ordinal);
    private bool _disposed;
    private readonly bool _canPresent = (manifest.RequestedHostApis ?? []).Contains(ApiExposure.HostApiIds.Ui,StringComparer.Ordinal) &&
        (manifest.Schedule?.Systems ?? []).Any(system => (system.Writes ?? []).Any(resource =>
            resource.ResourceId == ApiExposure.HostApiIds.Ui && resource.Mode == Host.ScheduledAccessMode.Write));
    public bool ShowNotification(string text) => !_disposed && _canPresent && text is not null && text.Length <= 4096 && backend.ShowNotification(text);
    public bool TryPollAction(out string actionId)
    {
        if (_disposed) { actionId = ""; return false; }
        return backend.TryPollAction(out actionId);
    }

    public bool TryPresentScreen(string assetId, IReadOnlyDictionary<string, string> fields)
    {
        if (_disposed || !_canPresent || backend is not IDeclaredScreenBackend presentation || fields is null || fields.Count > 32 || assetId is null) return false;
        try
        {
            if (!_screens.TryGetValue(assetId, out var screen))
            {
                if (_screens.Count >= 16) return false;
                var files = new DeclaredUiFiles(manifest,moduleRoot);
                var json = files.ReadText(files.Asset(assetId,"ui"),8192);
                using var document = JsonDocument.Parse(json);
                var data = document.RootElement;
                Unique(data);
                if (data.GetProperty("version").GetInt32() != 1 || !Identifier(assetId) || data.GetProperty("screen_id").GetString() != assetId) return false;
                if(data.GetProperty("model").GetString()=="declared_document")screen=DeclaredUiDocument.Prepare(files,data,assetId);
                else if(data.GetProperty("model").GetString()=="module_panel")screen=Panel(data,json,assetId);
                else return false;
                if(screen is null || Encoding.UTF8.GetByteCount(screen.Json)>1024*1024 ||
                    _screens.Values.Sum(value=>Encoding.UTF8.GetByteCount(value.Json))+Encoding.UTF8.GetByteCount(screen.Json)>2*1024*1024)return false;
                _screens.Add(assetId,screen);
            }
            foreach (var (id, value) in fields)
                if (!screen.Fields.Contains(id) || value is null || value.Length > 4096 || value.Contains('\0')) return false;
            var values = JsonSerializer.Serialize(fields);
            return Encoding.UTF8.GetByteCount(values) <= 16384 && presentation.PresentDeclaredScreen(screen.Json, values);
        }
        catch (Exception failure) when (failure is IOException or UnauthorizedAccessException or ArgumentException or
            JsonException or KeyNotFoundException or InvalidOperationException or FormatException or OverflowException or System.Xml.XmlException or
            System.Text.RegularExpressions.RegexMatchTimeoutException) { return false; }
    }

    public bool HideScreen(string assetId) => !_disposed && _canPresent && assetId is not null && _screens.ContainsKey(assetId) && backend is IDeclaredScreenBackend presentation && presentation.HideDeclaredScreen(assetId);
    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        try
        {
            if (backend is IDeclaredScreenBackend presentation)
                foreach (var id in _screens.Keys) presentation.HideDeclaredScreen(id);
        }
        finally { _screens.Clear(); }
    }
    private static Screen? Panel(JsonElement data,string json,string assetId)
    {
        var title=data.GetProperty("title").GetString();
        if(string.IsNullOrEmpty(title) || title.Length>128)return null;
        var allowed=new HashSet<string>(StringComparer.Ordinal);
        foreach(var field in data.GetProperty("fields").EnumerateArray())
        {Unique(field);var id=field.GetProperty("id").GetString();if(!Identifier(id) || !Label(field) || !allowed.Add(id!))return null;}
        if(allowed.Count is 0 or >32)return null;
        var actions=new HashSet<string>(StringComparer.Ordinal);
        foreach(var action in data.GetProperty("actions").EnumerateArray())
        {
            Unique(action);var id=action.GetProperty("id").GetString();
            if(!Identifier(id) || !id!.StartsWith(assetId+".",StringComparison.Ordinal) || !Label(action) || !actions.Add(id))return null;
        }
        return actions.Count<=16?new(json,allowed):null;
    }
    private static bool Label(JsonElement element) => element.GetProperty("label").GetString() is { Length: <= 128 };
    internal static bool Identifier(string? value) => value is { Length: > 0 and <= 128 } &&
        value.All(c => char.IsAsciiLetterOrDigit(c) || c is '.' or '_' or '-');
    internal static void Unique(JsonElement value)
    {
        var names=new HashSet<string>(StringComparer.Ordinal);
        foreach(var property in value.EnumerateObject())if(!names.Add(property.Name))throw new InvalidOperationException("Duplicate UI declaration property.");
    }
}
