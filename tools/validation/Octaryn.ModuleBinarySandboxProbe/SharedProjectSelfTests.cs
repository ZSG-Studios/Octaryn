using System.Text.Json;

internal static class SharedProjectSelfTests
{
    public static void Run(List<string> errors)
    {
        var root = Path.Combine(Path.GetTempPath(), "octaryn-shared-project-" + Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(Path.Combine(root,"engine","octaryn-shared"));
            Directory.CreateDirectory(Path.Combine(root,"external-game"));
            var trusted = Path.Combine(root,"engine","octaryn-shared","Octaryn.Shared.csproj");
            File.WriteAllText(trusted,"<Project />");
            var policy = Path.Combine(root,"closed-policy.json");
            File.WriteAllText(policy,"{\"runtimeDirect\":{},\"runtimeTransitive\":{}}");
            var assets = Path.Combine(root,"project.assets.json");
            var original = JsonSerializer.Serialize(new
            {
                targets = new Dictionary<string, object> { ["net10.0"] = new Dictionary<string,object>
                {
                    ["Octaryn.Shared/1.0.0"] = new { compile = new Dictionary<string,object>{["placeholder/Octaryn.Shared.dll"] = new {}}, runtime = new Dictionary<string,object>{["placeholder/Octaryn.Shared.dll"] = new {}} },
                    ["Unrequested/1.0.0"] = new { compile = new Dictionary<string,object>{["placeholder/Unrequested.dll"] = new {}}, runtime = new Dictionary<string,object>{["placeholder/Unrequested.dll"] = new {}} }
                } },
                libraries = new Dictionary<string,object> { ["Octaryn.Shared/1.0.0"] = new { type="project",path="../engine/octaryn-shared/Octaryn.Shared.csproj" } },
                project = new { restore = new { projectPath=Path.Combine(root,"external-game","Game.csproj") } }
            });
            File.WriteAllText(assets,original);
            var allowed = AssemblyReferencePolicy.LoadAllowedAssemblyReferences(assets,policy,errors,trusted);
            if(!allowed.Contains("Octaryn.Shared") || allowed.Contains("Unrequested")) errors.Add("Trusted external Shared with zero runtime packages failed.");
            File.WriteAllText(assets,original.Replace("../engine/octaryn-shared/Octaryn.Shared.csproj","../spoof/Octaryn.Shared.csproj"));
            allowed = AssemblyReferencePolicy.LoadAllowedAssemblyReferences(assets,policy,errors,trusted);
            if(allowed.Contains("Octaryn.Shared"))errors.Add("External source spoof granted Shared trust.");
            File.WriteAllText(assets,original);
            allowed = AssemblyReferencePolicy.LoadAllowedAssemblyReferences(assets,policy,errors,Path.Combine(root,"missing","Octaryn.Shared.csproj"));
            if(allowed.Contains("Octaryn.Shared"))errors.Add("Missing expected Shared project granted trust.");
        }
        finally { if(Directory.Exists(root))Directory.Delete(root,true); }
    }
}
