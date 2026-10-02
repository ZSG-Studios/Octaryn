namespace Octaryn.Shared.GameModules;

public static class GameModuleUiAssets
{
    public static bool IsUiKind(string kind) => kind is "ui" or "ui.document" or "ui.style" or "ui.resources";
    public static bool IsPassiveDeclaration(GameModuleAssetDeclaration asset)
    {
        var path=asset.RelativePath;
        if(string.IsNullOrWhiteSpace(path) || !path.StartsWith("Assets/Ui/",StringComparison.Ordinal) ||
            path.Contains('\\') || path.Contains(':') || path.Any(char.IsControl) ||
            path.Split('/').Any(part=>part.Length==0 || part is "." or ".." || part.EndsWith('.') || part.EndsWith(' ')))return false;
        var extension=Path.GetExtension(path);
        return asset.AssetKind switch
        {
            "ui.document"=>extension==".rml",
            "ui.style"=>extension==".rcss",
            "ui.resources"=>extension==".json",
            "ui"=>extension is ".rml" or ".rcss" or ".ttf" or ".txt" || Path.GetFileName(path)=="screen.json",
            _=>false
        };
    }
}
