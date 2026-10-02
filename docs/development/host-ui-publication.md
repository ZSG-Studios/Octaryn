# Declared module UI publication

`host.ui` version 2 retains notification/action polling and appends declared
screen publication and hiding. Its native table is 40 bytes on the supported
64-bit ABI: version/size, notification and action callbacks, then presentation
and hiding pointers at offsets 24 and 32. Native table layout assertions keep
managed and C projections aligned. Game modules request `host.ui` and declare a
scheduled UI write before publishing; unknown scheduled host resources and UI
access without the requested grant remain rejected.

Managed `IHostUiApi.TryPresentScreen(assetId, fields)` accepts a module-owned UI
asset declaration, not a filesystem path. The scoped host resolves its confined
`Assets/.../screen.json`, rejects links/traversal, checks the screen ID against
the declaration, and passes validated metadata to the client backend.
Declarations use model `module_panel`, a title, field IDs/labels and action
IDs/labels. Actions must be inside the screen ID namespace. The host builds the
document; values are escaped plain text, never module-provided executable markup.

Bounds are 8 KiB for the declaration, 32 fields, 16 actions, 4 KiB per published
text field and 16 KiB total field JSON. The native pending-action queue admits
64 entries. `TryPollAction` drains a dedicated module-panel queue so existing
WorldLibrary/session actions retain their host-owned route. Publishing failure
returns false. Headless providers do not present screens; server policy accepts
confined passive UI metadata/support assets without bundling presentation payloads.

F8 toggles the presented panel. Escape or a game-declared Close action can hide
it. Updating hidden fields does not force it open. Shutdown closes the document
and releases the owning game instance's resources.

`InvariantText` exposes bounded, pure invariant numeric parsing/formatting to
game code. It does not expose process culture objects or setters. Game source
and binary policies continue to deny direct `CultureInfo` and reflection access.
This keeps catalogue and command numbers deterministic without broad framework
permissions. The external game binary probe accepts an explicit trusted
`--shared-project` path; a module cannot grant itself a different host assembly.

Managed checks cover actual registration and JSON parity, client/server policy,
declared asset confinement, bounds, namespaced actions, finite number/culture
behavior, publication retry, headless operation and the game command/state path.
Native visual qualification is separate from these managed checks; none of this
establishes original-game menu fidelity, replicated gameplay or full-game parity.
