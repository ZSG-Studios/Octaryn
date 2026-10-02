# Bounded frame-cap waiting

The original failed Goodsprings capture is preserved in the sibling OpenFNV
`logs/goodsprings-world-hidden-v2` case. Its audit receipt is
`logs/goodsprings-pacing-audit-v1/receipt.json` with hashes of the original CSVs.
Frame 312 took 1,609.080 ms, including 1,607.450 ms inside `SDL_DelayNS`; rendering
took 1.543 ms. Further completed sleeps took 246.656, 490.023, 473.141 and
181.113 ms. The corresponding main-thread CPU samples were zero at the available
timer resolution. The screenshot frame's synchronous readback is a separate
216 ms rendering hitch, not part of the sleep diagnosis.

The final renderer frame 365 completed in 2.8894 ms. The main timing and camera
logs stop at frame 364 / playable frame 281, before the requested 300 playable
frames. This is not evidence of frame-budget exhaustion or normal shutdown.
The last unfinished call is only narrowed to the post-render phase; subsequent
qualification must inspect the new post-render trace to establish that call.

The pinned [SDL 3.4.4 Windows timer implementation](https://github.com/libsdl-org/SDL/blob/release-3.4.4/src/timer/windows/SDL_systimer.c)
arms a per-thread high-resolution timer and then waits with an infinite timeout.
The engine's cap calculation uses nanoseconds correctly. At 60 FPS its requested
sleep is at most 16,666,666 ns; there is no recovered conversion overflow.

`ResponsiveFrameWait` replaces the map session's SDL delay with an owned timer:

- One private high-resolution Windows timer; older systems can use a standard
  timer. No global timer-resolution or process-priority change.
- Relative timer due time rounded up to 100 ns, plus a **finite** wait timeout
  rounded up to milliseconds. An already-passed monotonic deadline ends the wait
  even if the timer has not signaled.
- Early wakeups recheck the same monotonic deadline. They do not remove the cap.
  API failures, backwards clocks, durations over one second and non-progressing
  wake loops fail explicitly.
- The actual elapsed wait remains in the timing evidence. Windows thread
  descheduling can exceed any timeout; this is not a guaranteed scheduling SLA
  or proof of universally corrected visible FPS.

When existing `OCTARYN_CLIENT_FRAME_CPU_TRACE=1` is enabled, `PostRenderTrace`
writes `<frame-cpu-path>.post-render.csv` through the bounded asynchronous writer.
Begin records identify pending stats, cap sleep and timing calls even when their
end records never arrive. Sleep end records include requested/actual duration,
number of waits, last finite timeout and result. The original watchdog limits,
render quality, GPU submission and screenshot capture remain unchanged.

Focused production-header CPU checks passed 24 assertions, including real Windows
timer waits and authored early-wake, delayed-signal, overscheduled, failed-wait,
clock and pending-call trace cases. Receipt:
`build/windows-x64/tools/frame-cap-wait-v1/result.json`. Run directly with
`python tools/validation/check_frame_cap_wait.py --out <new-evidence-directory>`.
This does not perform a GPU test or CTest. Canonical compilation and matched
hidden/visible qualification remain separate requirements.
