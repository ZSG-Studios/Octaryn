# Item and UI continuation

2026-09-27. Continuation of the existing item-physics and RmlUi work. This
records the item, UI renderer, and action delivery repairs and their evidence.

## Behavior repaired

- `ItemSystem.TryCollectEntity` returns success only when inventory receives
  items. A full inventory leaves the world stack intact and emits no grant.
- `InteractionSystem` publishes a target change when its count changes, even
  when the entity is unchanged. Partial pickup therefore updates the card.
- Interact refreshes the target from the current authoritative player view
  before collecting. Turning away between ticks cannot collect a stale target.
- The inherited physics fixes preserve the sleep timer through managed/native
  state, gather contact planes before solving, probe below the capsule bottom,
  and clamp slow supported motion. Radius pickup waits for sleep or one second
  of item age, allowing a toss to travel before collection.
- Map input now queues module actions directly: T drops one, Ctrl+T drops the
  selected stack, G interacts, and 1-0 select slots. It no longer depends on the
  empty client block inventory to initiate a module drop. Inventory authority
  remains in the server module.

## Item checks completed

`tools/validation/ItemBehaviorProbe` uses the real `ArchHostEcsApi` and game
systems. All 29 assertions passed: full-inventory rejection, partial and full
grants, exact inventory/world counts, target count refresh, repeated pickup
rejection, whole-stack drop/pickup, receipt ordering, eight simultaneous vacuum
pickups, and refreshed targeting after changing view direction.

Both mailbox runtime probes passed against the fresh Windows server bundle:

- `tools/validation/item_probe.py`: exactly one apple dropped and granted,
  with zero world-stack remainder; server exited normally.
- `tools/validation/item_interact_probe.py`: item settled, no pickup occurred
  before interact, the player remained outside vacuum range after settling,
  and exactly one grant matched the targeted entity; server exited normally.

Recorded logs are `logs/server/item-vacuum-5w5ceept/server.log` and
`logs/server/item-interact-pijdyapd/server.log`. The packaged basegame DLL was
newer than the final targeting fix when these probes ran. These logs establish
the tested server paths; they do not independently establish final inventory
UI synchronization or network delivery.

The Python probes derive the repository root from their location, isolate each
run below `logs/server`, parse complete numeric events, reject duplicate grants,
and wait for process termination during cleanup. They retain logs for review.

## UI and transport scope

`octaryn_all` built successfully with the native Windows release preset. Build
evidence is `logs/build/continue-octaryn-all.log`. This is a successful build,
not a claim of warning-free compilation or other-platform qualification.

The renderer fixes honor drop-shadow blur sigma, keep filter ordering and mask
UVs, restore the active layer after composites, initialize new filter surfaces,
use matching color formats, and end render passes before texture copies. Clip
clears and cropped copies use explicit subresource ranges. Capture and stencil
textures declare the Vulkan transfer usage required by their clear operations.
The oversized renderer was split into a focused draw submission implementation.

Map CLI captures now wait until loading is finished. Requested menu/settings
surfaces are applied after the loading screen closes; `--show-item-target`
is an explicit presentation fixture. Ordinary startup does not show that fixture.

The current renderer has separate layer, filter, clip-mask, and shader owners
under `octaryn-client/Source/Rendering/Ui`, with Slang shader assets. The card
visibility check exercises hidden/shown/hidden computed display values. The
capture fixture explicitly displays the menu or target card; its activity
counters require filter/layer execution and card clipping instead of inferring
those features from an ordinary map frame.

The menu and card each exited 0 after 320 authoritative map frames on both
Windows backends, with 710 general UI assertions and the separate
hidden/shown/hidden card contract passing. RHI and Vulkan validation reported
no errors or warnings in the passing runs. Layer, filter, shadow, and blur
counters were positive; card stencil writes were positive. All four GPU PNGs
were inspected, and corresponding DX12/Vulkan images were pixel-identical:

- DX12: `logs/client/ui-effects/run-cuklqvcx/{dx12-menu,dx12-card}`.
- Vulkan: `logs/client/ui-effects/run-okolr1b2/{vulkan-menu,vulkan-card}`.
- Pixel comparison: `logs/client/ui-effects/run-okolr1b2/backend-comparison.json`.

Gradients, mask-image, backdrop filters, saved box-shadow textures, and Metal
are not qualified by these menu/card cases. Mask storage retains the pinned
upstream renderer's immediate-use assumption.

The upstream [RmlUi render-interface reference](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/interfaces/render.html)
defines layers and filters for isolated effects, clip masks for rounded overflow,
and premultiplied-alpha blending. Use those contracts and inspected GPU captures
to assess the custom renderer. A successful compile alone is insufficient.

Current mailbox action/event transport retains bounded ordered journals and
acknowledgements. Singleplayer now has a server-owned event mailbox, consumed
through the same `LocalSession::poll_module_event` interface as remote events.
Action ACKs include the session epoch, and remote protocol version 7 requires
matching client/server bundles. Queue overflow rejects admission or reports a
failure instead of silently evicting receipts.

`logs/server/session-transport-y3ob7_1a/run.log` records passing regressions
against the actual built authority ingress and native SessionIo: repeated and
overlapping journals, malformed inputs, admitted-prefix ACKs under 256-entry
backpressure, retained suffix recovery, epoch resets, stale-epoch ACK rejection,
and ordered delivery of 256 events above 16 KiB. The event read bound is 64 KiB;
other mailbox reads keep their prior bound.

`logs/client/module-actions/run-320me6jm/results.json` records passing local
singleplayer and real loopback `--listen`/`--connect` runs. Each rendered 320
map frames and received exactly two unique one-item drop receipts from a
four-action burst (select, select, drop, drop). Target events also reached the
client; the dedicated-server world capture visibly contains the authoritative
item target card. Both `local/frame.png` and `remote/frame.png` were inspected.
The dedicated server shut down with exit code 0. The packaged client UI
contracts and RHI checks passed in both runs.

The dedicated-server reconnect check also passed:
`logs/server/rejoin/rejoin-su0vkl71/result.json` records three sessions and two
menu returns, with normal client/server shutdown. Each session rendered 240
map frames. The action runs retained a supported authoritative pose after
settling (eye height approximately 1.935 m); this is not a high-speed movement
or long-duration persistence qualification.

The inherited 560-line native host API provider was mechanically split into
`NativeHostApiProvider.cs` and `NativeHostApiPhysics.cs` without changing bridge
behavior. The final changed/new source scan has no files over 500 lines, and
`git diff --check` passes. Existing unrelated uncommitted work remains present.

The item simulation checks and UI fixture are not full qualification of a
module-backed inventory UI, world-item geometry rendering, persistence, or
end-to-end multiplayer gameplay. Do not describe the target-card fixture as
proof that a rendered dropped item was selected and collected through live UI.

## Reproduction

From the repository root in native Windows PowerShell, after configuring the
existing release preset:

```powershell
python tools/build/windows.py --action build --preset release-windows --target octaryn_all
dotnet run --project tools/validation/ItemBehaviorProbe/ItemBehaviorProbe.csproj -c Release -p:OctarynBuildPresetName=item-probe-windows
python -B tools/validation/item_probe.py
python -B tools/validation/item_interact_probe.py
python tools/validation/session_transport_probe.py
python tools/validation/validate_module_actions.py
python tools/validation/validate_session_rejoin.py --client-bundle build/release-windows/client/bundle --server-bundle build/release-windows/server/bundle --evidence-root logs/server/rejoin --port 17561
```

The managed behavior probe uses separate outputs under
`build/item-probe-windows`. The server probes use `release-windows` by default;
`OCTARYN_PROBE_PRESET` selects a different existing bundle preset.

For GPU qualification, build first, then run the capture wrapper and inspect
each resulting `ui.png` alongside `client.log` and `result.json`:

```powershell
python tools/validation/capture_ui_effects.py --backend all --surface all
```

Vulkan validation requires `VULKAN_SDK` to identify an installed SDK with the
Khronos validation layer. The wrapper writes isolated evidence under
`logs/client/ui-effects`. The explicit module-action run checks a burst through
the real UI action queue and verifies exactly two unique drop receipts from
both local and dedicated authority. It also retains an actual world capture.
