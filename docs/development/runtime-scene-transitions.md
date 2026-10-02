# Prepared runtime scene transitions

The client exposes `host.transition/v1` to declared game modules. The core owns
the current view, bounded transition mailbox, source verification, renderer
replacement and local authoritative session. Games own destination selection,
coordinate conversion, interaction rules, script effects and loading artwork.

The current replacement adapter accepts version 1 prepared static glTF scene
descriptors. A declared scene asset confines the descriptor to its package.
Before retiring the current renderer, the shared scene preparation verifier
checks resource closure, hashes, limits and supported cook generations. Its
reservation uses the existing shared preparation ledger. This does not yet
qualify replacement of every tiled scene/catalog combination.

The client module survives replacement and its API hooks are rebound. A ticket
is complete only after a destination authoritative pose and successful renderer
frame. The ticket remains loading while resources or the server are preparing.
Loading frames count only when actually drawn; event pumping alone is not a
render heartbeat. Texture batches yield to the main thread at completed upload
boundaries, with no concurrent worker renderer access. Individual slow uploads
remain exposed by `map_texture_upload_progress` diagnostics.

Replacement now runs authority stop/start on a joined native scheduler task while
the main thread presents loading frames. CPU-only map preparation, texture cache
reads, decoding and mip construction give presentation an exclusive renderer
lease at a 60 Hz cadence. Stage boundaries share that cadence so cheap texture
operations do not force redundant frames. Module ticks run only at presentation
boundaries; they cannot call host graphics APIs while the map worker owns the
renderer. Texture diagnostics separate maximum CPU preparation and RHI upload
time. A single RHI upload or pipeline creation remains indivisible and requires
runtime timing evidence before smoothness can be qualified.

The loading gate stays active through destination authority and the first
successful rendered frame. Failed replacement keeps its ticket loading while
restoring the source scene; the failure is published after the restored authority
and renderer are ready, or immediately when restoration itself fails.

Scene transfers write a private spawn manifest and pass
`OCTARYN_SERVER_MAP_TRANSFER_SPAWN=1` to the owned local server. The server then
applies that pose even if a source-scene position was saved. Explicit transfers
bypass the new-player floor-search heuristic, which must not relocate an
authored destination to the top of nearby visual geometry. Normal startup
passes zero and preserves saved-position precedence. This is a local process
handoff, not a remote-server teleport protocol or a complete cross-scene save
format. Failed destination preparation attempts to reopen the previous scene
with its current view pose; restoration failure ends the session.

Hidden authored-camera route fixtures exercise source game interactions,
transition tickets, resource replacement, retained client state and authority
arrivals. They do not qualify physical movement, collision-ray picking, gameplay
save parity or original-game interaction timing.
