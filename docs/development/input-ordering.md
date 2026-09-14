# Ordered block actions

Updated 2026-09-13. The original
`ref/upstream-octaryn/references/old-architecture/source/app/runtime/events.cpp`
handles block clicks and wheel selection in event order. With block A selected,
right-click followed by wheel selection of B places A, then selects B. Wheel
followed by right-click places B.

The restored client previously collapsed clicks into per-frame booleans and
wheel movement into a sum. Its dispatcher always cycled selection before placing,
so both sequences placed B and repeated clicks collapsed into one operation.

## Active implementation

`octaryn-client/Source/App/OpenWorld/ActionQueue.h` retains up to 64 cycle, pick,
break and place actions per frame. `Controls.cpp` appends accepted events in
order, and `ActionFeedback.h` dispatches that exact sequence. A full queue keeps
the accepted prefix and drops later actions; Controls writes one explicit
overflow warning per session. Zero wheel movement consumes no queue slot.
Frame startup and noninteractive/benchmark handling clear the queue.

Existing event acceptance rules remain intact. A valid click before a later menu
opening or focus loss remains accepted, matching the original immediate action;
opening a modal does not erase earlier authorized input. Modal events remain
captured, the first uncaptured click reacquires mouse capture, and wheel selection
while uncaptured remains the original behavior. This change preserves block-action
ordering within a batch; it does not introduce event-time camera snapshots.

Every edit still requires `BlockInteraction::make_edit` and successful
`LocalSession::submit_block_edit`. The client does not mutate authoritative world
data. Audio follows successful actions in their accepted order: valid selection
changes/picks, and edits accepted into the local command queue. Queued feedback
does not claim that the server applied the edit. Attack presentation still advances
its sequence at most once per frame containing gameplay break/place input,
including an edit input dropped by queue overflow; no dropped edit is submitted.

## Verification and limits

`tools/Source/ClientPlayerModelProbe/ActionFeedbackProbe.cpp` exercises the actual
queue and dispatcher with a controlled interaction/command sink. It covers both
place/wheel orders and selected block IDs, repeated clicks, invalid targets,
rejected submissions, already-selected picks, feedback order, idle/zero wheel,
64-action prefix preservation, dropped suffixes and clearing without replay.

`build/ordered-actions-build.log` records the native application link and passing
canonical player-model probe, including `action_feedback=passed` and the existing
presentation, animation, pose and residency checks. These are CPU/build results.
No injected SDL events, gameplay runtime, audio playback or GPU validation was
performed for this change. The complete 151-file stage now passes shader,
module/server payload, hash and ownership checks in
`logs/client/ordered-actions-stage.json`. `build/ordered-actions-bundle.log`
reaches the final install and stops at WinError 5 while the running game holds
the canonical directory open. This ordered-action fix and the earlier pending
audio, query, culling and mesh updates remain staged together, not installed.
