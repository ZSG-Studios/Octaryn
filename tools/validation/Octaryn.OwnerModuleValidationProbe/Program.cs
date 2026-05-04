using Octaryn.Client;
using Octaryn.Server;
using Octaryn.Server.Modules;
using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.FrameworkAllowlist;
using Octaryn.Shared.GameModules;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

return OwnerModuleValidationProbe.Run();

internal static class OwnerModuleValidationProbe
{
    public static int Run()
    {
        ExpectValid("client accepts valid manifest", ClientModuleValidation.Validate(Module(ValidManifest())));
        ExpectValid("server accepts valid manifest", ServerModuleValidation.Validate(Module(ValidManifest())));
        ValidateHostModuleContextGrants();

        ExpectInvalid(
            "client rejects missing frame API",
            ClientModuleValidation.Validate(Module(ValidManifest(requestedHostApis: [HostApiIds.Commands]))),
            "client.module.host_api.required");
        ExpectInvalid(
            "shared validator rejects missing frame API for frame read",
            ClientModuleValidation.Validate(Module(ValidManifest(requestedHostApis: [HostApiIds.Commands]))),
            "module.schedule.frame.read.required");
        ExpectInvalid(
            "client rejects shader asset outside Shaders",
            ClientModuleValidation.Validate(Module(ValidManifest(assetKind: "shader", assetPath: "Assets/Shaders/octaryn.test.shader.glsl"))),
            "client.module.shader_asset.path.invalid");
        ExpectInvalid(
            "client rejects server authority phase",
            ClientModuleValidation.Validate(Module(ValidManifest(phase: HostWorkPhase.PersistencePrepare))),
            "client.module.schedule.phase.invalid");
        ExpectInvalid(
            "client rejects server snapshot API",
            ClientModuleValidation.Validate(Module(ValidManifest(requestedHostApis:
            [
                HostApiIds.Commands,
                HostApiIds.Frame,
                HostApiIds.ServerSnapshots
            ]))),
            "client.module.host_api.server_only");
        ExpectInvalid(
            "client rejects replication API",
            ClientModuleValidation.Validate(Module(ValidManifest(requestedHostApis:
            [
                HostApiIds.Commands,
                HostApiIds.Frame,
                HostApiIds.Replication
            ]))),
            "client.module.host_api.replication_not_supported");
        ExpectInvalid(
            "client rejects multiplayer",
            ClientModuleValidation.Validate(Module(ValidManifest(supportsMultiplayer: true))),
            "client.module.multiplayer.not_supported");
        ExpectInvalid(
            "server rejects missing commands API",
            ServerModuleValidation.Validate(Module(ValidManifest(requestedHostApis: [HostApiIds.Frame]))),
            "server.module.host_api.required");
        ExpectInvalid(
            "shared validator rejects missing commands API for command write",
            ServerModuleValidation.Validate(Module(ValidManifest(requestedHostApis: [HostApiIds.Frame]))),
            "module.schedule.commands.host_api.required");
        ExpectInvalid(
            "server rejects client commands API",
            ServerModuleValidation.Validate(Module(ValidManifest(requestedHostApis:
            [
                HostApiIds.Commands,
                HostApiIds.Frame,
                HostApiIds.ClientCommands
            ]))),
            "server.module.host_api.client_only");
        ExpectInvalid(
            "server rejects replication API",
            ServerModuleValidation.Validate(Module(ValidManifest(requestedHostApis:
            [
                HostApiIds.Commands,
                HostApiIds.Frame,
                HostApiIds.Replication
            ]))),
            "server.module.host_api.replication_not_supported");
        ExpectInvalid(
            "server rejects missing gameplay rules capability",
            ServerModuleValidation.Validate(Module(ValidManifest(requiredCapabilities:
            [
                ModuleCapabilityIds.ContentBlocks,
                ModuleCapabilityIds.ContentItems,
                ModuleCapabilityIds.GameplayInteractions,
                ModuleCapabilityIds.WorldBlockEdits
            ]))),
            "server.module.capability.required");
        ExpectInvalid(
            "shared validator rejects missing world block edit capability",
            ClientModuleValidation.Validate(Module(ValidManifest(requiredCapabilities:
            [
                ModuleCapabilityIds.ContentBlocks,
                ModuleCapabilityIds.ContentItems,
                ModuleCapabilityIds.GameplayInteractions,
                ModuleCapabilityIds.GameplayRules
            ]))),
            "module.capability.world_block_edits.required");
        ExpectInvalid(
            "server rejects presentation assets",
            ServerModuleValidation.Validate(Module(ValidManifest(assetKind: "shader", assetPath: "Shaders/octaryn.test.shader.glsl"))),
            "server.module.presentation_asset.invalid");
        ExpectInvalid(
            "server rejects ui presentation assets",
            ServerModuleValidation.Validate(Module(ValidManifest(assetKind: "ui", assetPath: "Assets/UI/octaryn.test.ui.json"))),
            "server.module.presentation_asset.invalid");
        ExpectInvalid(
            "server rejects presentation phase",
            ServerModuleValidation.Validate(Module(ValidManifest(phase: HostWorkPhase.PresentationPrepare))),
            "server.module.schedule.phase.invalid");
        ExpectInvalid(
            "server rejects non-owned save compatibility ID",
            ServerModuleValidation.Validate(Module(ValidManifest(saveCompatibilityId: "other.save.v0"))),
            "server.module.save_compatibility_id.invalid");
        ExpectInvalid(
            "server rejects multiplayer",
            ServerModuleValidation.Validate(Module(ValidManifest(supportsMultiplayer: true))),
            "server.module.multiplayer.not_supported");
        ExpectInvalid(
            "commands require scheduled command write",
            ClientModuleValidation.Validate(Module(ValidManifest(includeCommandWrite: false))),
            "module.schedule.commands.write.required");
        ValidateActivatorsRejectInvalidManifestBeforeSchedulerCreation();
        ValidateActivatorsDisposeSchedulerWhenCreateInstanceFails();
        ValidateActivatorsDisposeSchedulerWhenInstanceDisposeFails();
        return 0;
    }

    private static GameModuleManifest ValidManifest(
        IReadOnlyList<string>? requiredCapabilities = null,
        IReadOnlyList<string>? requestedHostApis = null,
        string assetKind = "texture",
        string assetPath = "Assets/Textures/octaryn.test.texture.txt",
        string saveCompatibilityId = "octaryn.test.save.v0",
        bool supportsMultiplayer = false,
        HostWorkPhase phase = HostWorkPhase.Gameplay,
        bool includeCommandWrite = true)
    {
        var hostApis = requestedHostApis ??
        [
            HostApiIds.Commands,
            HostApiIds.Frame
        ];

        return new GameModuleManifest(
            ModuleId: "octaryn.test",
            DisplayName: "Octaryn Test Module",
            Version: "0.1.0",
            OctarynApiVersion: "0.1.0",
            RequiredCapabilities: requiredCapabilities ??
            [
                ModuleCapabilityIds.ContentBlocks,
                ModuleCapabilityIds.ContentItems,
                ModuleCapabilityIds.GameplayInteractions,
                ModuleCapabilityIds.GameplayRules,
                ModuleCapabilityIds.WorldBlockEdits
            ],
            RequestedHostApis: hostApis,
            RequestedRuntimePackages: [],
            RequestedBuildPackages: [],
            RequestedFrameworkApiGroups:
            [
                FrameworkApiGroupIds.BclPrimitives,
                FrameworkApiGroupIds.BclCollections
            ],
            ModuleDependencies: [],
            ContentDeclarations:
            [
                new GameModuleContentDeclaration(
                    "octaryn.test.rule",
                    "rule",
                    "Data/Rules/octaryn.test.rule.json")
            ],
            AssetDeclarations:
            [
                new GameModuleAssetDeclaration(
                    "octaryn.test.asset",
                    assetKind,
                    assetPath)
            ],
            Schedule: new GameModuleScheduleDeclaration(
            [
                new ScheduledSystemDeclaration(
                    SystemId: "octaryn.test.tick",
                    Phase: phase,
                    FrameOrTickOwner: HostScheduleIds.FrameOrTickOwner,
                    Reads:
                    [
                        new ScheduledResourceAccess("host.frame", ScheduledAccessMode.Read)
                    ],
                    Writes: ScheduleWrites(includeCommandWrite),
                    RunsAfter: [],
                    RunsBefore: [],
                    Flags: HostWorkScheduleFlags.DeterministicOrder | HostWorkScheduleFlags.RequiresTickBarrier,
                    CommitBarrier: HostScheduleIds.FrameOrTickEndBarrier)
            ]),
            Compatibility: new GameModuleCompatibility(
                MinimumHostApiVersion: "0.1.0",
                MaximumHostApiVersion: "0.1.0",
                SaveCompatibilityId: saveCompatibilityId,
                SupportsMultiplayer: supportsMultiplayer));
    }

    private static IReadOnlyList<ScheduledResourceAccess> ScheduleWrites(bool includeCommandWrite)
    {
        var writes = new List<ScheduledResourceAccess>
        {
            new("octaryn.test.state", ScheduledAccessMode.Write)
        };
        if (includeCommandWrite)
        {
            writes.Add(new ScheduledResourceAccess(HostApiIds.Commands, ScheduledAccessMode.Write));
        }

        return writes;
    }

    private static TestRegistration Module(GameModuleManifest manifest)
    {
        return new TestRegistration(manifest);
    }

    private static void ExpectValid(string name, ModuleValidationReport report)
    {
        if (!report.IsValid)
        {
            throw new InvalidOperationException($"{name}: expected valid report, got {Issues(report)}");
        }
    }

    private static void ExpectInvalid(string name, ModuleValidationReport report, string expectedCode)
    {
        if (report.IsValid)
        {
            throw new InvalidOperationException($"{name}: expected invalid report.");
        }

        if (!report.Issues.Any(issue => issue.Code == expectedCode))
        {
            throw new InvalidOperationException($"{name}: expected {expectedCode}, got {Issues(report)}");
        }
    }

    private static string Issues(ModuleValidationReport report)
    {
        return string.Join(", ", report.Issues.Select(issue => issue.Code));
    }

    private static void ValidateHostModuleContextGrants()
    {
        var commandSink = new TestCommandSink();

        var fullContext = HostModuleContext.Create(ValidManifest(), commandSink);
        if (fullContext.Commands.TryRequest(ModuleCommandRequest.BreakBlock(new BlockPosition(1, 2, 3), 2)))
        {
            throw new InvalidOperationException("host context should deny block edit request outside scheduled command write scope.");
        }

        using (HostCommandWriteScope.Enter(HostWorkAccess.CommandSinkWrite))
        {
            if (!fullContext.Commands.TryRequest(ModuleCommandRequest.BreakBlock(new BlockPosition(1, 2, 3), 2)))
            {
                throw new InvalidOperationException("host context should grant block edit request inside scheduled command write scope.");
            }
            commandSink.ExpectLastBlockEdit(new BlockPosition(1, 2, 3), BlockId.Air, 2);

            if (fullContext.Commands.TryRequest(new ModuleCommandRequest(
                ModuleCommandRequestKind.None,
                3,
                new BlockEdit(new BlockPosition(4, 5, 6), new BlockId(7)))))
            {
                throw new InvalidOperationException("host context should reject empty command request kinds.");
            }
        }

        using (HostCommandWriteScope.Enter(HostWorkAccess.GameplayStateWrite))
        {
            if (fullContext.Commands.TryRequest(ModuleCommandRequest.BreakBlock(new BlockPosition(1, 2, 3), 2)))
            {
                throw new InvalidOperationException("host context should deny block edit request inside non-command scheduled scope.");
            }
        }

        var deniedCommands = HostModuleContext.Create(
            ValidManifest(requestedHostApis: [HostApiIds.Frame]),
            commandSink);
        if (deniedCommands.Commands.TryRequest(ModuleCommandRequest.BreakBlock(new BlockPosition(1, 2, 3), 2)))
        {
            throw new InvalidOperationException("host context should deny block edits when commands are not requested.");
        }

        var deniedUndeclaredCommands = HostModuleContext.Create(
            ValidManifest(includeCommandWrite: false),
            commandSink);
        if (deniedUndeclaredCommands.Commands.TryRequest(ModuleCommandRequest.BreakBlock(new BlockPosition(1, 2, 3), 2)))
        {
            throw new InvalidOperationException("host context should deny block edits without a scheduled command write.");
        }

        var deniedMissingCapability = HostModuleContext.Create(
            ValidManifest(requiredCapabilities:
            [
                ModuleCapabilityIds.ContentBlocks,
                ModuleCapabilityIds.ContentItems,
                ModuleCapabilityIds.GameplayInteractions,
                ModuleCapabilityIds.GameplayRules
            ]),
            commandSink);
        using (HostCommandWriteScope.Enter(HostWorkAccess.CommandSinkWrite))
        {
            if (deniedMissingCapability.Commands.TryRequest(ModuleCommandRequest.BreakBlock(new BlockPosition(1, 2, 3), 2)))
            {
                throw new InvalidOperationException("host context should deny block edits without the world block edits capability.");
            }
        }

    }

    private static void ValidateActivatorsRejectInvalidManifestBeforeSchedulerCreation()
    {
        var duplicateSystem = ValidManifest() with
        {
            Schedule = new GameModuleScheduleDeclaration(
            [
                ValidManifest().Schedule.Systems[0],
                ValidManifest().Schedule.Systems[0]
            ])
        };

        using (var client = new ClientGameModuleActivator(Module(duplicateSystem)))
        {
            if (client.Activate(new TestCommandSink()) != -2 || client.IsActive)
            {
                throw new InvalidOperationException("client activator should reject invalid manifest before scheduler creation.");
            }
        }

        using (var server = new ModuleActivator(Module(duplicateSystem)))
        {
            if (server.Activate(new TestCommandSink()) != -2 || server.IsActive)
            {
                throw new InvalidOperationException("server activator should reject invalid manifest before scheduler creation.");
            }
        }
    }

    private static void ValidateActivatorsDisposeSchedulerWhenCreateInstanceFails()
    {
        var expected = new InvalidOperationException("expected create failure");

        using (var client = new ClientGameModuleActivator(new ThrowingCreateRegistration(ValidManifest(), expected)))
        {
            ExpectThrows("client create failure", expected, () => client.Activate(new TestCommandSink()));
            ExpectInactiveWithDisposedScheduler("client create failure", client);
        }

        using (var server = new ModuleActivator(new ThrowingCreateRegistration(ValidManifest(), expected)))
        {
            ExpectThrows("server create failure", expected, () => server.Activate(new TestCommandSink()));
            ExpectInactiveWithDisposedScheduler("server create failure", server);
        }
    }

    private static void ValidateActivatorsDisposeSchedulerWhenInstanceDisposeFails()
    {
        var expected = new InvalidOperationException("expected dispose failure");

        var client = new ClientGameModuleActivator(new ThrowingDisposeRegistration(ValidManifest(), expected));
        if (client.Activate(new TestCommandSink()) != 0 || !client.IsActive)
        {
            throw new InvalidOperationException("client throwing-dispose activator did not activate.");
        }

        ExpectThrows("client dispose failure", expected, client.Dispose);
        ExpectInactiveWithDisposedScheduler("client dispose failure", client);

        var server = new ModuleActivator(new ThrowingDisposeRegistration(ValidManifest(), expected));
        if (server.Activate(new TestCommandSink()) != 0 || !server.IsActive)
        {
            throw new InvalidOperationException("server throwing-dispose activator did not activate.");
        }

        ExpectThrows("server dispose failure", expected, server.Dispose);
        ExpectInactiveWithDisposedScheduler("server dispose failure", server);
    }

    private static void ExpectThrows(string name, Exception expected, Action action)
    {
        try
        {
            action();
            throw new InvalidOperationException($"{name}: expected exception.");
        }
        catch (Exception exception) when (ReferenceEquals(exception, expected))
        {
        }
    }

    private static void ExpectInactiveWithDisposedScheduler(string name, object activator)
    {
        if (IsActive(activator) || FieldValue(activator, "_instance") is not null || FieldValue(activator, "_scheduler") is not null)
        {
            throw new InvalidOperationException($"{name}: activator retained module instance or scheduler after failure.");
        }
    }

    private static bool IsActive(object activator)
    {
        return activator switch
        {
            ClientGameModuleActivator client => client.IsActive,
            ModuleActivator server => server.IsActive,
            _ => throw new InvalidOperationException($"Unknown activator type {activator.GetType().FullName}.")
        };
    }

    private static object? FieldValue(object instance, string fieldName)
    {
        return instance.GetType()
            .GetField(fieldName, System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic)
            ?.GetValue(instance);
    }

}
