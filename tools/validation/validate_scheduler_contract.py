#!/usr/bin/env python3
import argparse
import pathlib
import sys


REQUIRED_FILES = (
    "octaryn-shared/Source/Host/HostSchedulingContract.cs",
    "octaryn-shared/Source/Host/IHostScheduler.cs",
    "octaryn-shared/Source/Host/HostSchedulerDiagnostics.cs",
    "octaryn-client/Source/ClientHost/ClientHostScheduler.cs",
    "octaryn-client/Source/ClientHost/ClientHostScheduler.Coordinator.cs",
    "octaryn-client/Source/ClientHost/ClientHostScheduler.Worker.cs",
    "octaryn-client/Source/ClientHost/ScheduledHostWork.cs",
    "octaryn-client/Source/ClientHost/ResourceAccessScope.cs",
    "octaryn-server/Source/Tick/ServerHostScheduler.cs",
    "octaryn-server/Source/Tick/ServerHostScheduler.Coordinator.cs",
    "octaryn-server/Source/Tick/ServerHostScheduler.Worker.cs",
    "octaryn-server/Source/Tick/ScheduledHostWork.cs",
    "octaryn-server/Source/Tick/ResourceAccessScope.cs",
    "octaryn-basegame/Source/Managed/GameContext.cs",
    "octaryn-basegame/Source/Module/BasegameScheduleDeclarations.cs",
    "octaryn-basegame/Source/Module/BasegameModuleRegistration.cs",
    "tools/validation/Octaryn.SchedulerProbe/Program.cs",
)


def require_contains(errors, path, text, snippets):
    for snippet in snippets:
        if snippet not in text:
            errors.append(f"{path}: missing scheduler contract snippet: {snippet}")


def validate_scheduler(path, owner):
    errors = []
    text = read_scheduler_source(path)

    require_contains(
        errors,
        path,
        text,
        [
            "IHostScheduler, IDisposable",
            "BlockingCollection<ScheduledHostWork>",
            "Thread _coordinatorThread",
            "Thread[] _workerThreads",
            "HostSchedulingContract.MinimumWorkerThreads",
            "Environment.ProcessorCount",
            "HostSchedulingContract.IsValidWorkerThreadCapacity",
            "internal HostSchedulerDiagnostics Diagnostics => new(",
            "_coordinatorThread.IsAlive",
            "_workerThreads.Count(thread => thread.IsAlive)",
            "_fireAndForgetFailureCount",
            "_lastFireAndForgetFailureWorkId",
            "_lastFireAndForgetFailureType",
            "[ThreadStatic]",
            "isSchedulerThread",
            "RecordFireAndForgetFailure",
            "MustRunSerially",
            "BuildWorkPrerequisites",
            "BuildOrderedWorkIds",
            "DispatchReadyWork",
            "DrainInFlightWork",
            "ResetCompletedWorkIdsIfIdle",
            "HasPrerequisites",
            "IsOrderedWork",
            "HasFailedPrerequisite",
            "RecordWorkCompletion",
            "FailSkippedWork",
            "CompleteUnresolvedWork",
            "HasFailed",
            "HostScheduledWork.AccessFromDeclaration(declaration)",
            "HostWorkScheduleFlags.CanRunInParallel",
            "HostWorkScheduleFlags.DeterministicOrder | HostWorkScheduleFlags.RequiresTickBarrier",
            "ResourceAccessScope.Enter",
            "public bool TryRun(HostScheduledWork work, HostFrameContext frame)",
            "ScheduledHostWork.Blocking",
            "HostThreadRole.WorkerPool",
            "scheduled.Frame",
            "scheduled.Work.Execute(context)",
        ])

    forbidden = [
        "WorkerThreadCapacity => 0",
        "IsWorkerPoolAvailable => false",
    ]
    for snippet in forbidden:
        if snippet in text:
            errors.append(f"{path}: scheduler still contains stub behavior: {snippet.strip()}")

    expected_name = f"octaryn.{owner}.coordinator"
    if expected_name not in text:
        errors.append(f"{path}: coordinator thread name must include {expected_name}")

    expected_worker_name = f"octaryn.{owner}.worker."
    if expected_worker_name not in text:
        errors.append(f"{path}: worker thread names must include {expected_worker_name}")

    return errors


def read_scheduler_source(path):
    source_files = sorted(path.parent.glob(f"{path.stem}*.cs"))
    return "\n".join(source.read_text(encoding="utf-8") for source in source_files)


def validate_scheduled_host_work(path):
    errors = []
    text = path.read_text(encoding="utf-8")

    require_contains(
        errors,
        path,
        text,
        [
            "internal sealed class ScheduledHostWork",
            "ManualResetEventSlim",
            "HostScheduledWork Work",
            "HostFrameContext Frame",
            "bool IsBlocking",
            "bool HasFailed",
            "FireAndForget",
            "Blocking",
            "MarkCoordinatorWaiter",
            "WaitForCompletion",
            "ThrowIfFailed",
        ])

    return errors


def validate_resource_access_scope(path):
    errors = []
    text = path.read_text(encoding="utf-8")

    require_contains(
        errors,
        path,
        text,
        [
            "internal sealed class ResourceAccessScope",
            "ConcurrentDictionary<string, ReaderWriterLockSlim>",
            "ScheduledSystemDeclaration declaration",
            "ScheduledResourceAccess",
            "ScheduledAccessMode.Write",
            "EnterWriteLock",
            "EnterReadLock",
            "ExitWriteLock",
            "ExitReadLock",
        ])

    return errors


def validate_basegame_tick_shape(errors, path, text):
    tick_anchor = "public void Tick(in ModuleFrameContext frame)"
    tick_start = text.find(tick_anchor)
    if tick_start < 0:
        errors.append(f"{path}: missing GameContext.Tick")
        return

    tick_body = text[tick_start:]
    if "_host.Scheduler" in tick_body or "HostScheduledWork.FromDeclaration" in tick_body:
        errors.append(f"{path}: GameContext.Tick must not own host scheduler dispatch")

    for mutation in ("EnsureBootstrapEntity()", "_world.Set("):
        if mutation not in tick_body:
            errors.append(f"{path}: GameContext.Tick must perform gameplay mutation: {mutation}")


def validate_activator_tick_shape(errors, path, text):
    tick_anchor = "public void Tick(in HostFrameSnapshot snapshot)"
    tick_start = text.find(tick_anchor)
    if tick_start < 0:
        errors.append(f"{path}: missing activator Tick")
        return

    tick_body = text[tick_start:]
    for snippet in (
        "HostFrameContext.FromSnapshot",
        "HostScheduledWork.FromDeclaration",
        "_scheduler.TryRun",
        "_instance.Tick(in moduleFrame)",
    ):
        if snippet not in tick_body:
            errors.append(f"{path}: activator Tick must own host scheduler dispatch: {snippet}")

    if (
        "new ModuleFrameContext(frame.DeltaSeconds, frame.FrameIndex)" not in tick_body and
        "new ModuleFrameContext(frame.DeltaSeconds, frame.FrameIndex, worldTime)" not in tick_body
    ):
        errors.append(f"{path}: activator Tick must own host scheduler dispatch: new ModuleFrameContext")


def read_scheduler_probe_source(path):
    source_files = sorted(path.glob("*.cs"))
    return "\n".join(source.read_text(encoding="utf-8") for source in source_files)


def validate(repo_root):
    errors = []
    for relative_path in REQUIRED_FILES:
        path = repo_root / relative_path
        if not path.exists():
            errors.append(f"{path}: missing scheduler contract file")

    contract = repo_root / "octaryn-shared/Source/Host/HostSchedulingContract.cs"
    if contract.exists():
        require_contains(
            errors,
            contract,
            contract.read_text(encoding="utf-8"),
            [
                "public const int MinimumWorkerThreads = 2;",
                "MainThreadRole",
                "CoordinatorThreadRole",
                "WorkerPoolRole",
                "workerThreadCapacity >= MinimumWorkerThreads",
            ])

    scheduler_api = repo_root / "octaryn-shared/Source/Host/IHostScheduler.cs"
    if scheduler_api.exists():
        require_contains(
            errors,
            scheduler_api,
            scheduler_api.read_text(encoding="utf-8"),
            [
                "bool TrySchedule(HostScheduledWork work);",
                "bool TryRun(HostScheduledWork work, HostFrameContext frame);",
            ])

    diagnostics = repo_root / "octaryn-shared/Source/Host/HostSchedulerDiagnostics.cs"
    if diagnostics.exists():
        require_contains(
            errors,
            diagnostics,
            diagnostics.read_text(encoding="utf-8"),
            [
                "FireAndForgetFailureCount",
                "LastFireAndForgetFailureWorkId",
                "LastFireAndForgetFailureType",
            ])

    client = repo_root / "octaryn-client/Source/ClientHost/ClientHostScheduler.cs"
    if client.exists():
        errors.extend(validate_scheduler(client, "client"))
    client_work = repo_root / "octaryn-client/Source/ClientHost/ScheduledHostWork.cs"
    if client_work.exists():
        errors.extend(validate_scheduled_host_work(client_work))
    client_scope = repo_root / "octaryn-client/Source/ClientHost/ResourceAccessScope.cs"
    if client_scope.exists():
        errors.extend(validate_resource_access_scope(client_scope))

    server = repo_root / "octaryn-server/Source/Tick/ServerHostScheduler.cs"
    if server.exists():
        errors.extend(validate_scheduler(server, "server"))
    server_work = repo_root / "octaryn-server/Source/Tick/ScheduledHostWork.cs"
    if server_work.exists():
        errors.extend(validate_scheduled_host_work(server_work))
    server_scope = repo_root / "octaryn-server/Source/Tick/ResourceAccessScope.cs"
    if server_scope.exists():
        errors.extend(validate_resource_access_scope(server_scope))

    basegame = repo_root / "octaryn-basegame/Source/Managed/GameContext.cs"
    if basegame.exists():
        basegame_text = basegame.read_text(encoding="utf-8")
        require_contains(
            errors,
            basegame,
            basegame_text,
            [
                "BasegameFrameState",
                "EnsureBootstrapEntity",
            ])
        validate_basegame_tick_shape(errors, basegame, basegame_text)

    client_activator = repo_root / "octaryn-client/Source/ClientHost/BasegameModuleActivator.cs"
    if client_activator.exists():
        validate_activator_tick_shape(errors, client_activator, client_activator.read_text(encoding="utf-8"))

    server_activator = repo_root / "octaryn-server/Source/Modules/ModuleActivator.cs"
    if server_activator.exists():
        validate_activator_tick_shape(errors, server_activator, server_activator.read_text(encoding="utf-8"))

    schedule_declarations = repo_root / "octaryn-basegame/Source/Module/BasegameScheduleDeclarations.cs"
    if schedule_declarations.exists():
        require_contains(
            errors,
            schedule_declarations,
            schedule_declarations.read_text(encoding="utf-8"),
            [
                "SystemId: \"octaryn.basegame.frame_tick\"",
                "FrameOrTickOwner: HostScheduleIds.FrameOrTickOwner",
                "CommitBarrier: HostScheduleIds.FrameOrTickEndBarrier",
            ])

    manifest = repo_root / "octaryn-basegame/Source/Module/BasegameModuleRegistration.cs"
    if manifest.exists():
        require_contains(
            errors,
            manifest,
            manifest.read_text(encoding="utf-8"),
            [
                "BasegameScheduleDeclarations.FrameTick",
            ])

    scheduler_probe = repo_root / "tools/validation/Octaryn.SchedulerProbe"
    if scheduler_probe.exists():
        require_contains(
            errors,
            scheduler_probe,
            read_scheduler_probe_source(scheduler_probe),
            [
                "ValidateDefaultCapacity",
                "ValidateSerialScheduling",
                "ValidateRunsAfterScheduling",
                "ValidateRunsAfterResetsBetweenScheduleEpochs",
                "ValidateRunsAfterFailedPrerequisiteSkipsDependent",
                "ValidateRunsBeforeScheduling",
                "ValidateCommitBarrierDrainsEarlierParallelWork",
                "ValidateNestedBlockingRunRejected",
                "ValidateUndeclaredWorkRejected",
                "ValidateBlockingTryRunOrderedDependencyRejected",
                "ValidateShutdownUnresolvedWorkDiagnostics",
                "ValidateParallelCapacity",
                "FireAndForgetFailureCount",
                "LastFireAndForgetFailureWorkId",
                "access-mismatched declared work",
                "HostWorkScheduleFlags.DeterministicOrder | HostWorkScheduleFlags.RequiresTickBarrier",
            ])

    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    args = parser.parse_args()

    errors = validate(pathlib.Path(args.repo_root))
    if errors:
        for error in errors:
            print(f"scheduler contract policy: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
