using Octaryn.Shared.ApiExposure;
using Octaryn.Shared.Host;

internal static partial class SchedulerProbe
{
    private static IReadOnlyList<ScheduledSystemDeclaration> ProbeDeclarations(string owner, int workerThreadCapacity)
    {
        var declarations = new List<ScheduledSystemDeclaration>
        {
            Declaration($"{owner}.probe.blocking", HostWorkPhase.Validation, HostWorkAccess.None, HostWorkScheduleFlags.RequiresTickBarrier),
            Declaration($"{owner}.probe.fire_and_forget", HostWorkPhase.Validation, HostWorkAccess.None, HostWorkScheduleFlags.None),
            Declaration($"{owner}.probe.failing_blocking", HostWorkPhase.Validation, HostWorkAccess.None, HostWorkScheduleFlags.RequiresTickBarrier),
            Declaration($"{owner}.probe.failing_fire_and_forget", HostWorkPhase.Validation, HostWorkAccess.None, HostWorkScheduleFlags.None),
            Declaration($"{owner}.probe.after_fire_and_forget_failure", HostWorkPhase.Validation, HostWorkAccess.None, HostWorkScheduleFlags.None),
            Declaration(
                $"{owner}.probe.serial.first",
                HostWorkPhase.Gameplay,
                HostWorkAccess.GameplayStateWrite,
                HostWorkScheduleFlags.DeterministicOrder | HostWorkScheduleFlags.RequiresTickBarrier),
            Declaration(
                $"{owner}.probe.serial.second",
                HostWorkPhase.Gameplay,
                HostWorkAccess.GameplayStateWrite,
                HostWorkScheduleFlags.DeterministicOrder | HostWorkScheduleFlags.RequiresTickBarrier),
            DeclarationWithOrdering(
                $"{owner}.probe.order.after.prerequisite",
                HostWorkPhase.Gameplay,
                HostWorkScheduleFlags.CanRunInParallel,
                runsAfter: [],
                runsBefore: []),
            DeclarationWithOrdering(
                $"{owner}.probe.order.after.dependent",
                HostWorkPhase.Gameplay,
                HostWorkScheduleFlags.CanRunInParallel,
                runsAfter: [$"{owner}.probe.order.after.prerequisite"],
                runsBefore: []),
            DeclarationWithOrdering(
                $"{owner}.probe.order.epoch.prerequisite",
                HostWorkPhase.Gameplay,
                HostWorkScheduleFlags.CanRunInParallel,
                runsAfter: [],
                runsBefore: []),
            DeclarationWithOrdering(
                $"{owner}.probe.order.epoch.dependent",
                HostWorkPhase.Gameplay,
                HostWorkScheduleFlags.CanRunInParallel,
                runsAfter: [$"{owner}.probe.order.epoch.prerequisite"],
                runsBefore: []),
            DeclarationWithOrdering(
                $"{owner}.probe.order.failed.prerequisite",
                HostWorkPhase.Gameplay,
                HostWorkScheduleFlags.CanRunInParallel,
                runsAfter: [],
                runsBefore: []),
            DeclarationWithOrdering(
                $"{owner}.probe.order.failed.dependent",
                HostWorkPhase.Gameplay,
                HostWorkScheduleFlags.CanRunInParallel,
                runsAfter: [$"{owner}.probe.order.failed.prerequisite"],
                runsBefore: []),
            DeclarationWithOrdering(
                $"{owner}.probe.order.before.before",
                HostWorkPhase.Gameplay,
                HostWorkScheduleFlags.CanRunInParallel,
                runsAfter: [],
                runsBefore: [$"{owner}.probe.order.before.after"]),
            DeclarationWithOrdering(
                $"{owner}.probe.order.before.after",
                HostWorkPhase.Gameplay,
                HostWorkScheduleFlags.CanRunInParallel,
                runsAfter: [],
                runsBefore: []),
            DeclarationWithWrites(
                $"{owner}.probe.barrier.parallel",
                HostWorkPhase.Gameplay,
                HostWorkScheduleFlags.CanRunInParallel,
                "octaryn.probe.barrier.parallel"),
            Declaration(
                $"{owner}.probe.barrier.commit",
                HostWorkPhase.Gameplay,
                HostWorkAccess.None,
                HostWorkScheduleFlags.RequiresTickBarrier),
            DeclarationWithWrites(
                $"{owner}.probe.resource_conflict.first",
                HostWorkPhase.Gameplay,
                HostWorkScheduleFlags.CanRunInParallel,
                "octaryn.probe.exact.shared"),
            DeclarationWithWrites(
                $"{owner}.probe.resource_conflict.second",
                HostWorkPhase.Gameplay,
                HostWorkScheduleFlags.CanRunInParallel,
                "octaryn.probe.exact.shared"),
            DeclarationWithWrites(
                $"{owner}.probe.resource_independent.0",
                HostWorkPhase.Gameplay,
                HostWorkScheduleFlags.CanRunInParallel,
                "octaryn.probe.exact.left"),
            DeclarationWithWrites(
                $"{owner}.probe.resource_independent.1",
                HostWorkPhase.Gameplay,
                HostWorkScheduleFlags.CanRunInParallel,
                "octaryn.probe.exact.right"),
            Declaration($"{owner}.probe.nested_blocking_inner", HostWorkPhase.Validation, HostWorkAccess.None, HostWorkScheduleFlags.RequiresTickBarrier),
            Declaration($"{owner}.probe.nested_blocking_outer", HostWorkPhase.Validation, HostWorkAccess.None, HostWorkScheduleFlags.CanRunInParallel)
        };

        for (var index = 0; index < workerThreadCapacity; index++)
        {
            declarations.Add(Declaration(
                $"{owner}.probe.parallel_capacity.{index}",
                HostWorkPhase.Validation,
                HostWorkAccess.None,
                HostWorkScheduleFlags.CanRunInParallel));
        }

        return declarations;
    }

    private static IReadOnlyList<ScheduledSystemDeclaration> UnresolvedProbeDeclarations(string owner)
    {
        return
        [
            DeclarationWithOrdering(
                $"{owner}.probe.shutdown_unresolved",
                HostWorkPhase.Gameplay,
                HostWorkScheduleFlags.CanRunInParallel,
                runsAfter: [$"{owner}.probe.shutdown_missing"],
                runsBefore: [])
        ];
    }

    private static ScheduledSystemDeclaration Declaration(
        string systemId,
        HostWorkPhase phase,
        HostWorkAccess access,
        HostWorkScheduleFlags flags)
    {
        return new ScheduledSystemDeclaration(
            systemId,
            phase,
            HostScheduleIds.FrameOrTickOwner,
            ReadsForAccess(access),
            WritesForAccess(access),
            [],
            [],
            flags,
            HostScheduleIds.FrameOrTickEndBarrier);
    }

    private static ScheduledSystemDeclaration DeclarationWithWrites(
        string systemId,
        HostWorkPhase phase,
        HostWorkScheduleFlags flags,
        params string[] resourceIds)
    {
        return new ScheduledSystemDeclaration(
            systemId,
            phase,
            HostScheduleIds.FrameOrTickOwner,
            [],
            resourceIds
                .Select(resourceId => new ScheduledResourceAccess(resourceId, ScheduledAccessMode.Write))
                .ToArray(),
            [],
            [],
            flags,
            HostScheduleIds.FrameOrTickEndBarrier);
    }

    private static ScheduledSystemDeclaration DeclarationWithOrdering(
        string systemId,
        HostWorkPhase phase,
        HostWorkScheduleFlags flags,
        IReadOnlyList<string> runsAfter,
        IReadOnlyList<string> runsBefore)
    {
        return new ScheduledSystemDeclaration(
            systemId,
            phase,
            HostScheduleIds.FrameOrTickOwner,
            [],
            [new ScheduledResourceAccess($"{systemId}.state", ScheduledAccessMode.Write)],
            runsAfter,
            runsBefore,
            flags,
            HostScheduleIds.FrameOrTickEndBarrier);
    }

    private static IReadOnlyList<ScheduledResourceAccess> ReadsForAccess(HostWorkAccess access)
    {
        var reads = new List<ScheduledResourceAccess>();
        if ((access & (HostWorkAccess.InputSnapshot | HostWorkAccess.FrameTimingSnapshot)) != 0)
        {
            reads.Add(new ScheduledResourceAccess(HostApiIds.Frame, ScheduledAccessMode.Read));
        }

        if ((access & HostWorkAccess.GameplayStateRead) != 0)
        {
            reads.Add(new ScheduledResourceAccess("octaryn.probe.state", ScheduledAccessMode.Read));
        }

        if ((access & HostWorkAccess.ContentRegistryRead) != 0)
        {
            reads.Add(new ScheduledResourceAccess("octaryn.probe.content.registry", ScheduledAccessMode.Read));
        }

        return reads;
    }

    private static IReadOnlyList<ScheduledResourceAccess> WritesForAccess(HostWorkAccess access)
    {
        var writes = new List<ScheduledResourceAccess>();
        if ((access & HostWorkAccess.GameplayStateWrite) != 0)
        {
            writes.Add(new ScheduledResourceAccess("octaryn.probe.state", ScheduledAccessMode.Write));
        }

        if ((access & HostWorkAccess.CommandSinkWrite) != 0)
        {
            writes.Add(new ScheduledResourceAccess(HostApiIds.Commands, ScheduledAccessMode.Write));
        }

        return writes;
    }
}
