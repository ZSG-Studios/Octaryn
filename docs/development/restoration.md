# Source provenance

The current owner layout originated in the restored `octaryn-workspace-dev`
archive. Implementation history is retained in Git. The separate read-only
`ref/upstream-octaryn` checkout at commit
`3557cbfdc803ec034122bb55070b62b3b43b5588` supplies original presentation source
under `references/old-architecture/source` for targeted parity work.

The full earlier local workspace is preserved outside the active repository at
`C:\Users\Rose-X\Documents\Octaryn-Backups\2026-09-13-before-old-engine\workspace`.
Do not modify it or use its build outputs as proof of the current application.
Reference checkouts and local backups are not required release payloads.

The active renderer is standalone Slang RHI, not the archive's Slang GFX path.
The [presentation source map](presentation-restoration.md) preserves relevant
owner mappings; [networking recovery](networking-recovery.md) explains why the
previous BEPU/network transport cannot be copied into the current Jolt session.
Current [architecture](../architecture/current.md), [build](../build/README.md)
and [feature status](feature-parity.md) replace obsolete migration plans.

The redundant tracked `references/` archive was removed from the release checkout
after all 401 original-architecture files matched SHA-256 hashes in both external
reference copies under the active development checkout. The vertex-pulling note
also matched its external copy. No external backup/reference was removed. Git
history retains the source archive; the release tree contains only active systems.
