# Native/shader response contract — 2026-09-18

Coordination contract for the concurrent native and shader repair:

- `DDGIControl.padding.z` is a bit mask: `1` gentle pending wake, `2` hard
  recursive rejection, `4` lighting-only refresh. Local addition is `5`, removal
  is `6`. Test hard rejection using `& 2`, never numeric equality.
- A pending geometry refresh clears `4` and is not overwritten by a later light
  change. Geometry still bumps `refreshFrame`; lighting-only changes must not
  unlock relocation, clear geometric validity or reset distance moments.
- History is retained per executed observation. `DDGIUpdate` discards irradiance
  outright only for a new cell or relocated/embedded probe (`reset`) or a genuine
  occlusion change (`refreshFrame > metadata.y` without bit `4`). Every other
  change adopts a bounded reactive policy (six observations leaning `.5`, then
  the mature `.94`) so one noisy ray sample never replaces the field.
- The observation that follows any explicit change spends the full lighting ray
  tier; ordinary refreshes keep the active/background tiers.
- Native gradual environment events are O(1) no-ops for scheduling: sun and sky
  drift is sampled by the ordinary age-based cadence, which avoids re-injecting
  ray noise every frame. An abrupt environment change requests a bounded global
  reactive response (refresh generation, lighting-only marker, six observations).
- Native scheduling uses the existing sample tiers and GPU credit/debt limits.

There is no `ddgiEnvironmentTracking` uniform: bounded response is expressed
through the control bits and the reactive countdown.