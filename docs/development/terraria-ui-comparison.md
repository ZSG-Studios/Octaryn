# Terraria UI reference comparison

Reviewed 2026-09-13. This is a visual comparison of Octaryn's inventory against
the classic Terraria PC inventory, not a claim of Terraria feature parity or a
pixel-perfect reproduction. Reference screenshots are research evidence only;
they are not shipped game assets.

## Reference evidence

- [Annotated Terraria 1.4 inventory](https://static.wikia.nocookie.net/terraria_gamepedia/images/8/85/1.4_Inventory.png/revision/latest?cb=20200608222401)
  shows the ten-column, five-row inventory, coins/ammo, trash, and distinct
  armor/vanity/dye/accessory slots. Saved as
  `logs/client/ui-reference/terraria-1.4-inventory.png`.
- [Classic PC gameplay and crafting screenshot](https://gamescrack.org/wp-content/uploads/2019/03/Crafting_guide.jpg)
  shows the left inventory and crafting area, with equipment at the right edge
  and unobscured gameplay between them. Saved as
  `logs/client/ui-reference/terraria-crafting-gameplay.jpg`.
- [Re-Logic UI upgrade description](https://terraria.org/news/terraria-1-3-user-interface-upgrades)
  describes hover edge highlights and inventory-related icon improvements.

The first image is explicitly from 1.4; the second is an older gameplay guide.
Neither is evidence of exact latest-version 1.4.5 interface parity.

## Comparison

| Visible feature | Octaryn before this pass | This pass / remaining gap |
| --- | --- | --- |
| Inventory at upper left | Present | Preserved |
| Ten columns, five rows; first row hotbar | Present | Preserved, 50 working slots |
| Blue translucent cells with softened corners | Blue, square corners | Rounded corners and revised blue fill |
| Gold active-slot emphasis | Gold border, blue interior | Gold border and warm selected interior |
| Equipment separated at right | Small panel beside left grid | Right-edge tool section on wide screens |
| Compact blue controls | Large teal accents | Blue rounded utility controls |
| Item artwork and text | Octaryn blocks, Silkscreen font | Original assets retained; typography differs |
| Armor, vanity, dyes, accessories | Absent | Absent; Hand is the only actual equipment |
| Coins, ammo, trash and crafting | Absent as dedicated systems | Still absent |
| Creative browser | Octaryn catalog | Not Terraria Journey research/duplication |

The grid composition is similar. The complete interface is not yet very close:
equipment density, crafting, specialty storage, typography and item artwork are
material differences. A numeric similarity percentage would be unsupported.

## Octaryn evidence

Before: `logs/client/validation/ui-responsive-final/rml-di9jkve2/inventory/frame.bmp`
(1280x720), visually inspected beside both references.

Current source changes are in `octaryn-basegame/Assets/Ui/inventory.rcss`.
Packaged and source stylesheets have matching SHA-256:
`963F9A529313F344F4A80CBBF1978B485B1E064CE16BC1D40714ACE75A8525D6`.

Fresh DX12 captures were visually inspected:

- `logs/client/validation/terraria-reference-proof/inventory-1280/frame.bmp`
- `logs/client/validation/terraria-reference-proof/inventory-640/frame.bmp`
- `logs/client/validation/terraria-reference-proof/creative-1280/frame.bmp`

Each case passed 600 frames with 81 columns, 320431 quads, clean exit, 679 UI
checks and 1721 inventory checks across four viewport sizes. No UI/RHI warnings
or errors. Adjacent `client.log` files contain results. These tests establish
rendering and interaction contracts, not a numeric visual similarity score.
All 50 inventory slots remain visible; the right-side tool and navigation fit
at both captured sizes. The creative search and 23-block catalog also fit.

## Visual proof

Terraria reference (older PC layout):

Terraria reference — local evidence: `logs/client/ui-reference/terraria-crafting-gameplay.jpg` (not distributed in source).

Octaryn before this pass:

Before — local evidence: `logs/client/validation/ui-responsive-final/rml-di9jkve2/inventory/frame.bmp` (not distributed in source).

Octaryn after this pass, captured from the real DX12 renderer:

After at 1280x720 — local evidence: `logs/client/validation/terraria-reference-proof/inventory-1280/frame.bmp` (not distributed in source).

After at 640x480 — local evidence: `logs/client/validation/terraria-reference-proof/inventory-640/frame.bmp` (not distributed in source).
