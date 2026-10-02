# Module graphics preferences

`host.graphics` domain 12, version 1, exposes `IHostGraphicsApi` through the
optional `ModuleHostContext.Graphics`. The client supplies it; the authority
server and missing/old native callbacks report unavailable. A module must request
the API and declare scheduled `host.graphics` access. A read permits `TryGet`;
a write permits both `TryGet` and `Apply`. Declare only the write when editing
preferences so the scheduling graph has one access per resource. Disposing the
activation revokes its wrapper.
Native graphics callbacks reject worker-thread access as unavailable; window
operations and preference mutation run only on the SDL main thread.

The snapshot contains requested fullscreen/window dimensions, presentation,
frame cap, FSR mode/scales/sharpening/dynamic target, ray tracing, PBR/POM,
fog/clouds/sky and shadow/reflection quality and ranges. Actual render/display
dimensions and ray-tracing availability are separate readback; they are ignored
on apply. No window handles, paths, native renderer objects or product labels
cross the module boundary.

`Apply(settings, persist)` rejects invalid values before mutation. Native SDL
display failure leaves the controls unchanged and attempts to restore the prior
window. Pending SDL window changes are synchronized and actual fullscreen/windowed
dimensions checked before accepting or persisting them. OS size constraints or a
denied/asynchronous window request can reject the transaction. Rollback also
synchronizes and verifies the prior state. An explicitly logged rollback failure remains an OS failure, not an
atomicity guarantee. `Applied` means accepted preferences consumed by the next
renderer frame; it does not promise completed pipeline/resource preparation or
successful presentation. `AppliedNotPersisted` means the live preferences changed
but the existing atomic settings save failed. Preferences are written only to
the host-selected settings path. The module cannot choose a path.

Window dimensions are 64..16384, present modes 0..2, upscalers 0..6, quality tiers
0..3 and traced ranges 0..1024. Frame cap is 0 (uncapped), 1 (display refresh), or
30..240; FSR target is 30..240. Sharpness is finite 0..1, render/minimum scales are
finite 1/3..1, and maximum scale is minimum..1. Enabling unavailable hardware ray
tracing returns `Unavailable`. Window dimensions apply to windowed mode; monitor
selection/exclusive refresh modes and lighting calibration are not in this API.
Fullscreen retains the host's existing display-mode selection.

The C table is 24 bytes; its versioned preference/readback struct is 88 bytes.
Both native layout assertions and managed fake callback checks pin the layout.
Graphics/render/UI algorithms remain owned by the existing engine, with game
menus and labels declared by the selected module.

`host.input` flag bit 5 is `HostInputState.MenuPressed`, a non-repeat Escape edge
observed before modal UI capture. Existing input snapshot sizes and versions are
unchanged. The game module chooses which menu to toggle. External-game modal
documents remember and restore mouse capture; basegame Escape handling remains
in its own UI adapter. No input injection is used for contract verification.

`host.application` domain 13, version 1, provides the optional
`ModuleHostContext.Application.RequestExit()`. It requires both a requested API
and a scheduled write grant. Read access does not authorize exit. Missing native
callbacks and authority-server contexts leave it unavailable; disposed wrappers
reject requests. The 16-byte native table has one callback at offset 8.
Accepted requests set the current client loop's running flag to false; ordinary
module, server, renderer, window and SDL cleanup still runs. No process handle,
termination operation or exit-code control is exposed to game modules.
Exit callbacks also reject worker-thread calls to keep the loop flag owned by
the client main thread.

Declared screen actions are local presentation events consumed through
`host.ui.TryPollAction`. The map session's authority relay drains a separate
first-party gameplay queue. A local menu action is never automatically sent to
the server; game modules explicitly choose any host operation after polling it.
