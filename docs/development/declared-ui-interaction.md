# Declared document interaction

The native host supplies generic interaction for explicitly declared actions.
The game supplies source-derived layout, highlight artwork, color, controller
commands, and navigation routes. No Fallout palette or previous/next command
meaning is built into the host.

## Source findings and corrections

Pinned RmlUi `Source/Core/Factory.cpp:176` registers `input` and other form
controls, but does not register `button`. An RML `button` therefore uses the
generic element instancer. `ElementFormControl.cpp` supplies automatic tab
focus and disabled focus behavior only for actual form controls. Declared
action admission now supplies tab focus and defaults missing directional
navigation properties to `auto`; explicit source RCSS routes are preserved.
Disabled attributes on the action or any ancestor prevent focus and dispatch.

RmlUi `Context.cpp:576` updates hover on mouse motion without replacing focus.
A previously clicked row could keep `:focus` while a different row gained
`:hover`. Pointer motion now selects its nearest enabled declared action;
moving onto a backdrop clears the stale action focus. Keyboard navigation
clears pointer hover before RmlUi processes the key. Nested action targets
take precedence over their containing row. Clicking a passive visual child
restores focus to the declared action, allowing subsequent Enter activation.

RmlUi focus applies to the entire ancestor chain, unlike an exclusive active
tile. A nested arrow and its row can both match `:focus`. The former
focus-only snapshot selectors consequently displayed two source states.
The host now marks only the exact selected declared action with
`active_declared_action`. Game-owned CSS uses this class for its unique source
snapshot; RmlUi's ordinary focus chain remains intact for navigation. The
selected ID is cached, so only the old and new action classes change.

Pointer mode reconciles the current RmlUi hit target after context updates,
including stationary-cursor menu swaps and logical-canvas resizing. Keyboard
navigation and activation select keyboard mode and deactivate pointer hover;
a stationary pointer cannot steal focus until real pointer input resumes.

The pinned SDL adapter scales motion coordinates by window pixel density but
its button handlers use the last cached position. The host now synchronizes
the button event's coordinates using the same density conversion before
forwarding down/up. Window focus loss clears hover and preserves the saved
pre-modal relative-mouse preference for restoration when the modal closes.

RmlUi's `GetElementAtPoint` projects the pointer through the element transform.
Logical document scaling and letterboxing therefore use native transformed
hit testing. Transform state is updated during RmlUi render, not just update.
Bitmap field reflow observes computed source image tint and skips unchanged
text, width, and tint. Passive source artwork can use `pointer-events:none`.

## Qualification

Run `python tools/validation/check_declared_interaction.py` from the repository
root. The fixture links the production `DeclaredDocumentUi.cpp` and the pinned
RmlUi library. It calls RmlUi's context input/update/render methods against a
CPU render sink. It does not create a window, send OS input, or submit GPU work.
The current receipt records 63 passing checks under
`build/windows-x64/tools/declared-interaction/`.

Coverage includes transformed 960×540, 1920×1080 and letterboxed hit testing,
passive overlays, single selection, exact nested targets, explicit navigation
routes, Up/Down/Tab/Enter/Space, stale click positions, disabled ancestors,
bitmap hover/idle tint, unchanged-field caching, and hide cleanup. Exactly
64 actions are admitted; 65 are rejected, and the pending queue remains 64.
The nested snapshot regression retains ordinary ancestor focus, reproduces
both legacy snapshots being visible, and proves exclusive active-action
display. Stationary-cursor menu swaps and resize reconciliation are included.
Immutable `v5-a` and `v5-b` evidence snapshots preserve the 55- and 63-check
receipts, binaries, source copies, and hashes. The failing new-menu fixture
before reconciliation is retained under `v5-stationary-before`.

Bitmap text bindings can opt into ordinary word wrapping with `wrap_width`, a
finite logical pixel width from zero through 16384. Zero preserves existing
single-line and explicit-newline behavior. Wrapping measures scaled declared
glyph advances, breaks at spaces, removes the separating spaces at a soft break,
and preserves explicit newlines, glyph bearings, alignment and tint. An
unbreakable word can exceed that width; it is not silently split. Bitmap font
metadata can provide `lineHeight` for an authored line advance separate from its
header `height`; omission preserves the existing height. Both managed and native
admission reject invalid bounds and wrapping on non-bitmap bindings or actions.
This generic subset does not translate game-specific tilde, tab or icon controls
and does not establish complete native Fallout text wrapping parity.

The bitmap-only `wrap_to_element` option uses the element's current logical
client width instead of `wrap_width`. Resize reflow recomputes glyph layout
without replacing the document. `fit_text_height` sets the field height to the
font's authored `measuredHeight` plus `lineHeight` for every additional line,
scaled by the binding scale; empty text has zero height. Both font metrics fall
back to header height when omitted. To size a surrounding matte automatically,
the field must participate in normal flow, for example with `position:relative`
and `display:block`; absolute glyph children do not provide intrinsic height.
Parent padding then contributes to its autoheight and an absolute background
with `height:100%` spans that padded box.

Logical canvases default to `fit_mode:"contain"`. Opting into `"height"` scales
uniformly by physical viewport height divided by declared logical height, places
the document at the viewport origin, and derives logical width from viewport
width divided by that scale. Percentage positions and widths therefore adapt
to the viewport aspect. Only those two fit modes are admitted, and height fit
requires a bounded logical canvas. Resize keeps the same document and animation
timeline. RmlUi rounds some layout positions to logical pixels; geometry
retention and this fit policy do not disable that existing layout behavior.

Left/Right navigate according to RmlUi routes; Enter/Space activate the focused
action. This does not establish original controller key-driven value mutation,
gamepad parity, pixel parity, visible GPU highlighting, or presentation FPS.
Mouse capture restoration is reviewed source behavior pending visible manual
qualification. Full native compilation and actual imported-menu captures are
separate checks owned by the packaging/runtime lane.
