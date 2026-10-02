using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Xml;
using System.Xml.Linq;

namespace Octaryn.Shared.Host.Api;

internal static class DeclaredUiDocument
{
    private sealed record Binding(string id,string element,string kind="text",string font="",string align="left",double scale=1,double wrap_width=0,bool fit_text_height=false,bool wrap_to_element=false);
    private sealed record Envelope(int version,string model,string screen_id,bool modal,string markup,
        string[] fonts,Binding[] fields,Binding[] actions,int image_count,DeclaredUiBitmapFonts.Font[] bitmap_fonts,int logical_width,int logical_height,
        DeclaredUiMeshes.Mesh[] meshes,string fit_mode);
    private static readonly Regex Url=new("""url\(\s*(['"]?)([^'"\)\s]+)\1\s*\)""",
        RegexOptions.IgnoreCase|RegexOptions.CultureInvariant,TimeSpan.FromSeconds(1));
    private static readonly HashSet<string> Tags=new(StringComparer.Ordinal)
    {"rml","head","title","style","body","div","span","p","img","button","input","label","h1","h2","h3","br","hr","textarea","select","option","progress"};

    internal static DeclaredUiApi.Screen Prepare(DeclaredUiFiles files,JsonElement declaration,string id)
    {
        var markup=files.ReadText(files.Asset(declaration.GetProperty("document").GetString()!,"ui.document"),256*1024);
        var resources=files.Resources(declaration.GetProperty("resources").GetString()!);
        var modal=declaration.GetProperty("modal").GetBoolean();
        var logicalWidth=declaration.TryGetProperty("logical_width",out var width)?width.GetInt32():0;
        var logicalHeight=declaration.TryGetProperty("logical_height",out var height)?height.GetInt32():0;
        var fitMode=declaration.TryGetProperty("fit_mode",out var mode)?mode.GetString():"contain";
        if(fitMode is not ("contain" or "height") || (fitMode=="height" && logicalHeight==0))
            throw new InvalidOperationException("Declared UI fit mode differs.");
        if((logicalWidth!=0 || logicalHeight!=0) && (logicalWidth<64 || logicalWidth>4096 || logicalHeight<64 || logicalHeight>4096))
            throw new InvalidOperationException("Declared UI logical viewport differs.");
        var styles=new Dictionary<string,string>(StringComparer.Ordinal);var styleBytes=0;
        foreach(var style in declaration.GetProperty("styles").EnumerateArray())
        {
            var asset=style.GetString();
            if(asset is null || styles.Count>=8 || styles.ContainsKey(asset))throw new InvalidOperationException("UI stylesheet admission differs.");
            var styleText=files.ReadText(files.Asset(asset,"ui.style"),128*1024);styles.Add(asset,styleText);
            styleBytes+=Encoding.UTF8.GetByteCount(styleText);
            if(styleBytes>128*1024)throw new InvalidOperationException("UI style aggregate admission exceeded.");
        }
        var fonts=new List<string>();
        foreach(var font in declaration.GetProperty("fonts").EnumerateArray())
        {
            var token=font.GetString();
            if(token is null || !resources.TryGetValue(token,out var resource) || resource.Kind!="font" ||
                fonts.Count>=4 || fonts.Contains(resource.Path))throw new InvalidOperationException("UI font is not indexed.");
            fonts.Add(resource.Path);
        }
        using var input=new StringReader(markup);
        using var reader=XmlReader.Create(input,new XmlReaderSettings
        {DtdProcessing=DtdProcessing.Prohibit,XmlResolver=null,MaxCharactersInDocument=256*1024,IgnoreProcessingInstructions=true});
        var document=XDocument.Load(reader,LoadOptions.PreserveWhitespace);
        if(document.Root?.Name!="rml")throw new InvalidOperationException("UI document requires an rml root.");
        var elements=new HashSet<string>(StringComparer.Ordinal);var count=0;
        foreach(var element in document.Root.DescendantsAndSelf())
        {
            if(++count>8192 || element.Ancestors().Take(65).Count()>64 || element.Name.NamespaceName.Length!=0 ||
                !Tags.Contains(element.Name.LocalName))throw new InvalidOperationException("UI document tag or element admission exceeded.");
            foreach(var attribute in element.Attributes().ToArray())
            {
                var name=attribute.Name.LocalName;
                if(attribute.Name.NamespaceName.Length!=0 || name.StartsWith("on",StringComparison.OrdinalIgnoreCase) ||
                    name.StartsWith("data-",StringComparison.OrdinalIgnoreCase) || name is "href" or "template" ||
                    attribute.Value.Contains('\0'))throw new InvalidOperationException("UI document has unsupported active markup.");
                if(name=="id" && (!DeclaredUiApi.Identifier(attribute.Value) || !elements.Add(attribute.Value)))
                    throw new InvalidOperationException("UI document element IDs must be unique.");
                if(name=="src")
                {
                    if(element.Name.LocalName!="img")throw new InvalidOperationException("UI source attributes require images.");
                    attribute.Value=Image(resources,attribute.Value);
                }
                if(name=="style")attribute.Value=Css(resources,attribute.Value);
            }
            if(element.Name.LocalName=="style")element.Value=Css(resources,element.Value);
        }
        if(styles.Count>0)
        {
            var head=document.Root.Element("head");
            if(head is null){head=new XElement("head");document.Root.AddFirst(head);}
            foreach(var style in styles.Values)head.Add(new XElement("style",Css(resources,style)));
        }
        var fields=Bindings(declaration.GetProperty("fields"),elements,32,null);
        var actions=Bindings(declaration.GetProperty("actions"),elements,64,id+".");
        var meshes=DeclaredUiMeshes.Prepare(files,resources,declaration,document,
            fields.Concat(actions).Select(field=>field.element).ToHashSet(StringComparer.Ordinal));
        var bitmap=DeclaredUiBitmapFonts.Prepare(files,resources,fields.Where(field=>field.kind=="bitmap_text").Select(field=>field.font));
        if(styleBytes+Encoding.UTF8.GetByteCount(bitmap.Css)>128*1024)throw new InvalidOperationException("UI style aggregate admission exceeded.");
        if(bitmap.Css.Length>0)
        {
            var head=document.Root.Element("head");
            if(head is null){head=new XElement("head");document.Root.AddFirst(head);}
            head.Add(new XElement("style",bitmap.Css));
        }
        // Action bindings may not be destroyed by a mutable text field update.
        foreach(var field in fields)
        {
            var element=document.Root.DescendantsAndSelf().Single(e=>(string?)e.Attribute("id")==field.element);
            if(element.HasElements || actions.Any(action=>action.element==field.element))
                throw new InvalidOperationException("UI text fields require noninteractive leaf elements.");
        }
        var output=document.ToString(SaveOptions.DisableFormatting);
        if(Encoding.UTF8.GetByteCount(output)>512*1024)throw new InvalidOperationException("Expanded UI markup admission exceeded.");
        return new(JsonSerializer.Serialize(new Envelope(2,"declared_document",id,modal,output,fonts.ToArray(),fields,actions,
                resources.Values.Count(resource=>resource.Kind=="image"),bitmap.Fonts,logicalWidth,logicalHeight,meshes,fitMode)),
            fields.Select(binding=>binding.id).ToHashSet(StringComparer.Ordinal));
    }
    private static Binding[] Bindings(JsonElement list,HashSet<string> elements,int maximum,string? prefix)
    {
        var result=new List<Binding>();var ids=new HashSet<string>(StringComparer.Ordinal);var targets=new HashSet<string>(StringComparer.Ordinal);
        foreach(var item in list.EnumerateArray())
        {
            DeclaredUiApi.Unique(item);var id=item.GetProperty("id").GetString();var element=item.GetProperty("element").GetString();
            if(result.Count>=maximum || !DeclaredUiApi.Identifier(id) || !DeclaredUiApi.Identifier(element) ||
                !elements.Contains(element!) || !ids.Add(id!) || !targets.Add(element!) ||
                (prefix is not null && !id!.StartsWith(prefix,StringComparison.Ordinal)))
                throw new InvalidOperationException("Invalid UI binding.");
            var kind=item.TryGetProperty("kind",out var kindValue)?kindValue.GetString():"text";
            var font=item.TryGetProperty("font",out var fontValue)?fontValue.GetString():"";
            var align=item.TryGetProperty("align",out var alignValue)?alignValue.GetString():"left";
            var scale=item.TryGetProperty("scale",out var scaleValue)?scaleValue.GetDouble():1;
            var wrap=item.TryGetProperty("wrap_width",out var wrapValue)?wrapValue.GetDouble():0;
            var fitHeight=item.TryGetProperty("fit_text_height",out var fitValue) && fitValue.GetBoolean();
            var wrapToElement=item.TryGetProperty("wrap_to_element",out var wrapTarget) && wrapTarget.GetBoolean();
            if(kind is not ("text" or "bitmap_text") || (prefix is not null && kind!="text") ||
                align is not ("left" or "center" or "right") || !double.IsFinite(scale) || scale<.1 || scale>8 ||
                !double.IsFinite(wrap) || wrap<0 || wrap>16384 || (kind!="bitmap_text" && (wrap!=0 || fitHeight || wrapToElement)) ||
                (kind=="bitmap_text" && !DeclaredUiApi.Identifier(font)))throw new InvalidOperationException("Invalid UI field kind or font.");
            result.Add(new(id!,element!,kind!,font??"",align!,scale,wrap,fitHeight,wrapToElement));
        }
        return result.ToArray();
    }
    private static string Image(Dictionary<string,DeclaredUiFiles.Resource> resources,string token)
    {
        if(!token.StartsWith("asset:",StringComparison.Ordinal) || !resources.TryGetValue(token[6..],out var resource) || resource.Kind!="image")
            throw new InvalidOperationException("UI image reference is not indexed.");
        return resource.ImageSource();
    }
    private static string Css(Dictionary<string,DeclaredUiFiles.Resource> resources,string css)
    {
        if(css.Contains('\\') || css.Contains('\0'))throw new InvalidOperationException("UI stylesheet has unsupported escapes.");
        css=DeclaredUiSprites.Resolve(resources,css);
        // Resource-bearing decorators need an explicit grammar before admission.
        if(css.Contains("decorator",StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("UI image decorators require explicit admission.");
        var remainder=Url.Replace(css,"");
        if(remainder.Contains("url",StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException("Malformed UI resource URL.");
        return Url.Replace(css,match=>"url(\""+Image(resources,match.Groups[2].Value)+"\")");
    }
}
