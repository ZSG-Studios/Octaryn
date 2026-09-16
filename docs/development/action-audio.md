# Action audio restoration

Updated 2026-09-13. The original four synthesized action sounds are integrated
into the native client and basegame catalog. Native compilation, action-hook
checks, 42 CPU checks and 49 actual OpenAL loopback checks pass. The recorded staged client/server bundle passed payload validation.
Current extracted-package qualification is recorded in the release report.

This supersedes the earlier inventory-only status. Default speaker output,
physical device disconnect and packaged gameplay action playback have not been
tested. The loopback test creates no speaker device or game window.

Original paths below are relative to the read-only
`ref/upstream-octaryn/references/old-architecture/source` at commit
`3557cbfdc803ec034122bb55070b62b3b43b5588`.

## Original sounds and owners

These four sounds are synthesized, not recordings requiring missing assets.
`app/audio/synthesis.cpp:10–32` supplies the parameters below. Its buffer builder
uses a miniaudio sine waveform, 48,000 Hz, mono, 4,000 samples (1/12 second),
quadratic envelope `(1 - sample_index / 4000)^2`, clamping to [-1, 1], then
signed 16-bit PCM conversion by multiplying by 32767. There are four reusable
OpenAL buffers and eight reusable sources (`app/audio/internal.h:15–26`).

| Event | Frequency | Gain | Original trigger in `app/player/blocks.cpp` |
| --- | ---: | ---: | --- |
| Place | 540 Hz | 0.18 | Lines 20/36: placement checks pass and `world_queue_block_edit` returns true. |
| Break | 180 Hz | 0.35 | Line 59: a block is targeted and the edit is queued. |
| Select | 760 Hz | 0.12 | Line 48: middle-click finds a block; plays even if it was already selected. |
| Change | 620 Hz | 0.10 | Line 75: wheel cycling resolves a placeable block. |

| Original owner | Responsibility | Smallest current destination |
| --- | --- | --- |
| `app/audio/synthesis.cpp`, `audio.cpp` | PCM generation and one miniaudio implementation translation unit | Focused client `Source/Audio/ActionAudio` synthesis implementation; sound parameters remain basegame content. |
| `app/audio/lifecycle.cpp`, `internal.h` | OpenAL device/context, four buffers, eight sources, cleanup | Same client audio owner, private OpenAL/miniaudio types. |
| `app/audio/play.cpp:5–26` | Find first non-playing source, bind event buffer, gain 1, play; drop event if all eight are busy | Preserve the bounded source pool and drop behavior. No new unbounded queue or asset system. |
| `app/runtime/startup/runtime.cpp:12`, `app/runtime/shutdown.cpp:65` | Initialize and shut down audio | Existing client application lifecycle. |
| `app/player/blocks.cpp` | Decide when game actions produce feedback | Existing OpenWorld interaction flow using basegame-defined action sounds. |

The original startup logs audio unavailable and continues if device/context
creation fails. Preserve a visible unavailable status rather than claiming audio
works. Do not copy its cleanup deadlock: `lifecycle.cpp:12` holds a non-recursive
mutex, then failure paths at lines 41/49 call shutdown, which locks it again at
line 60. Use cleanup with one lock owner and handle partial resource creation.

## Current integration and event semantics

`cmake/Dependencies/ClientDependencies.cmake` declares
`octaryn::deps::openal` (OpenAL Soft 1.25.1, static) and
`octaryn::deps::miniaudio` (0.11.25). Their source/header trees are present under
`build/dependencies/src`. The client `Source/Audio/ActionAudio` owner now links
them privately. Miniaudio synthesizes PCM; its device I/O is disabled. OpenAL
owns the device, context, four buffers and bounded eight-source pool through
RAII, including partial initialization cleanup. There is no added audio DLL.

`App/OpenWorld/OpenWorld.cpp` initializes the owner from the strict, bounded
basegame `Assets/Audio/action-sounds.json` catalog and logs availability. Invalid
content fails startup; an unavailable audio device is reported and the game
continues. `ActionFeedback.h` dispatches the existing filtered cycle, pick,
break and place edges in their existing order. The attack animation condition
remains separate because it can run without an accepted edit.

Original `world/edit/queue.cpp:30–44` only validates basic input and appends an
edit; actual application happens later at lines 47 onward. Thus original
break/place audio means **accepted into the local queue**, not an authoritative
success acknowledgement. The smallest equivalent current hook is one sound
when both `BlockInteraction::make_edit` and `LocalSession::submit_block_edit`
return true. Keep all current server authority and placement checks. Do not
replay sounds on worker retries, snapshot refreshes or render frames.

`LocalSession.cpp:196–217` returns true after accepting a command into the bounded
SessionIo queue. `SessionIo.cpp:100–133` observes command-file consumption but
receives no per-command applied/rejected result. Server
`ChunkStreamProcessBridge.cs:441–449` consumes both accepted and rejected commands;
only capacity failure is retained. File disappearance therefore cannot drive a
"successful edit" sound. A confirmed-success sound would require a separate
result contract and is outside this original-timing restoration slice.

For Select, require the same valid-target condition as `BlockInteraction::pick`,
including the already-selected case. For Change, require an actual nonzero
cycle input resolving a placeable block; do not sound once per idle frame.
Keep modal/focus event filtering in the existing controls owner. The client
owns playback; basegame sound choices should not become shared/server policy.

## Verification and remaining qualification

- `build/action-audio-reviewed-build.log`: native client links successfully;
  the canonical player probe passes its existing asset, pose, streaming and
  new action-feedback checks. The production dispatcher checks accepted/rejected
  submissions, missing target, already-selected pick, cycle and idle edges.
- `octaryn_validate_client_action_audio`: 42 CPU checks pass, including an
  independent original sine/envelope PCM oracle, deterministic output, saturation,
  invalid definitions and strict catalog rejection. Valid device creation is
  absent from this CPU mode.
- `octaryn_validate_client_action_audio_loopback`: 49 checks pass with actual
  `ALC_SOFT_loopback`, four distinct nonzero outputs, source advancement and
  completion, eight voices, ninth-request drop, reuse, counters, teardown during
  playback and reinitialization. A missing loopback extension fails the target;
  it cannot silently fall back to speakers or count a skip as a pass.
- Cached `ALC_EXT_disconnect` support checks device connection before playback
  and status and after submission. A disconnected device reports unavailable
  without counting a successful play. The connected path is tested; physical
  disconnect and injected partial-resource failures are not tested.
- Direct `Octaryn.ModuleManifestProbe` and `Octaryn.OwnerModuleValidationProbe`
  runs exit zero. The compiled manifest equals the source descriptor, including
  `octaryn.basegame.audio.actions`. The CMake aggregate module targets did not
  complete because their bundle dependency encountered the live file lock.
- `logs/client/action-audio-stage.json` records passing staged shaders, client
  and server module payloads, dedicated/bundled server equality, catalog and
  executable hashes, compiled/source manifest equality and dependency notices.
  All 723 scanned owner/tool/CMake code files satisfy the 500-line limit.
- `logs/client/action-audio-render-audit.json`: 78 active sources, 69 headers and
  24 reachable shader modules pass standalone RHI policy. Audio changes no shader.

`build/action-audio-bundle-final.log` records completed staging followed by
WinError 5 at canonical bundle replacement. Seven existing payloads differ and
five are added, including basegame assemblies, manifests and audio content.
No partial executable-only installation was attempted. After the game exits,
rerun `python tools/build/windows.py --action build --preset release-windows --target octaryn_client_bundle` to
install the coherent bundle, then qualify actual gameplay audio. No new GPU
capture, audible-output result or frame-performance result is claimed here.
