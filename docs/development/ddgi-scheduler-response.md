# DDGI bounded scheduling response — 2026-09-18

## Causes repaired

- Dirty probes previously received a 200,000-point bonus, equivalent to about
  195 seconds of age. Continuous world/light publication could leave initialized
  tunnel probes untouched throughout a capture. Eligibility at 0.25 seconds did
  not guarantee service.
- The single-probe fresh reserve used rendered frame modulo four. Fractional
  work credit at high refresh could repeatedly dispatch on other frames.
- A consumed hard-removal generation remained rejected if a gentle wake arrived
  before the next scheduler invocation. Repeated gentle wakes pinned the marker.
- Fine-16's 32,768 probes shared the same 128-per-60-Hz baseline budget as
  coarse-128's 12,288 probes. Available GPU headroom could not increase throughput.

## Implementation

`DDGISchedule.cpp` reserves one in four executed selections for oldest eligible
history, one for uninitialized geometry, and the remainder for dirty-priority
work. Empty lanes lend their slots to priority work. No selection is duplicated.
Service accounting advances only when work is selected, independent of rendered
frame number. Recycled slots receive their arrival time; ongoing streaming cannot
make every fresh arrival artificially older than retained history. Dirty repeats
do not reset waiting age. Solid and provisional-hole probes remain excluded;
their pending wakes survive until they become traceable.

Hard rejection survives its trace/update dispatch. On a subsequent frame, a
completed generation retires even if a gentle wake is pending; that wake retains
the gentle marker. A newer hard generation remains rejected until retraced.

`DDGITiming.h` owns four timestamps in each of two fence-owned frame slots per
volume. It measures trace and update separately, excluding the other volume's
work. Readback occurs when that frame slot is next reused, after the existing
frame fence, without an extra wait. It operates in ordinary startup as well as
explicit qualification; CSV profiling is not required.

Each volume gets a conservative 0.35 ms per 1/60 second allowance. Measured
cost is normalized by scheduled rays plus 64 update-work units per probe; budget
estimation charges the full ray tier. Cost increases apply immediately, decreases
use a 0.9/0.1 filter, and work-rate growth is limited to 12.5% per valid sample.
Actual throughput is additionally limited by active-volume refresh demand,
fractional elapsed-time credit, and a 4,096-probe allocation/dispatch ceiling.
Pending urgent work requests a 0.1-second cadence but cannot bypass measured
cost. No valid timing means the original conservative configured rate. Idle
credit is discarded and elapsed-time accumulation is capped at 0.1 seconds.
These are scheduling targets, not GPU-latency or convergence guarantees.

An actual uncapped integration capture exposed a fixed-overhead feedback bug in
the initial adaptive controller: budgets fell from 167/291 to 2–5 probes per
60-Hz tick. One-probe dispatch overhead was charged as per-probe cost, shrinking
the next dispatch further. The 60-Hz capture did not exhibit sustained collapse.
The scheduler now accumulates already-earned credit into approximately one
60-Hz tick of work (at least 64 probes, bounded by available work and capacity).
It skips intermediate dispatches rather than splitting that work across hundreds
of tiny uncapped-frame dispatches. This amortizes command/timestamp overhead
without borrowing work credit or increasing the GPU allowance.

The subsequent timing diagnostic confirmed a second path: coarse frame 3296
traced only 19 eligible probes (2,736 work units) in 0.07444 ms, reducing the
estimated rate from 494 to 53.6. The preceding 411-probe dispatch took 0.1182 ms.
Every dispatch now carries its intended amortized probe target through the
fence-owned timing slot. A smaller eligible tail is excluded from proportional
cost estimation. Its measured cost is still paid: excess over already-charged
full-tier probe cost first consumes banked work credit, then creates GPU-time
debt repaid from subsequent elapsed-time allowance. Thus even a genuinely
expensive short dispatch throttles future work without pretending that its fixed
overhead scales with all probes. Expensive full batches retain immediate cost
backoff. Idle frames also repay time debt.

Set `OCTARYN_DDGI_TIMING_PROFILE_PATH` to a path prefix for optional `.coarse.csv`
and `.fine.csv` files. They include source/resolve frame and slot, probe/work
counts, separate GPU trace/update times, elapsed/frame times, before/after
budgets, estimated work cost, backlog/oldest age/credit, intended batch target,
partial-batch flag and outstanding GPU-time debt.

The independent volume radius/count mapping and saved settings are unchanged.
The new light-change tracker is called once before preparing either volume and
after both statistics resets. It replaces the obsolete per-volume position-only
light matcher. Acceleration publication now invalidates the full trace influence
distance. Existing logging adds both volumes' pending count, oldest traced age,
and adaptive work rate so initialization cannot hide stale retained histories.

Reference inspected: upstream NVIDIA RTXGI `rtxgi-sdk/src/ddgi/DDGIVolume.cpp`
(`DDGIVolumeBase::Update`, `ComputeScrolling`, and `GetRayDispatchDimensions`).
RTXGI updates ray rotation/scroll state and exposes the full volume's dispatch
dimensions; it does not supply this engine's bounded CPU selection policy. No
vendor source was copied.

## CPU verification

Production `DDGISchedule.cpp`, compiled with MSVC C++20/O2:

- Existing 35 scheduler/configuration regression cases pass.
- New response harness covers fine 0/6/16/32 and coarse 0/128/1024, independently
  disabled volumes, 30/60/144 Hz, one-probe caps, continuous dirty publications,
  blocked streaming probes, repeated hard/gentle generations, cost increases,
  invalid timing, growth limits, long pauses and idle-credit reset.
- An additional 240 Hz case exercises frame/work-credit aliasing.
- Under a **synthetic** affordable rate of 1,024 full-tier probes per 60-Hz tick,
  fine-16's first sweep takes 0.533–0.542 seconds and coarse-128 0.250–0.267
  seconds. Fine-32 takes 4.267–4.271 seconds, explicitly demonstrating the
  remaining bounded-throughput limit rather than claiming constant latency.
- With half fine-16 repeatedly dirtied, every retained history refreshes within
  the 2.433-second fairness bound; observed oldest age at completion is
  1.067–1.083 seconds. All coarse-128 histories refresh within the 1.3-second
  bound, with observed oldest age 0.5 seconds.
- At a one-probe dispatch cap, the fresh probe is serviced in 33–67 ms across
  30/60/144/240 Hz while repeatedly edited probes remain active.
- Fixed-budget and priority-only negative controls both fail their respective
  backlog/fresh-service assertions as intended.
- A closed-loop fixed-overhead regression feeds each actual selected workload
  back into the production adaptive controller (0.045 ms dispatch overhead plus
  0.000001 ms per work unit). It passes at 30/60/144/240/600/2,000 Hz: final
  budgets are 1,203–1,330 per 60-Hz tick and oldest retained age is 0.43–0.47
  seconds. The unbatched negative control reproduces throughput collapse and
  fails the same assertion. These simulated GPU costs are not measured hardware
  results; actual uncapped/capped requalification remains necessary.
- The actual 19-probe diagnostic sample is replayed in the production controller:
  it preserves the 494-probe full-batch estimate while charging excess GPU time.
  A 4 ms full-batch sample still reduces throughput immediately; a 2 ms partial
  sample pays its excess exactly through banked credit and time debt. The
  proportional-tail negative control fails the diagnostic regression.
- Continuous dirty-publication fixtures alternate 1/19/76/101/411/1,024 eligible
  probes at 30/60/144/600/2,000 Hz. Final rates remain 967–1,053 per 60-Hz tick;
  five-second simulated GPU cost is 13.6–74.3 ms against a 105 ms allowance.

Logs: `logs/build/ddgi-live-response-scheduler.log`,
`ddgi-live-response-scheduler-existing.log`, and
`ddgi-live-response-scheduler-negative-{fixed,priority}.log` in the same folder.
The added harness is `tools/validation/ddgi_schedule_response_test.cpp`.

Native application linking and actual GPU timing, image convergence, and backend
qualification are coordinated by the main repair session. CPU synthetic timing
does not establish GPU throughput or visual correctness.
