using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;

namespace Octaryn.Shared.Host.Api;

internal static class DeclaredUiSprites
{
    private static readonly Regex Sheet=new("@spritesheet(?:\\s+[a-zA-Z0-9_.-]+)?\\s*\\{([^{}]*)\\}",
        RegexOptions.IgnoreCase|RegexOptions.CultureInvariant,TimeSpan.FromSeconds(1));
    internal static string Resolve(Dictionary<string,DeclaredUiFiles.Resource> resources,string css)
    {
        if(DeclaredUiAnimations.StripValidated(Sheet.Replace(css,"")).Contains('@'))
            throw new InvalidOperationException("Unsupported UI stylesheet at-rule.");
        var count=0;
        return Sheet.Replace(css,match=>
        {
            if(++count>256)throw new InvalidOperationException("UI spritesheet count admission exceeded.");
            var entries=new Dictionary<string,string>(StringComparer.Ordinal);
            foreach(var row in match.Groups[1].Value.Split(';'))
            {
                var text=row.Trim();if(text.Length==0)continue;var colon=text.IndexOf(':');
                if(colon<1 || entries.Count>=8192)throw new InvalidOperationException("Malformed UI sprite property.");
                var key=text[..colon].Trim();var value=text[(colon+1)..].Trim();
                if(!DeclaredUiApi.Identifier(key) || !entries.TryAdd(key,value))throw new InvalidOperationException("Duplicate UI sprite property.");
            }
            if(!entries.TryGetValue("src",out var token))throw new InvalidOperationException("UI spritesheet source is absent.");
            token=token.Trim('"','\'');
            if(!token.StartsWith("asset:",StringComparison.Ordinal) || !resources.TryGetValue(token[6..],out var image) || image.Kind!="image")
                throw new InvalidOperationException("UI spritesheet source is not indexed.");
            var output=new StringBuilder(match.Value[..match.Value.IndexOf('{')]);output.Append("{src:\"").Append(image.ImageSource()).Append("\";");
            foreach(var (key,value) in entries)
            {
                if(key=="src")continue;
                if(key=="resolution")
                {
                    var resolution=value.Replace(" ","");
                    if(!resolution.EndsWith('x') || !double.TryParse(resolution[..^1],NumberStyles.AllowDecimalPoint,CultureInfo.InvariantCulture,out var scale) ||
                        !double.IsFinite(scale) || scale<=0 || scale>16)throw new InvalidOperationException("Invalid UI sprite resolution.");
                }
                else
                {
                    var parts=value.Split((char[]?)null,StringSplitOptions.RemoveEmptyEntries);
                    if(parts.Length!=4)throw new InvalidOperationException("Invalid UI sprite rectangle.");
                    var rectangle=new double[4];
                    for(var index=0;index<parts.Length;++index)
                        if(!parts[index].EndsWith("px",StringComparison.Ordinal) ||
                            !double.TryParse(parts[index][..^2],NumberStyles.AllowDecimalPoint,CultureInfo.InvariantCulture,out rectangle[index]) ||
                            !double.IsFinite(rectangle[index]) || rectangle[index]<0)throw new InvalidOperationException("Invalid UI sprite coordinate.");
                    if(rectangle[2]<=0 || rectangle[3]<=0 || rectangle[0]+rectangle[2]>image.Width || rectangle[1]+rectangle[3]>image.Height)
                        throw new InvalidOperationException("UI sprite rectangle escapes its indexed image.");
                }
                output.Append(key).Append(':').Append(value).Append(';');
            }
            return output.Append('}').ToString();
        });
    }
}
