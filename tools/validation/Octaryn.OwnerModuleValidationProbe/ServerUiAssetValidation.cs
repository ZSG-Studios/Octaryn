using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.Host;
using ServerValidation = Octaryn.Server.Validation.ModuleValidation;

internal static partial class OwnerModuleValidationProbe
{
    private static void ValidatePassiveUiAssets()
    {
        string[] resources =
        [
            "Assets/Ui/game.rml",
            "Assets/Ui/game.rcss",
            "Assets/Ui/Fonts/Silkscreen-Regular.ttf",
            "Assets/Ui/Fonts/OFL.txt"
        ];
        foreach (var path in resources)
        {
            ExpectValid($"server accepts passive UI resource {path}",
                ServerValidation.Validate(Module(ValidManifest(assetKind: "ui", assetPath: path))));
        }

        string[] rejectedPaths =
        [
            "Assets/Ui/game.json",
            "Assets/Ui/code.dll",
            "Assets/Ui/Fonts/font.ttf.dll",
            "Assets/Ui/shader.slang",
            "Assets/Ui/../code.rml",
            "Assets/Ui/Fonts/../../code.ttf",
            "Assets/Ui/Fonts\\font.ttf",
            "Assets/Ui/game.rml:stream",
            "Assets/Other/game.rml",
            "Assets/Uiforms/game.rml",
            "Shaders/game.rml"
        ];
        foreach (var path in rejectedPaths)
        {
            ExpectInvalid($"server rejects unsupported UI resource {path}",
                ServerValidation.Validate(Module(ValidManifest(assetKind: "ui", assetPath: path))),
                "server.module.presentation_asset.invalid");
        }

        ExpectInvalid("passive UI does not authorize shaders",
            ServerValidation.Validate(Module(ValidManifest(assetKind: "shader", assetPath: "Assets/Ui/game.rml"))),
            "server.module.presentation_asset.invalid");
        foreach (var phase in new[] { HostWorkPhase.PresentationPrepare, HostWorkPhase.AssetProcessing })
        {
            ExpectInvalid($"passive UI does not authorize {phase}",
                ServerValidation.Validate(Module(ValidManifest(assetKind: "ui", assetPath: resources[0], phase: phase))),
                "server.module.schedule.phase.invalid");
        }
        ExpectInvalid("passive UI does not authorize client APIs",
            ServerValidation.Validate(Module(ValidManifest(assetKind: "ui", assetPath: resources[0],
                requestedHostApis: [HostApiIds.Commands, HostApiIds.Frame, HostApiIds.ClientCommands]))),
            "server.module.host_api.client_only");
        Console.WriteLine("server_ui_asset_validation=passed passive=4 rejected_paths=11 authority_boundaries=4");
    }
}
