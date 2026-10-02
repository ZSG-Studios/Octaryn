using System.Text.Json;
using System.Xml.Linq;

namespace Octaryn.Shared.Host.Api;

internal static class DeclaredUiMeshes
{
    internal sealed record Vertex(double x,double y,double u,double v);
    internal sealed record Mesh(string element,string texture,Vertex[] vertices,int[] indices,bool wrap);

    internal static Mesh[] Prepare(DeclaredUiFiles files,Dictionary<string,DeclaredUiFiles.Resource> resources,
        JsonElement declaration,XDocument document,HashSet<string> mutable)
    {
        if(!declaration.TryGetProperty("meshes",out var bindings))return [];
        if(bindings.ValueKind!=JsonValueKind.Array)throw new InvalidOperationException("UI mesh bindings require an array.");
        var result=new List<Mesh>();var targets=new HashSet<string>(StringComparer.Ordinal);var vertexCount=0;var indexCount=0;
        foreach(var binding in bindings.EnumerateArray())
        {
            DeclaredUiApi.Unique(binding);
            var element=binding.GetProperty("element").GetString();var id=binding.GetProperty("resource").GetString();
            var target=document.Root!.DescendantsAndSelf().SingleOrDefault(e=>(string?)e.Attribute("id")==element);
            if(result.Count>=64 || !DeclaredUiApi.Identifier(element) || !targets.Add(element!) || mutable.Contains(element!) ||
                target?.Name!="div" || target.Nodes().Any() || id is null ||
                !resources.TryGetValue(id,out var resource) || resource.Kind!="ui.mesh2d")
                throw new InvalidOperationException("UI mesh requires an indexed resource and an empty noninteractive div.");
            using var input=JsonDocument.Parse(files.Read(resource.Path,128*1024));
            var root=input.RootElement;DeclaredUiApi.Unique(root);
            if(root.GetProperty("version").GetInt32()!=1)throw new InvalidOperationException("Unknown UI mesh schema.");
            var textureId=root.GetProperty("texture").GetString();
            if(textureId is null || !resources.TryGetValue(textureId,out var texture) || texture.Kind!="image")
                throw new InvalidOperationException("UI mesh texture must be indexed in the same resource set.");
            var vertices=new List<Vertex>();
            foreach(var value in root.GetProperty("vertices").EnumerateArray())
            {
                DeclaredUiApi.Unique(value);
                if(vertices.Count>=256)throw new InvalidOperationException("UI mesh vertex budget exceeded.");
                vertices.Add(new(Number(value,"x",16384),Number(value,"y",16384),Number(value,"u",64),Number(value,"v",64)));
            }
            var indices=new List<int>();
            foreach(var value in root.GetProperty("indices").EnumerateArray())
            {
                var index=value.GetInt32();
                if(indices.Count>=768 || index<0 || index>=vertices.Count)throw new InvalidOperationException("UI mesh index admission differs.");
                indices.Add(index);
            }
            if(vertices.Count<3 || indices.Count<3 || indices.Count%3!=0 ||
                (vertexCount+=vertices.Count)>4096 || (indexCount+=indices.Count)>12288)
                throw new InvalidOperationException("UI mesh aggregate geometry admission exceeded.");
            var wrap=root.TryGetProperty("wrap",out var wrapping) && wrapping.GetBoolean();
            result.Add(new(element!,texture.ImageSource(),vertices.ToArray(),indices.ToArray(),wrap));
        }
        return result.ToArray();
    }
    private static double Number(JsonElement value,string key,double bound)
    {
        var number=value.GetProperty(key).GetDouble();
        if(!double.IsFinite(number) || Math.Abs(number)>bound)throw new InvalidOperationException("UI mesh coordinate admission differs.");
        return number;
    }
}
