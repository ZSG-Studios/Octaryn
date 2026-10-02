# Game-owned presentation

The client core owns the RmlUi context, SDL input routing and the existing RHI
renderer. It does not select an inventory, product menu, stylesheet, colors or
font for an external game. The selected module declares its documents and
resources through `host.ui`; no Fallout XML or trait interpretation belongs to
the engine.

First-party presentation moved from `octaryn-client/Source/Ui/GameUi` and the
world-library startup menu into `octaryn-basegame/Source/Client/Ui`. This includes
inventory policy, legacy `module_panel` layout, world-library declaration loading
and native menu actions. Former client UI files/fonts now belong to
`octaryn-basegame/Assets/Ui/Game`; world-library assets remain under
`octaryn-basegame/Assets/Ui/WorldLibrary`. CMake selects these sources/resources
only for basegame. External games select `ModuleGameUi` and `ModuleMenu`, without
loading the basegame document, pixel font, inventory or world-library menu.

Existing app bridge entrypoints remain for callers, but external-game product
inventory/menu operations return unsupported or perform no presentation. The
external shell keeps generic camera/window controls and bounded loading cancel
state; it supplies no default product menu. Notifications return unavailable
without a game presentation consumer. Renderer startup presents a clear frame
while its normal initialization diagnostics run; the former hardcoded
"Starting Octaryn" RML/font surface was removed. Basegame subsequently starts
its existing document normally.

`IHostUiApi.TryPresentScreen(assetId, fields)` accepts the additive
`declared_document` model through the existing UI ABI callbacks:

```json
{
  "version": 1, "model": "declared_document", "screen_id": "my.game.hud",
  "document": "my.game.hud.document", "resources": "my.game.hud.resources",
  "styles": ["my.game.hud.style"], "fonts": [], "modal": false,
  "fields": [{"id": "message", "element": "message-text"}],
  "actions": [{"id": "my.game.hud.accept", "element": "accept-button"}]
}
```

Manifest asset kinds are `ui` for the screen JSON, `ui.document` for RML,
`ui.style` for RCSS and `ui.resources` for the resource index. Shared/client/server
validation requires passive declarations under `Assets/Ui/`. Server packaging
removes actual UI/audio resources while retaining the module metadata; authority
does not load presentation. UI writes require the module's declared `host.ui`
request and a scheduled write grant.

The resource index is `{ "version": 1, "resources": [...] }`; each entry contains
`id`, `path`, `kind` (`image` or `font`) and lowercase `sha256`. Paths are normalized
and relative to the index directory, confined under module `Assets`, without
links. References use `asset:<local-resource-id>`. Admitted images are PNGs;
optional indexed fonts are TTFs. No undeclared path, URL, import or fallback font
is supplied. The prepared declaration is cached for this module activation;
resource packages must remain unchanged until it ends. Hash preflight does not
claim an atomic snapshot against an external process replacing files afterwards.

RML permits a bounded passive tag subset, unique IDs and text-leaf field bindings.
Script, template, linked stylesheet, DTD, event-handler and data-binding markup
are rejected. Declared styles are appended in declaration order after inline
styles. CSS URLs resolve only indexed images. Narrow `@spritesheet` declarations
resolve indexed `src` and validate sprite rectangles against image dimensions;
`<img sprite="..."/>` uses those original atlases. Other at-rules and image
decorators require a future explicit admission grammar. This follows
[RmlUi's sprite-sheet syntax](https://mikke89.github.io/RmlUiDoc/pages/rcss/sprite_sheets.html).

Admission is eight cached screens and 2 MiB of prepared envelopes, 32 text
fields, 16 namespaced actions and a 64-entry native action queue. Per screen:
256 KiB document, 128 KiB aggregate style text, 8,192 elements with depth at most
64, 256 indexed resources, 64 MiB resource bytes, four fonts, 4 MiB per resource,
4,096 pixels per image dimension and 16 million aggregate image pixels. Native
envelopes are capped at 1 MiB and expanded markup at 512 KiB. Fields are escaped
text, not replacement markup or script execution. `modal:false` HUD documents
do not capture the camera; modal game documents own input capture.

`tools/validation/validate_ui_ownership.py` checks source/resource selection
without running a client. Managed admission checks, compilation, headless module
activation and actual GPU UI output require separate receipts. Admission of a
converted game document does not establish complete original UI behavior.

The client and server activators dispose their module UI wrapper on activation
failure and normal shutdown. Disposal hides every admitted screen and rejects
subsequent wrapper calls. Hiding a declared document closes it, removes its click
listener and pending actions, and releases its native cache slot; presenting it
again recreates the document from the activation's admitted envelope. First-party
legacy panels retain their existing hide behavior within the basegame adapter.
