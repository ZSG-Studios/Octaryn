using Octaryn.Shared.Host;

namespace Octaryn.Shared.GameModules;

public static partial class GameModuleValidator
{
    private static void RequireScheduleResourceConflicts(
        ModuleValidationReport report,
        IReadOnlyList<ScheduledSystemDeclaration> systems)
    {
        var orderedPairs = BuildOrderedSchedulePairs(systems);
        for (var leftIndex = 0; leftIndex < systems.Count; leftIndex++)
        {
            for (var rightIndex = leftIndex + 1; rightIndex < systems.Count; rightIndex++)
            {
                var left = systems[leftIndex];
                var right = systems[rightIndex];
                if (string.IsNullOrWhiteSpace(left.SystemId) || string.IsNullOrWhiteSpace(right.SystemId))
                {
                    continue;
                }

                if (IsOrdered(orderedPairs, left.SystemId, right.SystemId))
                {
                    continue;
                }

                if (ConflictingResources(left, right).FirstOrDefault() is { } resourceId)
                {
                    report.AddError(
                        "module.schedule.resource.conflict_unordered",
                        $"Scheduled systems with write conflicts must declare ordering. Left: {left.SystemId} Right: {right.SystemId} Resource: {resourceId}");
                }
            }
        }
    }

    private static IEnumerable<string> ConflictingResources(
        ScheduledSystemDeclaration left,
        ScheduledSystemDeclaration right)
    {
        var leftReads = ResourceIds(left.Reads);
        var leftWrites = ResourceIds(left.Writes);
        var rightReads = ResourceIds(right.Reads);
        var rightWrites = ResourceIds(right.Writes);

        foreach (var resourceId in leftWrites.Intersect(rightWrites, StringComparer.Ordinal)
            .Concat(leftWrites.Intersect(rightReads, StringComparer.Ordinal))
            .Concat(rightWrites.Intersect(leftReads, StringComparer.Ordinal)))
        {
            yield return resourceId;
        }
    }

    private static HashSet<string> ResourceIds(IReadOnlyList<ScheduledResourceAccess>? resources)
    {
        return (resources ?? [])
            .Select(resource => resource.ResourceId)
            .Where(resourceId => !string.IsNullOrWhiteSpace(resourceId))
            .ToHashSet(StringComparer.Ordinal);
    }

    private static HashSet<(string Before, string After)> BuildOrderedSchedulePairs(
        IReadOnlyList<ScheduledSystemDeclaration> systems)
    {
        var systemIds = systems
            .Select(system => system.SystemId)
            .Where(systemId => !string.IsNullOrWhiteSpace(systemId))
            .ToHashSet(StringComparer.Ordinal);
        var directEdges = systemIds.ToDictionary(systemId => systemId, _ => new List<string>(), StringComparer.Ordinal);

        foreach (var system in systems)
        {
            if (string.IsNullOrWhiteSpace(system.SystemId) || !directEdges.ContainsKey(system.SystemId))
            {
                continue;
            }

            foreach (var dependency in system.RunsAfter ?? [])
            {
                if (!string.IsNullOrWhiteSpace(dependency) && directEdges.TryGetValue(dependency, out var dependencyEdges))
                {
                    dependencyEdges.Add(system.SystemId);
                }
            }

            foreach (var dependency in system.RunsBefore ?? [])
            {
                if (!string.IsNullOrWhiteSpace(dependency) && directEdges.ContainsKey(dependency))
                {
                    directEdges[system.SystemId].Add(dependency);
                }
            }
        }

        var orderedPairs = new HashSet<(string Before, string After)>();
        foreach (var systemId in systemIds)
        {
            AddReachablePairs(systemId, systemId, directEdges, orderedPairs, []);
        }

        return orderedPairs;
    }

    private static void AddReachablePairs(
        string root,
        string current,
        IReadOnlyDictionary<string, List<string>> edges,
        HashSet<(string Before, string After)> orderedPairs,
        HashSet<string> visited)
    {
        if (!visited.Add(current))
        {
            return;
        }

        foreach (var next in edges[current])
        {
            orderedPairs.Add((root, next));
            AddReachablePairs(root, next, edges, orderedPairs, visited);
        }
    }

    private static bool IsOrdered(
        HashSet<(string Before, string After)> orderedPairs,
        string left,
        string right)
    {
        return orderedPairs.Contains((left, right)) || orderedPairs.Contains((right, left));
    }

    private static void RequireScheduleGraph(
        ModuleValidationReport report,
        IReadOnlyList<ScheduledSystemDeclaration> systems)
    {
        var systemIds = systems
            .Select(system => system.SystemId)
            .Where(systemId => !string.IsNullOrWhiteSpace(systemId))
            .ToHashSet(StringComparer.Ordinal);

        foreach (var system in systems)
        {
            RequireScheduleDependencies(
                report,
                system.SystemId,
                system.RunsAfter ?? [],
                systemIds,
                "runs_after");
            RequireScheduleDependencies(
                report,
                system.SystemId,
                system.RunsBefore ?? [],
                systemIds,
                "runs_before");
        }

        var edges = new Dictionary<string, List<string>>(StringComparer.Ordinal);
        foreach (var systemId in systemIds)
        {
            edges[systemId] = [];
        }

        foreach (var system in systems)
        {
            if (string.IsNullOrWhiteSpace(system.SystemId) || !edges.ContainsKey(system.SystemId))
            {
                continue;
            }

            foreach (var dependency in system.RunsAfter ?? [])
            {
                if (!string.IsNullOrWhiteSpace(dependency) &&
                    edges.TryGetValue(dependency, out var dependencyEdges))
                {
                    dependencyEdges.Add(system.SystemId);
                }
            }

            foreach (var dependency in system.RunsBefore ?? [])
            {
                if (!string.IsNullOrWhiteSpace(dependency) && edges.ContainsKey(dependency))
                {
                    edges[system.SystemId].Add(dependency);
                }
            }
        }

        RequireAcyclicScheduleGraph(report, edges);
    }

    private static void RequireScheduleDependencies(
        ModuleValidationReport report,
        string systemId,
        IReadOnlyList<string> dependencies,
        IReadOnlySet<string> systemIds,
        string direction)
    {
        foreach (var dependency in dependencies)
        {
            if (string.IsNullOrWhiteSpace(dependency))
            {
                continue;
            }

            if (dependency == systemId)
            {
                report.AddError(
                    $"module.schedule.{direction}.self",
                    $"Scheduled system cannot depend on itself. System: {systemId}");
                continue;
            }

            if (!systemIds.Contains(dependency))
            {
                report.AddError(
                    $"module.schedule.{direction}.unknown",
                    $"Scheduled system dependency is not declared. System: {systemId} Dependency: {dependency}");
            }
        }
    }

    private static void RequireAcyclicScheduleGraph(
        ModuleValidationReport report,
        IReadOnlyDictionary<string, List<string>> edges)
    {
        var visiting = new HashSet<string>(StringComparer.Ordinal);
        var visited = new HashSet<string>(StringComparer.Ordinal);

        foreach (var systemId in edges.Keys)
        {
            if (VisitScheduleNode(systemId, edges, visiting, visited))
            {
                report.AddError(
                    "module.schedule.graph.cycle",
                    $"Scheduled system dependency graph contains a cycle. System: {systemId}");
                return;
            }
        }
    }

    private static bool VisitScheduleNode(
        string systemId,
        IReadOnlyDictionary<string, List<string>> edges,
        HashSet<string> visiting,
        HashSet<string> visited)
    {
        if (visited.Contains(systemId))
        {
            return false;
        }

        if (!visiting.Add(systemId))
        {
            return true;
        }

        foreach (var next in edges[systemId])
        {
            if (VisitScheduleNode(next, edges, visiting, visited))
            {
                return true;
            }
        }

        visiting.Remove(systemId);
        visited.Add(systemId);
        return false;
    }
}
