# Loading presentation observations

Declared product documents retain their RmlUi animation timeline when fields
are republished. The SDL system interface uses its performance counter in
seconds, and the host updates the context before each drawn loading frame.
RmlUi supports opacity and transform keyframes through its
[RCSS animation syntax](https://mikke89.github.io/RmlUiDoc/pages/rcss/animations_transitions_transforms.html).
Product artwork and animation rules remain game-owned.

Managed declared-document admission now accepts structurally validated
`@keyframes` alongside confined spritesheets. Each stylesheet allows at most
64 named rules, 64 frame groups per rule, 16 selectors per group, and 64
properties per group. Selectors are `from`, `to`, or percentages from 0 to 100.
Unknown at-rules, imports, nested/malformed rules, duplicate animation names and
empty frames remain rejected. Image URLs inside animations pass through the
same indexed-resource resolver as ordinary CSS.

`OCTARYN_CLIENT_LOADING_TRACE=1` records each actual drawn loading frame with
request epoch, frame index, elapsed time, presentation gap, UI update/draw time,
and capture time. Input-only pumping does not increment the render heartbeat.
The external module shell begins a new observation epoch on `show_loading`.

Set `OCTARYN_CLIENT_LOADING_CAPTURE_DIR` for viewport screenshots in
`loading-<epoch>/`. By default, each loading stage is captured once per request,
so a repeated transition is no longer suppressed by the initial loading pass.
For an animation series, set `OCTARYN_CLIENT_LOADING_CAPTURE_INTERVAL_MS`
(50 through 60,000 milliseconds) and optionally
`OCTARYN_CLIENT_LOADING_CAPTURE_COUNT` (1 through 256, default 64). Captures use
actual context/window dimensions without expanding document bounds or changing
panel styles. Other document-capture callers retain their existing defaults.

Screenshot readbacks synchronize GPU work and affect subsequent frame gaps.
Use a separate run with tracing enabled and captures disabled to measure loading
cadence; a screenshot series qualifies composition and motion only.

`tools/validation/check_loading_animation.py --document <document.rml> --style
<style.rcss>` checks an authored eight-slide loading presentation through the
production `DeclaredDocumentUi` and pinned RmlUi library. It samples two 40-second
cycles with a controlled 60 Hz clock, checks opacity conservation/continuity,
slide order, wheel transform changes, stable unchanged-field publication and
first-slide restart for a new request. The supplied markup/styles are hashed in
the receipt. The test replaces text-field metadata with a single empty text
binding and uses a CPU rendering sink. It does not test resource admission,
bitmap fonts, actual GPU composition, live loading cadence or window presentation.

The OpenFNV authored source passed 92,049 checks over 4,801 samples with 704
crossfade samples and 4,800 wheel transform changes. Initial evidence is under
`build/windows-x64/tools/loading-animation/v1/`; native build and viewport-series
qualification are separate requirements.

The v12 visibility optimization passes 135,370 production-Rml checks in
`build/windows-x64/tools/loading-animation/v12-visibility-v1/`. Throughout both
cycles only the active slide or its crossfade pair is visible. Every slide with
positive opacity is visible, inactive slides are hidden during hold periods,
and the opacity and wheel checks remain unchanged. This is controlled-clock
validation; GPU upload/draw cost and live cadence require separate measurements.

For an indexed mesh presentation, add `--screen <screen.json>` and
`--mesh-expectations <source-projection.json>`. The latter is a product-owned
independent source export containing `parts`, each with `element`, `hidden`,
`mesh` vertices/indices and optional animation duration/keyframes. The probe
resolves the supplied screen's mesh bindings and verifies indexed mesh/image
hashes, then admits the ordinary generic mesh envelope through production
`DeclaredDocumentUi`. It samples a 10-second initial hold followed by two
80-second slideshow cycles by default. The expected source events start each
fade at 10, 20, 30 seconds and so on; each fade lasts 20/9 seconds after its
event, instead of finishing at that event. Optional `--slide-seconds` and
`--fade-seconds` arguments change the event interval and fade duration.

For each animated source mesh, every sample compares the actual RmlUi rotation
primitive with the source endpoint angle and loop duration, preserving full
turn counts and direction. At 10 Hz a CPU render sink checks that all four
visible generic meshes draw the source-exported vertices, UVs and triangle
indices exactly once; static parts must remain static. Field stability,
visibility during fades, opacity conservation and new-request restart checks
still apply. Opacity timing allows 0.004 error for the pinned Rml float animation
accumulator's few milliseconds of drift across long cycles. The CPU sink does
not validate GPU texture sampling or layer
composition. Mesh and image hash checks do not replace managed document
admission, and bitmap-font admission remains a separate check.

## Indexed image sampling

An image resource may declare `"sampling":"linear"` and
`"alphaSampling":"straight"`, plus `"addressing":"wrap"`. The defaults are
`point`, `premultiplied`, and `clamp`;
other values and these properties on non-image resources are rejected.
The host applies sampling metadata after path confinement, image bounds and
SHA256 admission. Ordinary images, CSS image URLs, spritesheets, bitmap-font
atlases and indexed meshes inherit the image's metadata. Mesh `wrap` remains
independent of filtering and alpha sampling.

With straight alpha sampling, PNG RGBA uploads retain source RGB. The GPU
filters straight RGBA, then multiplies RGB by the sampled alpha before applying
RmlUi's premultiplied vertex tint and existing premultiplied blend operation.
This preserves the distinction between filtering before and after
premultiplication. It introduces no alpha threshold or added shadow blur.
Generated RmlUi textures and images without metadata retain their previous
point sampling and premultiplied upload behavior.

The host-generated `octaryn-ui-linear:`, `octaryn-ui-straight:` and combined
`octaryn-ui-straight-linear:` prefixes encode this policy at the native renderer
boundary. Wrapped variants append `-wrap` before the colon. Authored module
paths still require ordinary confined paths; URI prefixes are not admitted as
resource paths.

`tools/validation/check_rml_texture_sampling.py --out <fresh-output>` checks
the production URI policy, wrapping idempotence, changed translation units,
and DXIL/SPIR-V shader compilation. `HostContentProbe` checks actual managed
resource admission and bitmap/spritesheet propagation. GPU composition and
original-game visual parity require separate captures.
