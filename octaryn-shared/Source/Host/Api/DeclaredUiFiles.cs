using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using Octaryn.Shared.GameModules;

namespace Octaryn.Shared.Host.Api;

internal sealed class DeclaredUiFiles(GameModuleManifest manifest,string moduleRoot)
{
    internal sealed record Resource(string Path,string Kind,int Width,int Height,string Sampling="point",string AlphaSampling="premultiplied",string Addressing="clamp")
    {
        internal string ImageSource()
        {
            var policy=AlphaSampling=="straight"?(Sampling=="linear"?"straight-linear":"straight"):
                Sampling=="linear"?"linear":"";
            if(Addressing=="wrap")policy+=policy.Length==0?"wrap":"-wrap";
            return policy.Length==0?Path:"octaryn-ui-"+policy+":"+Path;
        }
    }
    internal string Asset(string id,string kind)
    {
        var asset=(manifest.AssetDeclarations ?? []).SingleOrDefault(a=>a.AssetId==id && a.AssetKind==kind);
        if(asset is null || !id.StartsWith(manifest.ModuleId+".",StringComparison.Ordinal))throw new InvalidOperationException("UI asset is undeclared.");
        return Confined(asset.RelativePath);
    }
    internal string Confined(string relative)
    {
        if(!relative.StartsWith("Assets/",StringComparison.Ordinal) || relative.Contains('\\') || relative.Contains(':') ||
            relative.Any(char.IsControl) || relative.Contains('\'') || relative.Contains('"') ||
            relative.Split('/').Any(p=>p.Length==0 || p is "." or ".." || p.EndsWith('.') || p.EndsWith(' ')))
            throw new InvalidOperationException("UI resource path is invalid.");
        var root=Path.TrimEndingDirectorySeparator(Path.GetFullPath(moduleRoot));
        var path=Path.GetFullPath(Path.Combine(root,relative));
        var comparison=OperatingSystem.IsWindows()?StringComparison.OrdinalIgnoreCase:StringComparison.Ordinal;
        if(!path.StartsWith(root+Path.DirectorySeparatorChar,comparison))throw new InvalidOperationException("UI resource escapes its module.");
        var current=root;
        if((File.GetAttributes(current)&FileAttributes.ReparsePoint)!=0)throw new InvalidOperationException("UI module root is a link.");
        foreach(var part in relative.Split('/'))
        {current=Path.Combine(current,part);if((File.GetAttributes(current)&FileAttributes.ReparsePoint)!=0)throw new InvalidOperationException("UI resource traverses a link.");}
        return path.Replace('\\','/');
    }
    internal byte[] Read(string path,int maximum)
    {
        using var input=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read);
        if(input.Length is <=0 || input.Length>maximum)throw new InvalidOperationException("UI resource exceeds admission.");
        var bytes=new byte[(int)input.Length];input.ReadExactly(bytes);
        if(input.ReadByte()!=-1)throw new InvalidOperationException("UI resource changed during admission.");return bytes;
    }
    internal string ReadText(string path,int maximum)
    {
        var text=new UTF8Encoding(false,true).GetString(Read(path,maximum));
        return text.StartsWith('\uFEFF')?text[1..]:text;
    }
    internal Dictionary<string,Resource> Resources(string assetId)
    {
        var path=Asset(assetId,"ui.resources");
        using var index=JsonDocument.Parse(Read(path,256*1024));
        var root=index.RootElement;DeclaredUiApi.Unique(root);
        if(root.GetProperty("version").GetInt32()!=1)throw new InvalidOperationException("Unknown UI resource index.");
        var result=new Dictionary<string,Resource>(StringComparer.Ordinal);long bytes=0,pixels=0;var fonts=0;
        var relativeDirectory=Path.GetRelativePath(Path.GetFullPath(moduleRoot),Path.GetDirectoryName(path)!).Replace('\\','/');
        var paths=new HashSet<string>(OperatingSystem.IsWindows()?StringComparer.OrdinalIgnoreCase:StringComparer.Ordinal);
        foreach(var entry in root.GetProperty("resources").EnumerateArray())
        {
            DeclaredUiApi.Unique(entry);var id=entry.GetProperty("id").GetString();var relative=entry.GetProperty("path").GetString();
            var kind=entry.GetProperty("kind").GetString();var hash=entry.GetProperty("sha256").GetString();
            var sampling=entry.TryGetProperty("sampling",out var filter)?filter.GetString():"point";
            if(sampling is not ("point" or "linear") || (filter.ValueKind!=JsonValueKind.Undefined && kind!="image"))
                throw new InvalidOperationException("UI sampling requires an image and point or linear filtering.");
            var alphaSampling=entry.TryGetProperty("alphaSampling",out var alpha)?alpha.GetString():"premultiplied";
            if(alphaSampling is not ("premultiplied" or "straight") || (alpha.ValueKind!=JsonValueKind.Undefined && kind!="image"))
                throw new InvalidOperationException("UI alpha sampling requires an image and premultiplied or straight filtering.");
            var addressing=entry.TryGetProperty("addressing",out var address)?address.GetString():"clamp";
            if(addressing is not ("clamp" or "wrap") || (address.ValueKind!=JsonValueKind.Undefined && kind!="image"))
                throw new InvalidOperationException("UI addressing requires an image and clamp or wrap addressing.");
            if(!DeclaredUiApi.Identifier(id) || relative is null || kind is not ("image" or "font" or "font.bitmap" or "ui.mesh2d") || result.Count>=256 ||
                hash is null || hash.Length!=64 || hash.Any(c=>!char.IsAsciiHexDigit(c) || char.IsUpper(c)))
                throw new InvalidOperationException("Invalid UI resource declaration.");
            var resource=Confined(relativeDirectory+"/"+relative);
            if(!paths.Add(resource))throw new InvalidOperationException("Duplicate UI resource path.");
            var content=Read(resource,4*1024*1024);bytes+=content.Length;
            if(bytes>64*1024*1024 || Convert.ToHexStringLower(SHA256.HashData(content))!=hash)
                throw new InvalidOperationException("UI resource identity or aggregate admission differs.");
            var imageWidth=0;var imageHeight=0;
            if(kind=="font") {if(++fonts>4 || !resource.EndsWith(".ttf",StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("UI font admission exceeded.");}
            else if(kind=="font.bitmap") {if(++fonts>4 || !resource.EndsWith(".json",StringComparison.OrdinalIgnoreCase) || content.Length>256*1024)throw new InvalidOperationException("UI bitmap font admission exceeded.");}
            else if(kind=="ui.mesh2d") {if(!resource.EndsWith(".json",StringComparison.OrdinalIgnoreCase) || content.Length>128*1024)throw new InvalidOperationException("UI mesh resource admission exceeded.");}
            else
            {
                if(!resource.EndsWith(".png",StringComparison.OrdinalIgnoreCase) || content.Length<33 ||
                    !content.AsSpan(0,8).SequenceEqual(new byte[]{137,80,78,71,13,10,26,10}) ||
                    !content.AsSpan(12,4).SequenceEqual("IHDR"u8))throw new InvalidOperationException("UI image must be bounded PNG.");
                var width=System.Buffers.Binary.BinaryPrimitives.ReadUInt32BigEndian(content.AsSpan(16,4));
                var height=System.Buffers.Binary.BinaryPrimitives.ReadUInt32BigEndian(content.AsSpan(20,4));
                if(width is 0 or >4096 || height is 0 or >4096)throw new InvalidOperationException("UI image dimension admission exceeded.");
                pixels+=(long)width*height;if(pixels>16*1024*1024)throw new InvalidOperationException("UI image decode admission exceeded.");
                imageWidth=(int)width;imageHeight=(int)height;
            }
            if(!result.TryAdd(id!,new(resource,kind,imageWidth,imageHeight,sampling!,alphaSampling!,addressing!)))throw new InvalidOperationException("Duplicate UI resource ID.");
        }
        return result;
    }
}
