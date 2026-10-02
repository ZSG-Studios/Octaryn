# Scene body authority

`host.scene-physics` exposes `IHostScenePhysicsApi` through the optional
`ModuleHostContext.ScenePhysics` slot. Modules must request the capability and
declare the resource in their schedule. The native client table is domain 16,
version 1: it submits bounded JSON commands and reads authority snapshots.

The local dedicated server owns catalogue admission, Box3D body creation,
raycasts, collision occlusion, grabbing, body removal and simulation. The client
may submit aim, grab, move, release and pickup requests. Request acceptance is
asynchronous: modules may credit their inventory only after a successful pickup
receipt. `SetAim` and `MoveGrab` require continued publication; stale aim loses
its target and a grab releases after 250 ms without movement intent.

`OCTARYN_SERVER_SCENE_PHYSICS_DIR` selects the local session mailbox directory.
`scene_physics.commands.json` is a versioned cumulative journal with an epoch,
sequence and at most 256 commands. The sequence counts commands, independently
of each module request ID. `scene_physics.snapshot.json` acknowledges that epoch
and sequence and publishes the absolute scene source path, authority tick,
selected source body, body poses and at most 256 action receipts. A presentation
consumer must reject snapshots belonging to another scene.

The server loads the adjacent `.physics.json` catalogue for the active GLB.
It admits at most 8192 bodies, each with at most 256 authored shapes. Body source
IDs must be unique and nonzero. Collectible eligibility, source item identity
and stack count come from this host-admitted catalogue, never from a command.
Unsupported source collisions must remain deferred during content preparation;
visual bounding boxes are not a substitute for source shapes.

The authority runs physics through its native scheduler worker after the player
tick barrier. It limits interaction reach to three metres, checks requested aim
near the authoritative player and uses the fresh world raycast surface point
for a grab anchor. Native removal completes before a successful pickup receipt.
Removed identities remain retained for the lifetime of that scene session.
Pickup also credits an authority inventory ledger with the admitted
source stack count. Capacity is checked before native removal: at most one
million items per base identity and 4096 distinct identities. Snapshot `Inventory`
publishes these counts. Before a successful pickup receipt, the host atomically
commits removed identities and inventory to `scene_physics.state.json` in the
shared local session directory. Source body poses are committed once per second
and at orderly shutdown. Loading this state preserves collected identities and
body placement when the dedicated server is replaced during a door transition.
Commit failure stops authority before acknowledging success. The safe shared API
can query authoritative inventory for recovery after an interrupted receipt.
The state also retains the last 256 action receipts for the caller journal epoch,
including the successful pickup receipt in the same durable ledger commit.
A replacement server recovers these receipts for that epoch, so pruning an
acknowledged command does not lose a module's outstanding transaction result.
New caller epochs start fresh receipt histories and may not reuse old successes.
This is an engine-owned compatibility state, not a retail Fallout save format.

The client mirrors admitted source shapes and authoritative poses into its
prediction collision world; it does not step a second dynamic authority.
This local mailbox does not implement remote network packets or retail save
inventory. Those require separate transport and save consumers. Catalogue
acceptance and a managed build alone do not prove source conversion fidelity
or live rendering parity.
