# World library

Ordinary startup opens the world library before loading map geometry or starting
authority. Add World chooses one or more `.glb`/`.gltf` files through SDL's native
file dialog; Find in folder searches a chosen directory. Search matches names and
source paths. Select a world and Open or Continue. Start a new save preserves the
previous save, and the save selector appears when a world has multiple saves.

The implementation reuses the pinned RmlUi, SDL3, fastgltf, Glaze, Taskflow,
Box3D and shared character mover. Screen declarations, markup, styles and fonts
belong to `octaryn-basegame/Assets/Ui/WorldLibrary`. Client code executes the
declared screen and handles the platform file dialogs and background imports.
Persistence and gameplay snapshots remain server/module-owned.

UI feedback uses the same client OpenAL Soft backend as gameplay audio. The
basegame sound catalog defines separate short hover, activation and value-change
tones. A context listener covers menus, selectors, sliders and inventory controls;
typing, disabled controls and programmatic UI refreshes remain silent. Hover and
slider feedback is throttled. The headless server package omits presentation
assets.

## Sources and saves

The catalog is `saves/worlds/catalog.json` under the repository root during
development, or the application preference directory in a standalone package.
Each world has a stable random ID. Saves live under
`saves/worlds/<world-id>/<save-id>/`; world and save names are display text.
`world.json` associates a save with its catalog entry.

Source assets stay in place. A glTF file must retain its referenced buffers and
images. Startup, Add World, Find in folder and Locate source file read bounded
library/manifest metadata and filesystem availability only. They do not parse
GLB/glTF payloads, hash resources, load geometry or collision, or validate a
prepared scene catalog. An unselected entry identifies a source path; it does
not certify that the source is playable. Save names and IDs remain available
without reading the world payload.

Open, Continue, a selected save and New save queue a cancellable loading job.
The loading screen receives the world/save name before that worker starts.
Only the selected world then validates its source resource graph and hashes,
prepared scene seal and starting position. Raw scenes receive a collision-
qualified spawn; adjacent authored manifests keep their declared spawn.
Invalid or incomplete sources produce an explicit error and create no save.
Discovery is bounded, and all source work runs through the native scheduler.

Large scenes first inspected during loading can require preparation. That
result is stored in catalog metadata, so later library refreshes show
Preparation needed without reopening the source. Prepare World is an explicit
selected action with progress and cancellation. Open and new-save creation
remain unavailable until the starting area passes geometry admission and
authority spawn qualification. A canceled operation never hands a world to the
session loader. If cancellation arrives after a valid save commit, that save
remains available in the library. Source errors remain distinct from
preparation status.

Prepared geometry lives in a source-fingerprint cache; locating changed content
selects a different cache without changing world or save IDs. A sealed sidecar
binds source resources, catalog digest and qualified spawn to the prepared map
manifest. Windows file I/O supports long Unicode paths without shortening IDs
or changing serialized paths. Cancellation preserves completed cache work.

Preparation currently qualifies the starting area. Traversal into uncooked parts
is blocked; the runtime does not yet cook additional regions on demand. A source
catalog alone does not make a large world playable. Zorah's 128 m render area is
rejected before cooking because current per-part allocations require 92.9 GB.
Its source assets and complete catalog remain intact. It is registered in the
development library with preparation status and no playable save.

Source fingerprints cover map files and referenced resources. A missing source
is identified by its path during listing; changed resource contents are checked
when that world is loaded. Locate registers the replacement path and retains
world/save identity; the next selected load validates it before reopening a
save. Timestamp-only changes are accepted when the content digest still
matches. Selecting a world reloads rendering and collision geometry even when
the source path is unchanged. Existing bundled-world progress is copied into the
first managed save; asset files and the original progress files are retained.

## Loading ownership

`WorldLibraryController` queues selected work until the loading screen has been
presented. Its worker owns source validation and save selection; `MainMenu`
receives a completed save path once. Progress reports the current stage and
elapsed time without inventing a completion percentage. Informational
cancellation messages and source/loading errors have separate presentation.

`SessionStartup` owns `LocalSession` until the main thread accepts its prepared
result. Cancellation rejects that handoff and the worker stops authority before
releasing ownership. `MapStartup` resolves the chosen manifest on its worker,
then prepares item assets, map geometry, presentation and prediction collision.
Item GLB loading and ray capacity preparation begin only after world selection.
Item preparation keeps meshes, pipelines and retained pose history in a local
owner until every asset and ray capacity check succeeds. Failed candidates use
the existing fence-aware mesh cleanup, so retrying does not inherit a partial
asset lookup. The retained history allocator moves with its table owner.

Each map loading stage transfers presentation ownership explicitly. The worker
waits while the main thread draws a loading frame, and grants recurring frames
only during declared CPU work. GPU work revokes that grant before it resumes.
During an exclusive GPU phase, the main thread handles quit and cached Cancel
input without running RmlUi layout or submitting renderer work. CPU-phase
cleanup revokes the grant before releasing graphics resources. Cancellation is
also handled while waiting for the authoritative player. Recoverable loading
failures stop authority, release the selected map, and return to the library;
they do not overwrite source assets or discard completed save data.

## Authority

`world-state.save` contains the player pose, world clock and bounded module
snapshot. Basegame snapshots preserve hotbar counts/selection, world-item
identities and motion, entity allocation watermark and receipt namespace.
Restoring a snapshot does not grant starter inventory again.

The server captures snapshots on its owner thread and writes through a bounded,
ordered queue every five seconds. Graceful shutdown flushes a final snapshot.
Each file has version/module compatibility checks and a SHA-256 envelope;
writes flush a temporary file before atomic replacement. Corrupt or incompatible
saves are rejected without overwriting their contents. A new receipt session
namespace is persisted before gameplay begins. An abrupt process or machine
failure can lose changes after the latest completed snapshot.

## Qualification

- Build: `python tools/build/windows.py --action build --preset release-windows --target octaryn_all octaryn_world_library_probe octaryn_ui_audio_probe`
- Silent audio: `build/release-windows/tools/native/bin/octaryn_ui_audio_probe.exe build/release-windows/client/bundle/Assets/Audio/action-sounds.json`
- Imports: `python tools/validation/validate_world_library.py --probe build/release-windows/tools/native/bin/octaryn_world_library_probe.exe`
- Authority: `python tools/validation/validate_world_saves.py --server-bundle build/release-windows/server/bundle --evidence-root logs/tools/world-saves`
- Packaged menu and restart: `python tools/validation/validate_world_library_runtime.py --client-bundle-root build/release-windows/client/bundle`

The packaged driver uses isolated libraries/settings, hidden capped windows and
a process watchdog. UI checks dispatch internal RmlUi contract events without
injecting OS input. Inspect its actual GPU menu captures as well as logs.

The current Windows DX12 bundle passed the full build recorded in
`logs/build/menu-feedback/verified-all.log`. The packaged library run passed
674 general UI, 112 library, 64 loading and 19 audio checks. Before selection,
all four payload counters stayed zero: source parses, resource hashes, model
loads (including item GLBs) and prepared-catalog reads. GLB and external-buffer
glTF worlds loaded, retained dropped items across restart, and kept their saves
isolated. Invalid selection returned to a visible library with the loading
overlay hidden. GPU captures at 640x480, 1280x720 and 1920x1080 were inspected;
status text remained readable and the loading indicator stayed inside its track.
Evidence is in `logs/client/menu-feedback/library-qualified/runtime-bovmjxvt/`
(`result.json`, `visual-audit.json`, captures and client logs).

Five cancellation cases passed: queued validation, starting authority,
preparing geometry, entering the player wait, and cancellation during that wait.
Each returned to the visible library, left no authority process running and
preserved original sources. Captures and visibility markers both confirmed the
loading overlay was hidden. Evidence is in
`logs/client/menu-feedback/cancellation-qualified/cancel-u_6wdzxe/`
(`result.json` and `visual-audit.json`). An earlier run at
`logs/client/menu-feedback/cancellation-qualified/cancel-t9g7c3cc/` reported a
catalog atomic replacement failure that did not reproduce; its exact OS error
remains unknown and the failed evidence is retained.

The unchanged watchdog passed with a sustained 50 ms frame threshold and
two-second stall limit. Isolated setup frames reached 163.014 ms for the GLB
fixture and 477.129 ms for glTF; this establishes responsive recovery and bounded
qualification, not hitch-free startup. Current results qualify these Windows
DX12 fixtures, not full Zorah readiness or other platform runtime behavior.

The current guarded listen/connect check passed three remote sessions and two
menu returns using the copied 12-triangle authored fixture. Each reload retained
the remote endpoint and refreshed the same source. Client and dedicated server
exited zero, with no owned descendants surviving before safety cleanup. The
50 ms sustained-frame and two-second stall limits stayed unchanged. This run
used normal process priority while eight Unity shader compiler workers were
observed consuming CPU; its inspected GPU frame and logs are in
`logs/client/menu-feedback/rejoin/rejoin-osvp6myd/`. Earlier below-normal runs
remain failed evidence: `rejoin-k6hmzsxv` stalled before its first menu return,
`rejoin-yowhgf2w` never reached server readiness, and `rejoin-cq3_0urb` timed out
connecting. CPU contention was observed, but the exact cause of each failure is
not established. `rejoin-eb44uc7o` completed one Bistro session and then exceeded
the heartbeat limit during the next texture upload; it does not qualify Bistro
rejoin. Loading frames currently lack frame-timing heartbeat records.

The timing reader now handles CSV recreation between sessions without resetting
active watchdog deadlines or slow-frame history. Its 43 tests include real file
truncation, replacement, continued frames, genuine stalls and slow sessions:
`logs/tools/zorah-import/watchdog-session-csv.log`.

The ordinary visible launch used the existing 2560x1440 user settings and
default audio device. Its inspected world-library capture confirms a hidden
loading overlay and zero preselection payload counters:
`logs/client/menu-feedback/live/menu-20260930-130516/`. A subsequent manual
selection started validation after the menu capture; listing did not preload it.

The actual Zorah registration-only probe completed in approximately 0.19 seconds
with its 10 GB payload locked against reads, all four counters zero, stable
identity after restart and no save created:
`logs/tools/world-library-lazy/actual-zorah-registration.log`. This verifies
catalog-only registration; the 92.9 GB preparation admission limit above still
blocks playing the complete source. The item ownership regression also passed:
discarding a failed candidate preserved the original table, committing kept its
allocator address stable, eight replacement generations made no new upstream
allocations, and destruction left zero live bytes. Its 1,000/10,000-item history,
snapshot rotation and reservation checks are recorded in
`logs/tools/world-library-lazy/item-history-ownership-tests.log`.

Earlier Windows DX12 qualification, before the loading feedback changes, passed
the full build, nine-world
import probe, 141 silent audio checks, and packaged UI contracts (674 general,
102 library and 19 audio checks). Two independent gameplay saves retained item
counts and identities across restart. Menu captures were inspected at 640x480,
1280x720 and 1920x1080, including Continue and new-save actions. The Bistro map
smoke exited zero with a stable settled pose and an inspected GPU capture.
Menu-to-world startup uses a dedicated geometry upload fence, followed by an
explicit completed-timeline handoff to frame rendering. The packaged two-world
save/restart run exercises that transition. Evidence is under
`logs/client/zorah-integration/menu-current/runtime-_usl6lvd` (earlier bundle)
and `logs/client/zorah-integration/menu-handoff` (initial fence repair).
Dedicated listen/connect completed three sessions with two menu returns,
refreshing the same source path on each return. These results qualify Windows;
other platform runtime behavior remains unverified.

The later lazy-library native regression passed with Windows payload files
locked against reads. Discovery, Add/Find, restart and saved-world listing each
recorded zero source parses, resource hashes, model loads and prepared-catalog
reads. Selecting the fixture recorded two parses, two hashes and one model load;
other payloads remained locked. Cancellation before validation and during
progress, canceled save selection, relocated sources, independent saves and
deferred invalid-source failures passed. The ordinary probe covered 13 entries
(nine valid, four rejected on selection), and the preparation probe confirmed
zero prepared-catalog reads during menu refresh. Evidence:
`logs/tools/world-library-lazy/lazy-plw6c6sz`,
`logs/tools/world-library-lazy/probe-b0oj0yfn`, and
`logs/tools/world-library-lazy/prepare-centered-20260930.log`.
Those native tests cover library I/O and save ownership; the current packaged
tests above separately exercise the asynchronous menu/session handoff.
