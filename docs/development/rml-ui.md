# RmlUi presentation

The client UI uses RmlUi 6.2, SDL3 input, FreeType, and the existing standalone
Slang RHI Vulkan renderer. The UI renders after HDR presentation. The former
compute overlay and ImGui rendering paths have been removed.

## Ownership and styling

`octaryn-basegame/Assets/Ui/game.rml` declares the basegame screens and
`game.rcss` defines their appearance. The original RCSS uses crisp pixel borders,
navy panels, mint highlights, and warm gold text. Silkscreen Regular is bundled
under its SIL Open Font License; no Terraria artwork is copied.

Client `Source/Ui/GameUi` owns document lifetime, input routing, and binding to
existing settings/actions. `Source/Rendering/Ui/RmlRenderer` supplies retained
geometry, PNG/font textures, premultiplied-alpha blending, transforms, and
framebuffer scissoring. Its shaders remain first-party Slang. RmlUi's native
types are not exposed to managed game modules.

The bundled server accepts declared passive UI resources under `Assets/Ui/`
with `.rml`, `.rcss`, `.ttf`, and `.txt` extensions because it carries the same
basegame payload as the client. Authority does not load or execute those files.
Shader declarations, presentation/asset-processing phases, and client-only host
APIs remain rejected. The owner-module validation probe covers this boundary.

## Surfaces and controls

- HUD: selected-block atlas image, crosshair, and compact control hints.
- Escape: display and world appearance settings. Existing display selection,
  resolution, fullscreen, view-distance limit, and appearance toggles remain.
- F3: performance diagnostics, hidden by default and refreshed four times per
  second while visible.
- F6: lighting controls using the existing lighting-settings persistence.
- RmlUi owns menu hit testing and keyboard focus; gameplay input is consumed
  while a menu is open. F11 remains the fullscreen shortcut.

The document also represents the pre-existing main, pause, world-selection, and
server menu models. This migration does not implement missing world-management
or multiplayer services. The current client still launches its existing local
authoritative world directly. Existing render-distance and backend feature
limits remain the responsibility of their current owners.

## Build and qualification

Run from `C:/Users/Rose-X/Documents/Octaryn`:

```powershell
python tools/build/windows.py --action configure --preset release-windows
python tools/build/windows.py --action build --preset release-windows --target octaryn_client_bundle
python tools/build/windows.py --action run-client --preset release-windows
```

`--show-settings`, `--show-lighting`, and `--show-diagnostics` select an initial
visible surface for GPU inspection. `--validate-ui` checks the actual documents
and their control/layout contracts without injecting mouse or keyboard input.

The focused packaged qualification command is:

```powershell
python tools/validation/validate_rml_ui.py --client-bundle-root build/release-windows/client/bundle --evidence-root logs/client/validation/rml-ui
```

It runs separate HUD, settings, lighting, and diagnostic sessions from an
unrelated working directory with isolated worlds. Each must receive authoritative
player state, retain the complete configured terrain window, render 600 complete
frames, submit real RmlUi geometry, and capture the presented GPU image. It rejects
RmlUi/RHI warnings or errors and retains logs and captures for visual inspection.
No desktop UI automation is used. This is not a claim of Linux/macOS validation
or a manual play-through of every control.

The renderer reports retained geometry and texture counts once, on its first
successful UI frame in a normal run. With `OCTARYN_CLIENT_RHI_VALIDATION` present
at renderer creation, it reports only successful UI frames 1, 301, and 601, then
stops. The flag is cached at creation; error reporting remains unconditional.
The qualification script enables this flag and requires the initial report.

## Verified on Windows x64, 2026-09-13

- Packaged build: `logs/client/rml-build-complete.log`.
- Final runtime evidence: `logs/client/validation/rml-ui/rml-7xp_h8uw/`, with
  separate `hud`, `settings`, `lighting`, and `diagnostics` subdirectories.
  Every case completed 600 fully resident frames, 81 columns, and 1,483,600
  world quads on Slang RHI/Vulkan/RX 9070 XT, with no RmlUi or RHI warnings/errors.
- Each case passed 543 document, binding, slider, focus-eligibility, and layout
  checks across three resolutions. Actual GPU captures used the current saved
  display mode of 2560x1440 and were visually inspected for text, panel bounds,
  atlas preview, crosshair, settings, sliders, and diagnostics.
- Retained geometry counts remained stable at 15 HUD, 55 settings, 38 lighting,
  and 78 diagnostics, sampled at frames 1, 301, and 601. Diagnostic compile counts
  rise during the intentional four-per-second text refresh; retained counts do
  not grow. These short runs do not establish long-session memory or performance.
- Owner module validation passed four accepted passive UI resources, eleven
  rejected paths/extensions, and four unchanged authority restrictions.
  Evidence: `logs/client/rml-build-final.log`.
- Native player-model regression checks, shader bundle/source comparison,
  module asset declarations/layout, and native owner boundaries passed.

Those earlier UI captures show oversized gray player geometry near the lower
screen. The later integrated first-person capture resolves this through the
head/torso index filter; see pipeline-parity.md. The UI captures remain evidence
for the RmlUi checks above, not the final player composition.
