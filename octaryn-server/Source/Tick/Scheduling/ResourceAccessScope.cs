using System.Collections.Concurrent;
using Octaryn.Shared.Host;

namespace Octaryn.Server.Tick;

internal sealed class ResourceAccessScope : IDisposable
{
    public static readonly ResourceAccessScope Empty = new([]);

    private readonly IReadOnlyList<LockedResource> _lockedResources;
    private bool _isDisposed;

    private ResourceAccessScope(IReadOnlyList<LockedResource> lockedResources)
    {
        _lockedResources = lockedResources;
    }

    public static ResourceAccessScope Enter(
        ConcurrentDictionary<string, ReaderWriterLockSlim> resourceLocks,
        ScheduledSystemDeclaration declaration)
    {
        var requestedResources = declaration.Reads
            .Concat(declaration.Writes)
            .GroupBy(resource => resource.ResourceId, StringComparer.Ordinal)
            .Select(group => new ScheduledResourceAccess(
                group.Key,
                group.Any(resource => resource.Mode == ScheduledAccessMode.Write)
                    ? ScheduledAccessMode.Write
                    : ScheduledAccessMode.Read))
            .OrderBy(resource => resource.ResourceId, StringComparer.Ordinal)
            .ToArray();
        if (requestedResources.Length == 0)
        {
            return Empty;
        }

        var lockedResources = new List<LockedResource>(requestedResources.Length);
        try
        {
            foreach (var resource in requestedResources)
            {
                var resourceLock = resourceLocks.GetOrAdd(
                    resource.ResourceId,
                    _ => new ReaderWriterLockSlim(LockRecursionPolicy.NoRecursion));
                if (resource.Mode == ScheduledAccessMode.Write)
                {
                    resourceLock.EnterWriteLock();
                }
                else
                {
                    resourceLock.EnterReadLock();
                }

                lockedResources.Add(new LockedResource(resourceLock, resource.Mode));
            }
        }
        catch
        {
            Release(lockedResources);
            throw;
        }

        return new ResourceAccessScope(lockedResources);
    }

    public void Dispose()
    {
        if (_isDisposed)
        {
            return;
        }

        _isDisposed = true;
        Release(_lockedResources);
    }

    private static void Release(IReadOnlyList<LockedResource> lockedResources)
    {
        for (var index = lockedResources.Count - 1; index >= 0; index--)
        {
            var lockedResource = lockedResources[index];
            if (lockedResource.Mode == ScheduledAccessMode.Write)
            {
                lockedResource.ResourceLock.ExitWriteLock();
            }
            else
            {
                lockedResource.ResourceLock.ExitReadLock();
            }
        }
    }

    private readonly record struct LockedResource(
        ReaderWriterLockSlim ResourceLock,
        ScheduledAccessMode Mode);
}
