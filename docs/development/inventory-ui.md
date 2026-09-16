# Inventory and in-game menu

The native client now exposes a Terraria-inspired building inventory through its
existing RmlUi and standalone Slang RHI interface. Escape opens the in-game menu;
Inventory, Creative blocks, display settings, lighting and Controls are reachable
from that menu. Return resumes input, and Save & quit uses the existing graceful
local-session shutdown. The world continues simulating while a menu is open.

## Player controls

- I or E opens the inventory; B opens the creative catalog; Escape closes it.
- The first inventory row is the ten-slot hotbar. Number keys 1 through 0 and
  the mouse wheel select a hotbar slot. Middle click picks a target block into
  the hotbar, reusing an existing matching slot where possible.
- Click an inventory item and then another slot to move or swap it. Closing
  returns an unfinished cursor stack without losing items. Tab and Enter can
  navigate and activate slots. Sorting affects the backpack, preserving hotbar
  order; Clear removes the selected creative shortcut.
- Drag stacks between slots, or click to hold, merge and swap. Right-click
  takes or places one item. The cursor follows the pointer with its item icon
  and count; hovered entries show a tooltip. T tosses one and Ctrl+T tosses the
  held or selected whole stack. Dragging a held stack outside the inventory
  tosses it. A pending authoritative drop locks conflicting inventory edits.
- In Creative blocks, select a destination in the visible target hotbar, then
  choose a block. Search matches names and stable IDs without case sensitivity.
  Categories filter terrain, nature, lighting and fluids. Non-placeable catalog
  entries such as air, clouds and simulated flowing-fluid variants are omitted.
- Space retains jump/fly-up. E no longer also ascends. Other movement bindings
  remain displayed in Controls. The HUD contains the hotbar and current block,
  without the previous key-helper legend.

The inventory is a 10-column, five-row upper-left grid with an integrated
ten-slot hotbar, compact blue panels and gold selection borders. Icons and
type retain the project's original pixel art. This is a counted creative
palette with unlimited catalog supply, not a survival loot system. The current
basegame only registers the Hand item; the equipment panel shows that actual
tool. Armor, accessories and stat effects are not implemented or claimed.

## Ownership and storage

`octaryn-client/Source/Ui/GameUi/Inventory*` owns the catalog and building slots.
`GameUiInventory.cpp` owns the document bindings and menu transitions. Basegame
RML/RCSS supplies the presentation; the module manifest declares both stylesheets.
`App/OpenWorld/InventoryActions.h` applies queued selection and placement actions
in order through `BlockInteraction::select`. Empty slots suppress placement;
break actions and server validation remain available.

Menus capture gameplay input, including camera toggles and block actions. Opening
a menu clears previously queued edits for that frame. Relative mouse mode is
restored when leaving menus, and slot refresh preserves keyboard focus.

The client saves slots, counts, cursor reservations and durable transaction
watermarks to `<world>/client/inventory.json`. JSON stores stable block IDs.
The first load can import the previous `settings/build-palette.json`; version 1
occupied shortcuts become stacks of 999. Version 2 preserves counts. An invalid
existing save fails clearly rather than replacing its transaction history.
`OCTARYN_CLIENT_INVENTORY_PATH` overrides the location for isolated tests.

Reservations are persisted before sending toss commands; pickup credits are
persisted before acknowledgement. Atomic replacement includes file flushes
(and parent-directory flush on POSIX). Replay cannot debit or credit a stack
twice. World entity authority and bounded pending grants are described in
[world-items.md](world-items.md). Placement still follows existing creative
rules; adding counts does not silently change building into survival gameplay.

## Verification

The current drag/drop, durable counts, responsive layout and temporal rendering
qualification is recorded in [presentation-integration.md](presentation-integration.md).
The original first-pass results below are retained as history.

Build the package with `python tools/build/windows.py --action build --preset release-windows --target
octaryn_client_bundle`. Reproduce model and interaction checks with targets
`octaryn_validate_client_inventory` and `octaryn_validate_client_interaction`.

Run `python tools/validation/validate_rml_ui.py --client-bundle-root
build/release-windows/client/bundle --evidence-root logs/client/validation/inventory-ui`.
The runner uses isolated worlds, palettes and lighting files; it captures HUD,
settings, lighting, diagnostics, inventory, creative and in-game menu surfaces.
The native contracts use document events and explicit RmlUi keyboard calls,
without injecting operating-system input. Layout checks cover 1280x720,
1920x1080 and 2560x1440, including scrolling to the first and last creative block.

Historical first inventory pass on Windows on 2026-09-13: native package build; 39 inventory model and
persistence checks; production interaction selection/empty-slot regression;
573 existing UI plus 1,175 new inventory/menu checks. All seven packaged surfaces
ran 600 completed-world frames at the saved radius 16 (1,089 columns), exited 0,
and produced actual GPU captures with no RmlUi/RHI warnings or errors. Evidence:
`logs/client/validation/inventory-final/rml-m4uttor6`,
`logs/client/inventory-final-build.log`, and `logs/client/inventory-model-build.log`.
These checks do not establish manual mouse-play parity or other platform support.
The installed client was then launched with `--show-menu` against the existing
`saves/open-world-v2` world; authoritative player startup was confirmed in
`logs/client/inventory-live.log`. A first launch collided with another validation
session's shared server log; retry succeeded after that session stopped.

Implementation references: [RmlUi input](https://mikke89.github.io/RmlUiDoc/pages/cpp_manual/input.html),
[inventory interactions](https://mikke89.github.io/RmlUiDoc/pages/tutorials/dragging.html),
and [flexbox layout](https://mikke89.github.io/RmlUiDoc/pages/rcss/flexboxes.html).
