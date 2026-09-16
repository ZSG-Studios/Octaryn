# Validation

Run checks against the current owner code and packaged executable. Validation is
explicit; ordinary startup opens the game and is not a test runner.

## Check groups

| Target | Purpose |
| --- | --- |
| `octaryn_validate_static` | Source layout, contracts, package/API policy and structural checks. |
| `octaryn_validate_cpu` | Native logic, owner module and server authority/lifecycle checks. |
| `octaryn_validate_gpu` | Actual Slang RHI and packaged UI/FSR checks; requires a working GPU/backend. |
| `octaryn_validate_all` | Explicit aggregate of all three groups. |

Build a group through the platform build entrypoint after configuring. For example,
`python tools/build/windows.py --action build --preset release-windows --target octaryn_validate_cpu` selects CPU
checks. A skipped platform or unavailable device is not a passing runtime check.
The normal `octaryn_all` build compiles tools without executing validation.
Run GPU checks serially (`--jobs 1` on either platform) so
independent hardware workloads do not overlap.

## Runtime and image evidence

Run the packaged client with an isolated world and settings. Record backend,
hardware, resolution, distance, exact binary/build identity, frame count and exit
status. Inspect the actual image as well as validation-layer output. Do not use
OS input injection or generic wrapper-only success as proof of rendering.

`--frames` selects a bounded interactive run; `--diagnostic` selects the explicit
renderer diagnostic. `--benchmark-seconds` measures a settled view after residency
and warmup. It does not measure full moving-center streaming. CSV/GPU timing and
renderer readbacks support focused profiling; see [runtime runs](runtime-runs.md).

## Release acceptance

A release needs a clean package manifest, mandatory native/managed payloads,
license notices, an archive checksum and isolated execution of the extracted
artifact. Report server startup/shutdown, graphics API, UI, persistence and any
omitted checks separately. Development report counts are historical evidence and
must not be relabeled as fresh release checks.

See [release packaging](../development/release-packaging.md),
[presentation qualification](../development/presentation-integration.md),
[terrain cache](../development/terrain-streaming-cache.md) and
[current gaps](../development/feature-parity.md).
