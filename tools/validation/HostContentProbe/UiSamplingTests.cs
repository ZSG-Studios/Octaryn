using System.Security.Cryptography;
using System.Text.Json;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host.Api;

internal static unsafe partial class Program
{
    private static void VerifyUiSampling(string root)
    {
        var directory=Path.Combine(root,"Assets","Ui");Directory.CreateDirectory(directory);
        var png=new byte[33];new byte[]{137,80,78,71,13,10,26,10}.CopyTo(png,0);
        "IHDR"u8.CopyTo(png.AsSpan(12));png[19]=1;png[23]=1;
        File.WriteAllBytes(Path.Combine(directory,"atlas.png"),png);
        var glyphs=Enumerable.Range(0,256).Select(index=>new
        {sourceIndex=index,codepoint=index==32?(int?)32:null,x=0,y=0,width=1,height=1,
         renderWidth=1,renderHeight=1,advance=1,bearingX=0,bearingY=0}).ToArray();
        var font=JsonSerializer.SerializeToUtf8Bytes(new
        {version=1,height=1,atlas="atlas",supportedCodepoints=new[]{32},glyphs});
        File.WriteAllBytes(Path.Combine(directory,"font.json"),font);
        var manifest=Manifest() with {AssetDeclarations=[new("openfnv.game.resources","ui.resources","Assets/Ui/resources.json")]};
        var files=new DeclaredUiFiles(manifest,root);
        void WriteIndex(object? sampling,bool explicitSampling=true,string imagePath="atlas.png",bool fontSampling=false,object? alphaSampling=null,bool explicitAlpha=false,object? addressing=null,bool explicitAddressing=false,bool fontAddressing=false)
        {
            var image=new Dictionary<string,object?>{{"id","atlas"},{"kind","image"},{"path",imagePath},
                {"sha256",Convert.ToHexStringLower(SHA256.HashData(png))}};
            if(explicitSampling)image["sampling"]=sampling;
            if(explicitAlpha)image["alphaSampling"]=alphaSampling;
            if(explicitAddressing)image["addressing"]=addressing;
            var bitmap=new Dictionary<string,object?>{{"id","font"},{"kind","font.bitmap"},{"path","font.json"},
                {"sha256",Convert.ToHexStringLower(SHA256.HashData(font))}};
            if(fontSampling)bitmap["sampling"]="linear";
            if(fontAddressing)bitmap["addressing"]="wrap";
            File.WriteAllText(Path.Combine(directory,"resources.json"),JsonSerializer.Serialize(new{version=1,resources=new[]{image,bitmap}}));
        }
        var path=Path.Combine(directory,"atlas.png").Replace('\\','/');
        foreach(var explicitPoint in new[]{false,true})
        {
            WriteIndex("point",explicitPoint);var indexed=files.Resources("openfnv.game.resources");
            Require(indexed["atlas"].ImageSource()==path,"Default point UI sampling changed.");
        }
        WriteIndex("linear");var resources=files.Resources("openfnv.game.resources");
        Require(resources["atlas"].ImageSource()=="octaryn-ui-linear:"+path,"Linear image source was not decorated.");
        var prepared=DeclaredUiBitmapFonts.Prepare(files,resources,["font"]);
        Require(prepared.Css.Contains("src: \"octaryn-ui-linear:"+path+"\""),"Bitmap atlas lost indexed filtering.");
        var sprites=DeclaredUiSprites.Resolve(resources,"@spritesheet test {src: asset:atlas; glyph: 0px 0px 1px 1px;}");
        Require(sprites.Contains("src:\"octaryn-ui-linear:"+path+"\""),"Sprite atlas lost indexed filtering.");
        void Denied(Action action)
        {
            try{action();}catch(InvalidOperationException){return;}
            throw new InvalidOperationException("Invalid image sampling or path admitted.");
        }
        foreach(var invalid in new object?[]{"anisotropic","LINEAR",null,4})
        {WriteIndex(invalid);Denied(()=>files.Resources("openfnv.game.resources"));}
        WriteIndex("linear",fontSampling:true);Denied(()=>files.Resources("openfnv.game.resources"));
        WriteIndex("linear",imagePath:"octaryn-ui-linear:atlas.png");Denied(()=>files.Resources("openfnv.game.resources"));
        WriteIndex("linear",imagePath:"../atlas.png");Denied(()=>files.Resources("openfnv.game.resources"));
        WriteIndex("linear",alphaSampling:"straight",explicitAlpha:true);
        resources=files.Resources("openfnv.game.resources");
        Require(resources["atlas"].ImageSource()=="octaryn-ui-straight-linear:"+path,"Straight linear image was not decorated.");
        Require(DeclaredUiBitmapFonts.Prepare(files,resources,["font"]).Css.Contains("octaryn-ui-straight-linear:"+path),"Straight bitmap atlas policy was lost.");
        File.WriteAllText(Path.Combine(directory,"document.rml"),"<rml><head><style>img {background-image:url(asset:atlas);}</style></head><body><img src=\"asset:atlas\"/></body></rml>");
        var imageFiles=new DeclaredUiFiles(manifest with {AssetDeclarations=manifest.AssetDeclarations.Concat(
            new[]{new GameModuleAssetDeclaration("openfnv.game.document","ui.document","Assets/Ui/document.rml")}).ToArray()},root);
        using(var declaration=JsonDocument.Parse("""{"document":"openfnv.game.document","resources":"openfnv.game.resources","modal":false,"styles":[],"fonts":[],"fields":[],"actions":[]}"""))
        using(var screen=JsonDocument.Parse(DeclaredUiDocument.Prepare(imageFiles,declaration.RootElement,"openfnv.game.screen").Json))
        {
            var markup=screen.RootElement.GetProperty("markup").GetString()!;
            Require(markup.Contains("src=\"octaryn-ui-straight-linear:"+path+"\""),"Ordinary image lost sampling metadata.");
            Require(markup.Contains("url(\"octaryn-ui-straight-linear:"+path+"\")"),"CSS image lost sampling metadata.");
        }
        var mesh=JsonSerializer.SerializeToUtf8Bytes(new{version=1,texture="atlas",wrap=true,
            vertices=new[]{new{x=0,y=0,u=0,v=0},new{x=1,y=0,u=1,v=0},new{x=0,y=1,u=0,v=1}},indices=new[]{0,1,2}});
        File.WriteAllBytes(Path.Combine(directory,"mesh.json"),mesh);
        resources.Add("mesh",new(Path.Combine(directory,"mesh.json"),"ui.mesh2d",0,0));
        using(var declaration=JsonDocument.Parse("""{"meshes":[{"element":"mesh","resource":"mesh"}]}"""))
        {
            var meshes=DeclaredUiMeshes.Prepare(files,resources,declaration.RootElement,
                System.Xml.Linq.XDocument.Parse("<rml><body><div id=\"mesh\"/></body></rml>"),[]);
            Require(meshes[0].wrap && meshes[0].texture=="octaryn-ui-straight-linear:"+path,"Mesh lost sampling metadata or source wrapping.");
        }
        foreach(var invalid in new object?[]{"unknown",null,4})
        {WriteIndex("linear",alphaSampling:invalid,explicitAlpha:true);Denied(()=>files.Resources("openfnv.game.resources"));}
        foreach(var linear in new[]{false,true})foreach(var straight in new[]{false,true})
        {
            WriteIndex(linear?"linear":"point",alphaSampling:straight?"straight":"premultiplied",explicitAlpha:true,addressing:"wrap",explicitAddressing:true);
            resources=files.Resources("openfnv.game.resources");
            var prefix=straight?(linear?"straight-linear-wrap":"straight-wrap"):(linear?"linear-wrap":"wrap");
            Require(resources["atlas"].ImageSource()=="octaryn-ui-"+prefix+":"+path,"Image addressing lost filtering or alpha policy.");
        }
        Require(DeclaredUiBitmapFonts.Prepare(files,resources,["font"]).Css.Contains("octaryn-ui-straight-linear-wrap:"+path),"Bitmap atlas lost wrap addressing.");
        foreach(var invalid in new object?[]{"mirror",null,4})
        {WriteIndex("linear",addressing:invalid,explicitAddressing:true);Denied(()=>files.Resources("openfnv.game.resources"));}
        WriteIndex("linear",fontAddressing:true);Denied(()=>files.Resources("openfnv.game.resources"));
        Console.WriteLine("ui_sampling_checks=29 passed=1 defaults=point bitmap_atlas=linear sprites=linear confinement=1 straight_alpha=1 image_css_mesh=1 addressing_wrap=1");
    }
}
