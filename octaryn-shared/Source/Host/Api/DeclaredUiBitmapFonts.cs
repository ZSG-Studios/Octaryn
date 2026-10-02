using System.Globalization;
using System.Text;
using System.Text.Json;

namespace Octaryn.Shared.Host.Api;

internal static class DeclaredUiBitmapFonts
{
    internal sealed record Glyph(int codepoint,string sprite,double width,double height,double advance,double bearing_x,double bearing_y);
    internal sealed record Font(string id,double height,Glyph[] glyphs,double line_height,double measured_height);
    internal sealed record Prepared(Font[] Fonts,string Css);

    internal static Prepared Prepare(DeclaredUiFiles files,Dictionary<string,DeclaredUiFiles.Resource> resources,IEnumerable<string> identifiers)
    {
        var fonts=new List<Font>();var css=new StringBuilder();
        foreach(var id in identifiers.Distinct(StringComparer.Ordinal))
        {
            if(fonts.Count>=4 || id.Length>96 || !resources.TryGetValue(id,out var resource) || resource.Kind!="font.bitmap")
                throw new InvalidOperationException("Bitmap text font is not indexed.");
            using var document=JsonDocument.Parse(files.Read(resource.Path,256*1024));
            var root=document.RootElement;DeclaredUiApi.Unique(root);
            if(root.GetProperty("version").GetInt32()!=1)throw new InvalidOperationException("Unknown bitmap font schema.");
            var height=Number(root,"height",.1,512);
            var lineHeight=root.TryGetProperty("lineHeight",out _)?Number(root,"lineHeight",.1,512):height;
            var measuredHeight=root.TryGetProperty("measuredHeight",out _)?Number(root,"measuredHeight",.1,512):height;
            var atlasId=root.GetProperty("atlas").GetString();
            if(atlasId is null || !resources.TryGetValue(atlasId,out var atlas) || atlas.Kind!="image")
                throw new InvalidOperationException("Bitmap font atlas is not indexed.");
            var supported=new HashSet<int>();
            foreach(var value in root.GetProperty("supportedCodepoints").EnumerateArray())
            {
                var code=value.GetInt32();
                if(supported.Count>=256 || code<32 || code>0x10ffff || code is >=0xd800 and <=0xdfff || !supported.Add(code))
                    throw new InvalidOperationException("Bitmap font character admission differs.");
            }
            if(supported.Count==0)throw new InvalidOperationException("Bitmap font has no admitted characters.");
            var glyphs=new Dictionary<int,Glyph>();var indices=new HashSet<int>();var count=0;
            css.Append("@spritesheet host_bitmap_").Append(id).Append(" { src: \"").Append(atlas.ImageSource()).Append("\";");
            foreach(var entry in root.GetProperty("glyphs").EnumerateArray())
            {
                DeclaredUiApi.Unique(entry);
                var index=entry.GetProperty("sourceIndex").GetInt32();
                if(++count>256 || index is <0 or >255 || !indices.Add(index))throw new InvalidOperationException("Duplicate bitmap glyph index.");
                var x=Number(entry,"x",0,atlas.Width);var y=Number(entry,"y",0,atlas.Height);
                var w=Number(entry,"width",0,atlas.Width);var h=Number(entry,"height",0,atlas.Height);
                if(x+w>atlas.Width+.00001 || y+h>atlas.Height+.00001)throw new InvalidOperationException("Bitmap glyph escaped its atlas.");
                var drawWidth=Number(entry,"renderWidth",0,16384);var drawHeight=Number(entry,"renderHeight",0,16384);
                var advance=Number(entry,"advance",0,16384);var bx=Number(entry,"bearingX",-16384,16384);var by=Number(entry,"bearingY",-16384,16384);
                if(entry.GetProperty("codepoint").ValueKind==JsonValueKind.Null)continue;
                var code=entry.GetProperty("codepoint").GetInt32();if(!supported.Contains(code))continue;
                var sprite="host_bitmap_"+id+"_"+index.ToString(CultureInfo.InvariantCulture);
                if(!glyphs.TryAdd(code,new(code,sprite,drawWidth,drawHeight,advance,bx,by)))throw new InvalidOperationException("Duplicate bitmap character mapping.");
                if(w>0 && h>0 && drawWidth>0 && drawHeight>0)
                    css.Append(sprite).Append(':').Append(Text(x)).Append(' ').Append(Text(y)).Append(' ').Append(Text(w)).Append(' ').Append(Text(h)).Append(';');
            }
            css.Append('}');
            if(count!=256 || glyphs.Count!=supported.Count)throw new InvalidOperationException("Bitmap font mapping is incomplete.");
            fonts.Add(new(id,height,glyphs.Values.OrderBy(glyph=>glyph.codepoint).ToArray(),lineHeight,measuredHeight));
        }
        if(css.Length>128*1024)throw new InvalidOperationException("Bitmap font stylesheet admission exceeded.");
        return new(fonts.ToArray(),css.ToString());
    }
    private static double Number(JsonElement element,string key,double minimum,double maximum)
    {
        var value=element.GetProperty(key).GetDouble();
        if(!double.IsFinite(value) || value<minimum || value>maximum)throw new InvalidOperationException("Bitmap glyph metric admission exceeded.");
        return value;
    }
    private static string Text(double value)=>value.ToString("G12",CultureInfo.InvariantCulture)+"px";
}
