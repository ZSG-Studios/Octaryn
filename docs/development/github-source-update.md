# Restored engine source update

This update brings the active native/managed engine work onto the existing
ZSG-Studios/Octaryn history. It was prepared from the restored working engine;
the unrelated zsg-studios-workspace Git metadata was not used as its parent.

## Included work

- Standalone Slang RHI rendering, native Windows DX12 selection and Vulkan
  validation, with reproducible pinned dependency patches and shader tooling.
- Restored terrain, sky, HDR, fluids, player, selection and RmlUi presentation.
- A Terraria-inspired 50-slot inventory, creative block catalog, cursor stacks,
  drag/drop, and authoritative item drops and pickups with persistence.
- Pinned FSR 2.2.1 integration with Slang shaders, native AA and quality modes,
  sharpening, custom scale and GPU-timed dynamic resolution controls.
- Exact terrain cave-noise caching and streaming/meshing/renderer improvements
  without LOD or reduced geometry.
- A menu event fix: pointer movement and drag notifications no longer activate
  settings or menu buttons. Clicks and intentional right-click adjustments are
  handled explicitly; a packaged regression check covers this behavior.

## Qualification

The active Windows bundle built successfully. Targeted CPU checks cover settings
persistence, dynamic-resolution behavior, exact terrain output, streaming,
server authority and lifecycle. The terrain oracle checked 258827 conditions
and 4.2 million voxels against the prior scalar implementation.

Packaged DX12 and Vulkan checks exercised FSR inputs and dispatch, mode switching,
resize/history resets, UI interactions, world items and rendering. The final
FSR settings matrix completed four 600-frame cases, with actual dispatch metadata
confirming custom scale, sharpening and bounded GPU-driven resolution changes.
The hover regression passed in a separate 600-frame packaged UI run.

See the linked development reports for exact evidence paths and limits. Logs,
captures, saves, downloaded reference checkouts, dependency/build outputs and
temporary work folders are intentionally local; they are not committed source.

## Remaining limits

- Native Linux/macOS and Metal execution are not qualified by Windows tests.
- The inventory is not full Terraria parity: armor, accessories, crafting,
  survival loot and consumable placement remain incomplete.
- Streaming can still hitch on synchronous initial GPU meshing; sustained
  moving-center streaming needs further qualification.
- FSR 2 provides reconstruction, not frame generation. Dynamic resolution
  cannot guarantee its target FPS when CPU or fixed GPU work is the bottleneck.

Rebuild the configured Windows tree with
`python tools/build/windows.py --action build --preset release-windows --target octaryn_client_bundle`.
This is the verified existing-tree build command, not a claim that every
platform's fresh-machine bootstrap has been tested.
